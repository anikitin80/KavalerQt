#include "DocumentParser.h"
#include "TextUtil.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>

const std::vector<RankInfo> CDocumentParser::MilitatyRanksRU = {
	{ L"рядовой", L"рядовому", L"рядового" },
	{ L"прапорщик", L"прапорщику", L"прапорщика"},
	{ L"старший прапорщик", L"старшему прапорщику", L"старшего прапорщика"},
	{ L"младший сержант", L"младшему сержанту", L"младшего сержанта" },
	{ L"старший сержант", L"старшему сержанту", L"старшего сержанта" },
	{ L"сержант", L"сержанту", L"сержанта" },
	{ L"старшина", L"старшине", L"старшину" },
	{ L"младший лейтенант", L"младшему лейтенанту", L"младшего лейтенанта" },
	{ L"старший лейтенант", L"старшему лейтенанту", L"старшего лейтенанта" },
	{ L"лейтенант", L"лейтенанту", L"лейтенанта" },
	{ L"капитан", L"капитану", L"капитана" },
	{ L"майор", L"майору", L"майора" },
	{ L"подполковник", L"подполковнику", L"подполковника" },
	{ L"полковник", L"полковнику", L"полковника" },
	{ L"генерал-майор", L"генерал-майору", L"генерал-майора" },
	{ L"генерал-лейтенант", L"генерал-лейтенанту", L"генерал-лейтенанта" },
	{ L"генерал-полковник", L"генерал-полковнику", L"генерал-полковника" },
	{ L"ефрейтор", L"ефрейтору", L"ефрейтора" },
	{ L"мичман", L"мичману", L"мичмана" },
	{ L"старший мичман", L"старшему мичману", L"старшего мичмана" }
};

const std::vector<RankInfo> CDocumentParser::MilitatyRanksBY = {
	{ L"радавы", L"радавому", L"радавога"},
	{ L"яфрэйтар", L"яфрэйтару", L"яфрэйтара"},
	{ L"прапаршчык", L"прапаршчыку", L"прапаршчыка"},
	{ L"мічман", L"мічману", L"мічмана"},
	{ L"старшы мічман", L"старшаму мічману", L"старшага мічмана"},
	{ L"старшы прапаршчык", L"старшаму прапаршчыку", L"старшага прапаршчыка"},
	{ L"малодшы сяржант", L"малодшаму сяржанту", L"малодшага сяржанта"},
	{ L"старшы сяржант", L"старшаму сяржанту", L"старшага сяржанта" },
	{ L"сяржант", L"сяржанту", L"сяржанта" },
	{ L"старшына", L"старшыне", L"старшыну" },
	{ L"малодшы лейтэнант", L"малодшаму лейтэнанту", L"малодшага лейтэнанта" },
	{ L"старшы лейтэнант", L"старшаму лейтэнанту", L"старшага лейтэнанта" },
	{ L"лейтэнант", L"лейтэнанту", L"лейтэнанта" },
	{ L"капітан", L"капітану", L"капітана" },
	{ L"маёр", L"маёру", L"маёра" },
	{ L"падпалкоўнік", L"падпалкоўніку", L"падпалкоўніка" },
	{ L"палкоўнік", L"палкоўніку", L"палкоўніка" },
	{ L"генерал-маёр", L"генерал-маёру", L"генерал-маёра" },
	{ L"генерал-лейтэнант", L"генерал-лейтэнанту", L"генерал-лейтэнанта" },
	{ L"генерал-палкоўнік", L"генерал-палкоўніку", L"генерал-палкоўніка" },
	{ L"саветнік", L"саветніку", L"саветніка" },
	{ L"старшы саветнік", L"старшаму саветніку", L"старшага саветніка" },
	{ L"малодшы саветнік", L"малодшаму саветніку", L"малодшага саветніка" },
	{ L"дзяржаўны саветнік", L"дзяржаўнаму саветніку", L"дзяржаўнага саветніка" }
};

void CDocumentParser::Initialize(std::map<long, DecreeAward>& awards)
{
	m_ProfessionConverter.Initialize();

	m_Awards = awards;
	for (const auto& award : m_Awards)
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
			// cut the country suffix together with the space before it
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

	// some old names (only when the award exists in the database)
	if (m_Awards.count(42))
		m_PochetDetector.push_back({ L"Заслуженный работник народного образования", 42 });
	if (m_Awards.count(37))
		m_PochetDetector.push_back({ L"Заслуженный работник культуры", 37 });
	if (m_Awards.count(64))
		m_PochetDetectorBY.push_back({ L"Заслужаны супрацоўнік органаў унутраных спраў", 64 });

	ResetData();
}

