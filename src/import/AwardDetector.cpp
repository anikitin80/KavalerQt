#include "AwardDetector.h"

#include <cwchar>

void CAwardDetector::Initialize(const std::map<long, DecreeAward>& awards)
{
	m_MedalDetector.clear();
	m_OrdenDetector.clear();
	m_PochetDetector.clear();
	m_MedalDetectorBY.clear();
	m_OrdenDetectorBY.clear();
	m_PochetDetectorBY.clear();

	for (const auto& award : awards)
	{
		std::wstring name = award.second.Name;
		name.erase(0, name.find(' ') + 1);
		name.erase(std::remove(name.begin(), name.end(), L'"'), name.end());

		std::wstring nameBY = award.second.NameBY;
		nameBY.erase(0, nameBY.find(' ') + 1);
		nameBY.erase(std::remove(nameBY.begin(), nameBY.end(), L'"'), nameBY.end());

		if (award.second.Type == DecreeAward::Orden)
		{
			m_OrdenDetector.push_back({ name, award.first });
			m_OrdenDetectorBY.push_back({ nameBY, award.first });
		}
		else if (award.second.Type == DecreeAward::Medal)
		{
			m_MedalDetector.push_back({ name, award.first });
			m_MedalDetectorBY.push_back({ nameBY, award.first });
		}
		else if (award.second.Type == DecreeAward::Pochet)
		{
			name.erase(0, name.find(' ') + 1);
			// страну отрезаем вместе с пробелом перед ней
			auto cutFrom = [](std::wstring& text, const wchar_t* token) {
				const size_t pos = text.find(token);
				if (pos != std::wstring::npos)
					text.erase(pos > 0 ? pos - 1 : 0);
			};
			cutFrom(name, L"Республики Беларусь");
			cutFrom(name, L"Беларуси");
			nameBY.erase(0, nameBY.find(' ') + 1);
			cutFrom(nameBY, L"Рэспублікі Беларусь");
			cutFrom(nameBY, L"Беларусі");
			m_PochetDetector.push_back({ name, award.first });
			m_PochetDetectorBY.push_back({ nameBY, award.first });
		}
	}

	// старые названия, только если награда есть в базе
	if (awards.count(42))
		m_PochetDetector.push_back({ L"Заслуженный работник народного образования", 42 });
	if (awards.count(37))
		m_PochetDetector.push_back({ L"Заслуженный работник культуры", 37 });
	if (awards.count(64))
		m_PochetDetectorBY.push_back({ L"Заслужаны супрацоўнік органаў унутраных спраў", 64 });
}

namespace {

// текст награды для сравнения с названиями каталога и место каждой
// оставленной буквы в строке указа. Кавычки убираются; «І» в степени
// («ІІІ степени») становится латинской «I»; неразрывный пробел — обычным.
struct AwardText
{
	std::wstring text;
	std::vector<size_t> origin;
};

AwardText normalizeAward(const std::wstring& award)
{
	AwardText out;
	out.text.reserve(award.size());
	out.origin.reserve(award.size());
	for (size_t i = 0; i < award.size(); ++i) {
		const wchar_t c = award[i];
		if (c == L'«' || c == L'»' || c == L'"')
			continue;
		out.origin.push_back(i);
		if (c == L'І')
			out.text.push_back(L'I');
		else if (c == L'\u00A0')
			out.text.push_back(L' ');
		else
			out.text.push_back(c);
	}
	return out;
}

bool atWordEnd(const std::wstring& text, size_t pos)
{
	if (pos >= text.size())
		return true;
	const wchar_t c = text[pos];
	if ((c >= L'А' && c <= L'я') || c == L'Ё' || c == L'ё' || c == L'І' || c == L'і' || c == L'Ў' || c == L'ў')
		return false;
	if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9'))
		return false;
	return true;
}

// индекс за последним символом фразы. Закрывающая кавычка стоит после
// последней буквы и в сравнение не входила.
size_t phraseEnd(const std::wstring& award, const std::vector<size_t>& origin, size_t normEnd)
{
	if (normEnd == 0 || normEnd > origin.size())
		return 0;
	size_t end = origin[normEnd - 1] + 1;
	while (end < award.size() && (award[end] == L'«' || award[end] == L'»' || award[end] == L'"'))
		++end;
	return end;
}

} // namespace

int CAwardDetector::Find(std::wstring award, bool bRussian, bool bPochetNameOnly, size_t* phraseLength) const
{
	if (phraseLength)
		*phraseLength = 0;

	const AwardText norm = normalizeAward(award);
	const std::wstring& text = norm.text;

	const Entries* detector = nullptr;
	size_t nameAt = 0;
	bool honorTitle = bPochetNameOnly;
	if (bPochetNameOnly) {
		detector = bRussian ? &m_PochetDetector : &m_PochetDetectorBY;
	} else {
		const size_t space = text.find(L' ');
		const std::wstring firstWord = text.substr(0, space);
		nameAt = space == std::wstring::npos ? text.size() : space + 1;
		if (firstWord == (bRussian ? L"медалью" : L"медалём"))
			detector = bRussian ? &m_MedalDetector : &m_MedalDetectorBY;
		else if (firstWord == (bRussian ? L"орденом" : L"ордэнам"))
			detector = bRussian ? &m_OrdenDetector : &m_OrdenDetectorBY;
		else if (firstWord == (bRussian ? L"почетное" : L"ганаровае")) {
			honorTitle = true;
			const size_t space2 = text.find(L' ', nameAt);
			nameAt = space2 == std::wstring::npos ? text.size() : space2 + 1;
			detector = bRussian ? &m_PochetDetector : &m_PochetDetectorBY;
		}
	}

	int bestId = 0;
	size_t bestNormEnd = nameAt;
	size_t bestLength = 0;
	if (detector) {
		for (const auto& item : *detector) {
			const std::wstring& key = item.first;
			if (key.size() <= bestLength || nameAt + key.size() > text.size())
				continue;
			if (text.compare(nameAt, key.size(), key) != 0)
				continue;
			bestLength = key.size();
			bestNormEnd = nameAt + key.size();
			bestId = item.second;
		}
		// опечатка в последней букве названия: «За працоўныя заслуг» (указ 687/2001)
		if (bestId == 0) {
			for (const auto& item : *detector) {
				const std::wstring& key = item.first;
				const size_t length = key.size() - 1;
				if (key.size() < 12 || length <= bestLength || nameAt + length > text.size())
					continue;
				if (text.compare(nameAt, length, key, 0, length) != 0 || !atWordEnd(text, nameAt + length))
					continue;
				bestLength = length;
				bestNormEnd = nameAt + length;
				bestId = item.second;
			}
		}
	}
	if (bestId == 0)
		return 0;

	// почётные звания в базе без «Республики Беларусь» / «Беларуси»:
	// в указе суффикс есть, и он часть фразы награды
	if (honorTitle) {
		static const wchar_t* suffixes[] = {
			L" Республики Беларусь", L" Рэспублікі Беларусь", L" Беларуси", L" Беларусі"
		};
		size_t extra = 0;
		for (const wchar_t* suffix : suffixes) {
			const size_t len = std::wcslen(suffix);
			if (len <= extra || bestNormEnd + len > text.size())
				continue;
			if (text.compare(bestNormEnd, len, suffix) == 0 && atWordEnd(text, bestNormEnd + len))
				extra = len;
		}
		bestNormEnd += extra;
	}

	if (phraseLength)
		*phraseLength = phraseEnd(award, norm.origin, bestNormEnd);
	return bestId;
}