void CDocumentParser::ResetData()
{
	m_AwardsStat.clear();
	m_Persons.clear();
	m_mapFileMonth.clear();
	m_GroupsInfo.clear();
	m_UkazInfo.Clear();
}

namespace {

// award text as it is compared with the catalog names, and where each kept
// character sits in the decree line. Quotes are dropped; "І" in a degree
// ("ІІІ степени") becomes Latin "I"; a no-break space becomes a usual one.
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

// index past the last character of the phrase. A closing quote stands after the
// last letter and was not part of the compared text.
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

int CDocumentParser::GetAwardId(std::wstring award, bool bRussian, bool bPochetNameOnly, size_t* phraseLength)
{
	if (phraseLength)
		*phraseLength = 0;

	const AwardText norm = normalizeAward(award);
	const std::wstring& text = norm.text;

	AwardDetector* detector = nullptr;
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
	}
	if (bestId == 0)
		return 0;

	// honorary titles are stored without "Республики Беларусь" / "Беларуси":
	// the decree still writes the suffix, and it belongs to the award phrase
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

NounCase CDocumentParser::GetNounCaseByAwardId(int awardId)
{
	auto iter = m_Awards.find(awardId);
	if (iter != m_Awards.end())
	{
		if (iter->second.Type == 4 || iter->second.Type == 1)
			return Dative;
	}

	return Accusative;
}

bool CDocumentParser::IsMilitaryAward(int awardId)
{
	if (awardId >= 6 && awardId <= 8 || awardId >= 20 && awardId <= 22)
		return true;

	return false;
}

namespace {

// words of a decree; Belarusian lines have Cyrillic "і" (Latin "i" is replaced first)
const std::vector<std::wstring> searchTargetsRU = { L"медалью", L"орденом", L"почетное звание", L"почетные звания" };
const std::vector<std::wstring> searchTargetsBY = { L"медалём", L"ордэнам", L"ганаровае званн", L"ганаровыя званн" };
const std::vector<std::wstring> searchFinishBY = { L"Прэзідэнт Рэспублікі Беларусь" };
const std::vector<std::wstring> searchFinishRU = { L"Президент Республики Беларусь" };
const std::vector<std::wstring> searchUkazStart = { L"УКАЗ ПР", L"УКАЗ ПР", L"УКАЗПР" };
const std::vector<std::wstring> searchActionsBY = { L"знагародзіць", L"прысвоіць", L"Прысвоіць" };
const std::vector<std::wstring> searchActionsRU = { L"наградить", L"присвоить", L"Присвоить" };
const std::vector<std::wstring> searchGroup = { L"группу", L"группы", L"работников", L"работнікаў", L"супрацоўнікаў", L"сотрудникам",
	L"сотрудников", L"ваеннаслужачых", L"военнослужащих" };
const std::vector<std::wstring> searchGroupEnd = { L"государственными", L"орденом", L"медалью", L"дзяржаўнымі", L"ордэнам", L"медалём", L"почетные", L"почетное", L"почетных" };

// Decrees of the early 1990s put "тов." in front of the name:
// "скульптору тов. Гумилевскому Льву Николаевичу". The space after the period
// is often a no-break space, so the surname stays glued to "тов." and the
// name is left as given name and patronymic.
void stripComradeTitle(std::wstring& text)
{
	const auto gap = [](wchar_t character) { return character == L' ' || character == L'\u00A0'; };
	for (size_t pos = 0; pos < text.size(); )
	{
		const bool title = (text.compare(pos, 3, L"тов") == 0 || text.compare(pos, 3, L"Тов") == 0)
			&& (pos == 0 || gap(text[pos - 1]));
		if (!title) {
			++pos;
			continue;
		}
		size_t end = pos + 3;
		if (end < text.size() && text[end] == L'.')
			++end;
		if (end < text.size() && gap(text[end])) {
			size_t name = end;
			while (name < text.size() && gap(text[name]))
				++name;
			if (name < text.size() && IsCapital(text[name])) {
				// the space before "тов." may be a no-break space; the name split looks for a usual one
				if (pos > 0 && text[pos - 1] == L'\u00A0')
					text[pos - 1] = L' ';
				text.erase(pos, name - pos);
				continue;
			}
		}
		pos = end > pos ? end : pos + 1;
	}
}

} // namespace

// "15 мая 1997 г.№ 282": the number after "№", the date before "г"
void CDocumentParser::ParseDecreeHeader(const std::wstring& strLine)
{
	size_t pos = strLine.find(L'№');
	if (pos == std::wstring::npos)
		return;
	m_UkazInfo.Number = int(std::wcstol(strLine.substr(pos + 1).c_str(), nullptr, 10));

	pos = strLine.rfind(L'г', pos - 1);
	if (pos != std::wstring::npos)
	{
		m_UkazInfo.Date = ParseDate(strLine.substr(0, pos), m_UkazInfo.RussianLang);
		m_mapFileMonth[m_FileName] = m_UkazInfo.Date.isValid() ? m_UkazInfo.Date.month() : 0;
	}
}

// a section of a list ("па Брэсцкай вобласці", "па г. Мінску"): people of a city
// section may have no place in the decree, it is the city ("г. Мінск")
void CDocumentParser::ParseSection(const std::wstring& strLine)
{
	std::wstring line = strLine;
	std::replace(line.begin(), line.end(), L' ', L' '); // "па г. Мінску"
	StrTrim(line);
	if (line.length() > 40 || line.find(L'–') != std::wstring::npos
		|| (line.compare(0, 3, L"па ") != 0 && line.compare(0, 3, L"по ") != 0))
		return;
	const std::wstring rest = line.substr(3);
	// "па г. Мінску", "па гораду Мінску", "по городу Минску", "по гор. Минску"
	std::size_t cityStart = std::wstring::npos;
	for (const wchar_t* word : { L"г. ", L"гор. ", L"гораду ", L"городу " })
	{
		const std::size_t length = std::wcslen(word);
		if (rest.compare(0, length, word) == 0)
			cityStart = length;
	}
	if (cityStart != std::wstring::npos && rest.length() > cityStart + 1
		&& rest.find(L' ', cityStart) == std::wstring::npos)
	{
		std::wstring city = rest.substr(cityStart); // dative: "Мінску", "Минску"
		if (city.back() == L'у')
			city.pop_back();
		m_SectionCity = L"г. " + city;
	}
	else if (rest.find(L"вобласці") != std::wstring::npos || rest.find(L"области") != std::wstring::npos)
		m_SectionCity.clear();
}

// info about a group: text after the group token up to the end of the sentence
// or up to the award type ("орденом", "медалью", ...)
void CDocumentParser::ParseGroupInfo(const std::wstring& strLine)
{
	size_t posStart = 0, posEnd = 0;
	if (!FindTokens(strLine, searchGroup, posStart, posEnd))
		return;
	std::wstring groupInfo = posEnd < strLine.length() ? strLine.substr(posEnd + 1) : std::wstring();
	posEnd = groupInfo.find_first_of(L":\t\n\r.");
	if (posEnd != std::wstring::npos)
		groupInfo.erase(posEnd);
	if (FindTokens(groupInfo, searchGroupEnd, posStart, posEnd) && posStart > 0)
		groupInfo.erase(posStart - 1);
	// the purpose after the organisation is not part of it: "«Мінскі трактарны
	// завод» у развіццё трактарабудавання", "органаў пракуратуры ва ўмацаванне ..."
	for (const wchar_t* preposition : { L" у ", L" ў ", L" в ", L" ва ", L" во ", L" за " })
	{
		for (size_t pos = groupInfo.find(preposition); pos != std::wstring::npos; pos = groupInfo.find(preposition, pos + 1))
		{
			// not inside quotes: the next quote after it is an opening one (or none)
			const size_t posQuote = groupInfo.find_first_of(L"«»", pos);
			if (posQuote == std::wstring::npos || groupInfo[posQuote] == L'«')
			{
				groupInfo.erase(pos);
				break;
			}
		}
	}

	AddGroupInfo(m_UkazInfo, m_FileName, groupInfo);
}

bool CDocumentParser::ProcessFile(const std::wstring& fileName, const std::vector<std::wstring>& arrLines)
{
	bool bResult = false;

	m_FileName = fileName;

	bool bUkazStarted = false, bAwardStarted = false, bAwardTypeFound = false, bPossibleGroupStart = false, bMultiplePochet = false;
	std::wstring curAward;
	int awardId = 0;
	int nAwardedCount = 0;
	size_t posStart = 0, posEnd = 0;
	// the award being read is over: count its people
	const auto finishAward = [&] {
		UpdateAwardStat(awardId, curAward, nAwardedCount);
		curAward.clear();
		awardId = nAwardedCount = 0;
	};

	for (auto itrLine = arrLines.begin(); itrLine != arrLines.end(); itrLine++)
	{
		// new ukaze of president found
		if (FindTokens(*itrLine, searchUkazStart, posStart, posEnd))
		{
			finishAward();
			bUkazStarted = true;
			bAwardStarted = bAwardTypeFound = bPossibleGroupStart = bMultiplePochet = false;
			m_UkazInfo.Clear();
			m_UkazInfo.RussianLang = ((*itrLine)[posEnd] == L'Е');
			m_SectionCity.clear();
			continue;
		}

		if (!bUkazStarted)
			continue;

		std::wstring strLine = *itrLine;
		size_t posActionEnd = 0, posTargetStart = 0;

		if (0 == m_UkazInfo.Number)
			ParseDecreeHeader(strLine);

		// replace latin 'i' with belorussian
		if (!m_UkazInfo.RussianLang)
			ToCyrillicI(strLine, false);

		// order about awarding found
		if (FindTokens(strLine, m_UkazInfo.RussianLang ? searchActionsRU : searchActionsBY, posStart, posEnd))
		{
			bAwardStarted = true;
			bAwardTypeFound = false;
			finishAward();
			posActionEnd = posEnd;
		}

		if (m_UkazInfo.Number)
			ParseGroupInfo(strLine);
		ParseSection(strLine);

		if (bAwardStarted)
		{
			// no more orders or medals further
			if (bAwardTypeFound && FindTokens(strLine, m_UkazInfo.RussianLang ? searchFinishRU : searchFinishBY, posStart, posEnd))
			{
				bUkazStarted = bAwardStarted = bAwardTypeFound = false;
				finishAward();
				continue;
			}

			// find award type
			if (FindTokens(strLine, m_UkazInfo.RussianLang ? searchTargetsRU : searchTargetsBY, posStart, posEnd))
			{
				bAwardTypeFound = true;
				posTargetStart = posStart;
				finishAward();
				curAward = strLine.substr(posTargetStart);
				curAward.erase(curAward.find_last_not_of(L" :\t\n\r.") + 1);
				// length of the phrase in the decree, not of the catalog name:
				// "медалью «За отвагу»" is longer than "Медаль "За отвагу""
				size_t awardPhrase = 0;
				awardId = GetAwardId(curAward, m_UkazInfo.RussianLang, false, &awardPhrase);

				// single awarding description
				if (posActionEnd > 0 && strLine.back() != L':')
				{
					std::wstring strAwardedInfo;
					// person and rank before the award ("...Кашкана ... орденом «...» (посмертно).")
					const bool personBeforeAward = posTargetStart - posActionEnd > 10;
					if (personBeforeAward)
						strAwardedInfo = strLine.substr(posActionEnd, posTargetStart - posActionEnd);
					else if (awardId > 0 && awardPhrase > 0 && posTargetStart + awardPhrase < strLine.length())
						strAwardedInfo = strLine.substr(posTargetStart + awardPhrase);

					if(!strAwardedInfo.empty())
					{
						const size_t addedFrom = m_Persons.size();
						ExtractPersonInfo(curAward, strAwardedInfo);
						// the mark may sit after the award title, outside strAwardedInfo
						if (personBeforeAward && awardId > 0 && awardPhrase > 0 && m_Persons.size() > addedFrom
							&& posTargetStart + awardPhrase < strLine.length())
						{
							const std::wstring tail = strLine.substr(posTargetStart + awardPhrase);
							const wchar_t* mark = m_UkazInfo.RussianLang ? L"(посмертно)" : L"(пасмяротна)";
							if (tail.find(mark) != std::wstring::npos)
							{
								for (size_t i = addedFrom; i < m_Persons.size(); ++i)
									m_Persons[i].Posthumous = true;
							}
						}
					}
				}

				if (strLine.back() == L':')
				{
					if (awardId)
						bPossibleGroupStart = true;
					else if (strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[2] : searchTargetsBY[2]) != std::wstring::npos ||
						     strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[3] : searchTargetsBY[3]) != std::wstring::npos )
					{
						bMultiplePochet = true;
					}
				}
			}

			if (bAwardTypeFound)
			{
				if (bMultiplePochet)
				{
					if (awardId <= 0)
					{
						awardId = GetAwardId(strLine.c_str(), m_UkazInfo.RussianLang, true);
						if (awardId > 0)
							continue;
					}
					else if (strLine.find(L"Заслуж") != std::wstring::npos || strLine.find(L"Народны") != std::wstring::npos)
					{
						int newAwardId = GetAwardId(strLine.c_str(), m_UkazInfo.RussianLang, true);
						if (newAwardId > 0)
						{
							UpdateAwardStat(awardId, strLine, nAwardedCount);
							awardId = newAwardId;
							nAwardedCount = 0;
							continue;
						}
					}
				}
				bResult = true;
				if (awardId > 0)
				{
					// awarding list
					size_t pos = strLine.find_first_of(L"–-", 0);
					if (pos == std::wstring::npos)
						continue;

					PersonInfo info;
					info.FileName = m_FileName;
					info.Ukaz = m_UkazInfo;
					info.AwardId = awardId;

					if(pos == 0)
					{
						// new format of persons list in the docx:
						// name on the previous line, description on the next one
						if (itrLine == arrLines.begin() || itrLine + 1 == arrLines.end())
							continue;
						nAwardedCount++;

						info.Name = *(itrLine - 1);
						std::replace(info.Name.begin(), info.Name.end(), L'\v', L' ');
						info.Description = *(++itrLine);

						NormalizePersonInfo(info);
						m_Persons.push_back(info);
					}
					else if ((bPossibleGroupStart || bMultiplePochet) && strLine[pos] == L'–')
					{
						// old format of persons list in the docx
						nAwardedCount++;

						info.Name = strLine.substr(0, pos);
						info.Description = strLine.substr(pos + 1);

						NormalizePersonInfo(info);
						m_Persons.push_back(info);
					}
				}
			}
		}
	}

	UpdateAwardStat(awardId, curAward, nAwardedCount);

	return bResult;
}

void CDocumentParser::UpdateAwardStat(int id, std::wstring& award, int nCount)
{
	if (award.empty())
		return;

	if(id <= 0)
		id = GetAwardId(award.c_str(), m_UkazInfo.RussianLang);

	bool bFound = false;
	for (AwardInfo& awardInfo : m_AwardsStat)
	{
		if (awardInfo.Id == id && (id != 0 || award == awardInfo.CustomName))
		{
			bFound = true;

			awardInfo.CountByFiles[m_FileName] += nCount ? nCount : 1; // a new file starts at 0
			awardInfo.TotalCount += nCount ? nCount : 1;
			if (0 == nCount)
			{
				awardInfo.CountByFiles[m_FileName] |= kFlagNeedReview;
				awardInfo.NeedReview = true;
			}
		}
	}

	if (!bFound)
	{
		AwardInfo awardInfo;
		awardInfo.Id = id;
		if (0 == id)
		{
			awardInfo.NeedReview = true;
			awardInfo.CustomName = award;
		}
		else
		{
			awardInfo.NeedReview = nCount ? false : true;
		}
		awardInfo.TotalCount = nCount ? nCount : 1;
		awardInfo.CountByFiles[m_FileName] = nCount ? nCount : 1;
		if (!nCount)
		{
			awardInfo.CountByFiles[m_FileName] |= kFlagNeedReview;
			awardInfo.NeedReview = true;
		}
		m_AwardsStat.push_back(awardInfo);
	}
}

void CDocumentParser::ExtractPersonInfo(std::wstring& curAward, std::wstring& strAwardedInfo)
{
	if (strAwardedInfo.empty())
		return;

	int awardId = GetAwardId(curAward, m_UkazInfo.RussianLang);
	if (awardId <= 0)
		return;

	NounCase nounCase = GetNounCaseByAwardId(awardId);

	bool bPosthumous = false;
	size_t posPost = strAwardedInfo.find(m_UkazInfo.RussianLang? L"(посмертно)" : L"(пасмяротна)");
	if (posPost != std::wstring::npos)
	{
		bPosthumous = true;
		strAwardedInfo.erase(posPost, strAwardedInfo.length() - posPost);
	}

	StrTrim(strAwardedInfo);
	stripComradeTitle(strAwardedInfo);

	// check for alias
	bool bHasAlias = false;
	size_t posBracket = strAwardedInfo.rfind(L'(');
	if (posBracket != std::wstring::npos && strAwardedInfo.rfind(L')') > strAwardedInfo.length() - 10)
		bHasAlias = true;

	// lookup name parts
	size_t pos1 = bHasAlias ? strAwardedInfo.rfind(' ', posBracket-2) : strAwardedInfo.rfind(' ');
	size_t pos2 = strAwardedInfo.rfind(' ', pos1 - 1);
	if (pos1 == std::wstring::npos || pos2 == std::wstring::npos)
		return;

	if (!IsCapital(strAwardedInfo[pos1 + 1]) || !IsCapital(strAwardedInfo[pos2 + 1]))
		return;

	size_t sep = pos2;
	// a name with nothing in front of it: "Иванова Ивана Ивановича"
	bool nameOnly = false;

	if (m_NameConverter.IsMiddleName(strAwardedInfo.substr(pos1), m_UkazInfo.RussianLang? LangRU : LangBY, nounCase))
	{
		size_t pos3 = strAwardedInfo.rfind(' ', pos2 - 1);
		if (pos3 != std::wstring::npos && IsCapital(strAwardedInfo[pos3 + 1]))
			sep = pos3;
		else if (pos3 == std::wstring::npos && IsCapital(strAwardedInfo[0]))
			nameOnly = true;
	}


	PersonInfo info;
	info.Ukaz = m_UkazInfo;
	info.FileName = m_FileName;
	info.AwardId = awardId;
	if (nameOnly) {
		info.Name = strAwardedInfo;
	} else {
		info.Name = strAwardedInfo.substr(sep + 1);
		info.Description = strAwardedInfo.substr(0, sep);
	}
	info.Posthumous = bPosthumous;

	NormalizePersonInfo(info);
	m_Persons.push_back(info);
}

bool CDocumentParser::SpltPersonInfo(PersonInfo& person)
{
	PersonInfo person2;

	person2.AwardId = person.AwardId;
	person2.FileName = person.FileName;
	person2.Ukaz = person.Ukaz;

	bool bSameProfession = false;
	NounCase nounCase = GetNounCaseByAwardId(person.AwardId);

	size_t posNextPerson = person.Description.rfind(person.Ukaz.RussianLang? L" и " : L" і ");
	if (posNextPerson == std::wstring::npos)
	{
		posNextPerson = person.Description.rfind(person.Ukaz.RussianLang ? L" и" : L" і");
		if (posNextPerson == std::wstring::npos)
			return false;

		bSameProfession = true;
	}

	std::wstring strAwardedInfo = person.Description.substr(0, posNextPerson);
	stripComradeTitle(strAwardedInfo);

	size_t pos1 = strAwardedInfo.rfind(' ');
	size_t pos2 = strAwardedInfo.rfind(' ', pos1 - 1);
	if (pos1 == std::wstring::npos || pos2 == std::wstring::npos)
		return false;

	if (!IsCapital(strAwardedInfo[pos1 + 1]) || !IsCapital(strAwardedInfo[pos2 + 1]))
		return false;

	size_t sep = pos2;
	size_t pos3 = strAwardedInfo.rfind(' ', pos2 - 1);
	if (pos3 != std::wstring::npos && IsCapital(strAwardedInfo[pos3 + 1]))
		sep = pos3;

	person2.Name = strAwardedInfo.substr(sep + 1);;
	person2.Description = strAwardedInfo.substr(0, sep);

	if (bSameProfession)
	{
		size_t posNext = person2.Description.find(L' ');
		std::wstring strFirstWord = person2.Description.substr(0, posNext);

		static const RulesList rulesPlural = { { SexAny, LangAny, L"ам", L"ов", L"--"} };
		if (CNameConverter::ApplyRules(strFirstWord, rulesPlural, SexAny, person.Ukaz.RussianLang ? LangRU : LangBY, nounCase, false))
		{
			person2.Description.erase(0, posNext);
			person2.Description.insert(0, strFirstWord);
		}

		person.Description = person2.Description;
		person.Flags |= PersonInfo::NF_PROFESSION;
		person2.Flags |= PersonInfo::NF_PROFESSION;
	}
	else
	{
		person.Description.erase(0, posNextPerson + 3);
		if (m_ProfessionConverter.ConvertProfession(person.Description, person.Ukaz.RussianLang ? LangRU : LangBY, nounCase))
			person.Flags |= PersonInfo::NF_PROFESSION;
	}

	NormalizePersonInfo(person2);
	m_Persons.push_back(person2);

	return true;
}

void CDocumentParser::NormalizePersonInfo(PersonInfo& person)
{
	StrTrim(person.Name);
	stripComradeTitle(person.Name);
	StrTrim(person.Name);
	StrTrim(person.Description);
	std::replace(person.Description.begin(), person.Description.end(), L' ', L' ');

	NounCase nounCase = GetNounCaseByAwardId(person.AwardId);

	LangType lang = person.Ukaz.RussianLang ? LangRU : LangBY;
	person.SourceName = person.Name;
	person.SourceCase = nounCase;
	if (m_NameConverter.NormalizeName(person.Name, lang, nounCase))
		person.Flags |= PersonInfo::NF_NAME;

	if (ExtractMilitaryRank(person, lang, nounCase))
		person.Flags |= PersonInfo::NF_RANK;

	// "наладчык – брыгадзір ...": the second profession after a dash, but not a
	// dash inside quotes («Дзіцячы сад – сярэдняя школа»): there the next quote
	// is a closing one. Nested quotes are closed once («НВА «Цэнтр»), so the
	// quotes before the dash are not counted.
	// every part after a dash is a profession of its own: "намесніка начальніка –
	// начальніка аддзела – урача-фтызіятра"
	std::vector<std::wstring> professions;
	std::wstring rest = person.Description;
	for (;;)
	{
		size_t posSep = rest.find(L" – ");
		while (posSep != std::wstring::npos)
		{
			const size_t posQuote = rest.find_first_of(L"«»", posSep);
			if (posQuote == std::wstring::npos || rest[posQuote] == L'«')
				break;
			posSep = rest.find(L" – ", posSep + 3);
		}
		if (posSep == std::wstring::npos)
			break;
		professions.push_back(rest.substr(0, posSep));
		rest.erase(0, posSep + 3);
	}
	professions.push_back(rest);

	for (size_t i = 0; i < professions.size(); i++)
	{
		if (m_ProfessionConverter.ConvertProfession(professions[i], lang, nounCase) && i == 0)
			person.Flags |= PersonInfo::NF_PROFESSION;
	}
	person.Description = professions[0];
	for (size_t i = 1; i < professions.size(); i++)
		person.Description += L" – " + professions[i];

	// a housewife in the city section of a list has no place in the decree:
	// "па г. Мінску ... хатнюю гаспадыню" -> "хатняя гаспадыня, г. Мінск"
	if (!m_SectionCity.empty())
	{
		const std::wstring description = ToLower(person.Description);
		if (description == L"хатняя гаспадыня" || description == L"домохозяйка")
			person.Description += L", " + m_SectionCity;
	}
}

bool CDocumentParser::ExtractMilitaryRank(PersonInfo& person, LangType lang, NounCase nounCase)
{
	size_t posPost = person.Description.find(lang == LangRU ? L"(посмертно)" : L"(пасмяротна)");
	if (posPost != std::wstring::npos)
	{
		person.Posthumous = true;
		person.Description.erase(posPost, person.Description.length() - posPost);
		StrTrim(person.Description);
	}

	size_t posComma = person.Description.rfind(L',');
	size_t posStart = 0;
	if (posComma != std::wstring::npos)
		posStart = person.Description.find_first_not_of(L" ,", posComma);

	if (posStart == std::wstring::npos || posStart >= person.Description.length())
		return false;

	std::wstring strCheck = person.Description.substr(posStart);
	ToCyrillicI(strCheck, false);

	// check longer ranks first: "старшему прапорщику" contains "прапорщику",
	// "генерал-майору" contains "майору" etc. (sorted once per language and case)
	const auto sortedRanks = [](const std::vector<RankInfo>& allRanks, NounCase nounCase) {
		std::vector<const RankInfo*> ranks;
		ranks.reserve(allRanks.size());
		for (const auto& rank : allRanks)
			ranks.push_back(&rank);
		std::stable_sort(ranks.begin(), ranks.end(), [nounCase](const RankInfo* a, const RankInfo* b) {
			const size_t lenA = (nounCase == Dative ? a->Dative : a->Accusative).length();
			const size_t lenB = (nounCase == Dative ? b->Dative : b->Accusative).length();
			return lenA > lenB;
		});
		return ranks;
	};
	static const std::vector<const RankInfo*> ranksByDative[2] = {
		sortedRanks(MilitatyRanksRU, Dative), sortedRanks(MilitatyRanksBY, Dative) };
	static const std::vector<const RankInfo*> ranksByAccusative[2] = {
		sortedRanks(MilitatyRanksRU, Accusative), sortedRanks(MilitatyRanksBY, Accusative) };
	const auto& ranks = (nounCase == Dative ? ranksByDative : ranksByAccusative)[lang == LangBY ? 1 : 0];

	for (const RankInfo* pRank : ranks)
	{
		const RankInfo& rank = *pRank;
		const std::wstring& rankOriginal = (nounCase == Dative ? rank.Dative : rank.Accusative);
		if (rankOriginal.length() > strCheck.length())
			continue;

		// a rank is followed by a few words at most ("міліцыі", "унутранай службы ў адстаўцы",
		// "медыцынскай службы запасу"): "капітана-механіка", "капітана – зменнага
		// механіка цеплахода", "капітана нацыянальнай каманды па тэнісу" are positions
		{
			const size_t posRank = strCheck.find(rankOriginal);
			if (posRank != std::wstring::npos)
			{
				const std::wstring rest = strCheck.substr(posRank + rankOriginal.length());
				if (!rest.empty() && (rest[0] == L'-' || rest.compare(0, 2, L" –") == 0
					|| std::count(rest.begin(), rest.end(), L' ') > 5))
					continue;
			}
		}

		// "саветнік" is a rank only as "саветнік юстыцыі" or with a class
		// ("дзяржаўны саветнік ... 3 класа"); "саветнік упраўлення ..." is a position
		if (rank.Nominative.find(L"саветнік") != std::wstring::npos)
		{
			const size_t posRank = strCheck.find(rankOriginal);
			if (posRank == std::wstring::npos)
				continue;
			const std::wstring rest = strCheck.substr(posRank + rankOriginal.length());
			if (rest.compare(0, 8, L" юстыцыі") != 0 && rest.find(L"класа") == std::wstring::npos)
				continue;
		}

		if (posComma != std::wstring::npos)
		{
			if (strCheck.compare(0, rankOriginal.length(), rankOriginal) == 0)
			{
				person.MilitaryRank = rank.Nominative + strCheck.substr(rankOriginal.length());
				StrTrim(person.MilitaryRank);
				person.Description.erase(posComma, person.Description.length() - posComma);
				return true;
			}
		}
		else
		{
			posStart = strCheck.find(rankOriginal.c_str());
			if (posStart != std::wstring::npos)
			{
				person.MilitaryRank = rank.Nominative + strCheck.substr(posStart + rankOriginal.length());
				StrTrim(person.MilitaryRank);
				person.Description.erase(posStart, person.Description.length() - posStart);
				StrTrim(person.Description);
				return true;
			}
		}
	}

	return false;
}

std::wstring CDocumentParser::GetPersonDescription(size_t nIndex)
{
	if (nIndex >= m_Persons.size())
		return L"";

	return GetPersonDescription(m_Persons[nIndex], true);
}

std::wstring CDocumentParser::GetPersonDescription(PersonInfo& person, bool bGetRank)
{
	std::wstring strDesc = person.Description.c_str();
	std::wstring strGroupInfo;
	if (GetGroupInfo(person.Ukaz, strGroupInfo))
	{
		StrTrim(strGroupInfo);
		// not twice: "дырэктар ААТ «Белкард»" already names the organisation
		if (!strGroupInfo.empty() && strDesc.find(strGroupInfo) == std::wstring::npos)
		{
			if(!strDesc.empty())
				strDesc += L' ';
			strDesc += strGroupInfo;
		}
	}

	if (!person.MilitaryRank.empty() && bGetRank)
	{
		if (!strDesc.empty())
			strDesc += L", ";
		strDesc += person.MilitaryRank;
	}

	return strDesc;
}

// decree numbers restart every year, so a decree is identified by number and date
void CDocumentParser::AddGroupInfo(const UkazInfo& ukaz, const std::wstring& fileName, const std::wstring& groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->UkazNumber == ukaz.Number && iter->UkazDate == ukaz.Date)
		{
			iter->Info = groupInfo;
			return;
		}
	}

	GroupInfo info;
	info.FileName = fileName;
	info.UkazNumber = ukaz.Number;
	info.UkazDate = ukaz.Date;
	info.Info = groupInfo;
	info.Checked = false;
	m_GroupsInfo.push_back(info);
}

bool CDocumentParser::GetGroupInfo(const UkazInfo& ukaz, std::wstring& groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->UkazNumber == ukaz.Number && iter->UkazDate == ukaz.Date)
		{
			groupInfo = iter->Info;
			return iter->Checked;
		}
	}

	return false;
}

void  CDocumentParser::CheckGroupInfo(const wchar_t* fileName, bool bCheck)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->FileName == fileName)
		{
			iter->Checked = bCheck;
			break;
		}
	}
}

void  CDocumentParser::UpdateGroupInfo(const wchar_t* fileName, const wchar_t* groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->FileName == fileName)
		{
			iter->Info = groupInfo;
			break;
		}
	}
}

bool CDocumentParser::GetGroupInfo(std::wstring& fileName, GroupInfo& groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->FileName == fileName)
		{
			groupInfo = *iter;
			return true;
		}
	}

	return false;
}