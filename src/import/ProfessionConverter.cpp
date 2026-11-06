#include "ProfessionConverter.h"
#include "TextUtil.h"

#include <QFile>
#include <QSettings>
#include <QTextStream>
#include <algorithm>
#include <cwchar>

RulesList CProfessionConverter::RulesAdjectives = {
	{ SexAny, LangAny, L"ему", L"его", L"---ий"},
	{ SexAny, LangAny, L"ому", L"ого", L"---ый"},
	{ SexAny, LangAny, L"ой", L"ую", L"--ая"},
	{ SexAny, LangAny, L"ей", L"юю", L"--яя"},
	// one ending per rule: this list is not expanded like the name rules
	{ SexAny, LangBY, L"каму", L"кага", L"---і"},
	{ SexAny, LangBY, L"кому", L"кога", L"---і"},
	{ SexAny, LangBY, L"аму", L"ага", L"---ы"},
	{ SexAny, LangBY, L"ому", L"ога", L"---ы"},	// былога
	// feminine: positions are converted with SexAny, so these must not be SexFemale
	// (masculine dative is "-аму", "-ай"/"-яй" is always feminine)
	{ SexAny, LangBY, L"ай", L"ую", L"--ая"},
	{ SexAny, LangBY, L"яй", L"юю", L"--яя"},
	{ SexFemale, LangBY, L"ы", L"", L"-а"},
};

RulesList CProfessionConverter::RulesProfession = {
	{ SexAny, LangAny, L"у", L"а", L"-"},
	{ SexAny, LangAny, L"е", L"у", L"-а"},
	{ SexAny, LangAny, L"ю", L"я", L"-ь"}
};

std::vector<RankInfo> CProfessionConverter::NonMilitatyRanksBY = {
	{ L"прафесар", L"прафесару", L"прафесара"}
};

namespace {

// nominative of an adjective ("вядучы", "дзяжурны", "старшая", "главный")
bool IsAdjective(const std::wstring& word)
{
	for (const wchar_t* ending : { L"ы", L"і", L"ый", L"ий", L"ой", L"ая", L"яя" })
	{
		const size_t length = std::wcslen(ending);
		if (word.length() > length + 2 && word.compare(word.length() - length, length, ending) == 0)
			return true;
	}
	return false;
}

} // namespace

CProfessionConverter::CProfessionConverter()
{

}

void CProfessionConverter::Initialize()
{
	QSettings settings;
	SetDictionaryPath(settings.value(QStringLiteral("DictionaryRU")).toString().toStdWString(), LangRU);
	SetDictionaryPath(settings.value(QStringLiteral("DictionaryBY")).toString().toStdWString(), LangBY);
}

const std::wstring& CProfessionConverter::DictionaryPath(LangType lang) const
{
	return lang == LangRU ? m_FilePathRU : m_FilePathBY;
}

bool CProfessionConverter::SetDictionaryPath(const std::wstring& filePath, LangType lang)
{
	Dictionary& dictionaryAcc = (lang == LangRU ? m_DictionaryAccRU : m_DictionaryAccBY);
	Dictionary& dictionaryDat = (lang == LangRU ? m_DictionaryDatRU : m_DictionaryDatBY);
	dictionaryAcc.clear();
	dictionaryDat.clear();
	(lang == LangRU ? m_FilePathRU : m_FilePathBY) = filePath;
	if (filePath.empty())
		return false;
	// a missing file is fine: it will be created on the first save
	if (!QFile::exists(QString::fromStdWString(filePath)))
		return true;
	return LoadDictionary(filePath, lang);
}

bool CProfessionConverter::LoadDictionary(const std::wstring& filePath, LangType lang)
{
	QFile file(QString::fromStdWString(filePath));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	Dictionary* dictionaryAcc = (lang == LangRU ? &m_DictionaryAccRU : &m_DictionaryAccBY);
	Dictionary* dictionaryDat = (lang == LangRU ? &m_DictionaryDatRU : &m_DictionaryDatBY);

	QTextStream input(&file);
	input.setEncoding(QStringConverter::Utf8);
	while (!input.atEnd()) {
		QString line = input.readLine();
		// Belarusian words were often typed with Latin "i" ("мацi"): keep one form
		if (lang == LangBY)
			line.replace(QLatin1Char('i'), QChar(0x0456)).replace(QLatin1Char('I'), QChar(0x0406));
		const int pos1 = line.indexOf(QLatin1Char(';'));
		const int pos2 = line.indexOf(QLatin1Char(';'), pos1 + 1);
		// empty words are skipped: the converter keeps the first letter of the
		// replacement and an empty one would crash it
		const auto add = [](Dictionary* dictionary, const QString& form, const QString& nominative) {
			if (!form.isEmpty() && !nominative.isEmpty())
				(*dictionary)[form.toStdWString()] = nominative.toStdWString();
		};
		if (pos2 != -1) {
			add(dictionaryDat, line.mid(pos1 + 1, pos2 - pos1 - 1), line.left(pos1));
			add(dictionaryAcc, line.mid(pos2 + 1), line.left(pos1));
		} else if (pos1 != -1) {
			add(dictionaryAcc, line.left(pos1), line.mid(pos1 + 1));
		}
	}
	return true;
}

bool CProfessionConverter::SaveDictionary(const std::wstring& filePath, LangType lang)
{
	if (filePath.empty())
		return false;
	QFile file(QString::fromStdWString(filePath));
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return false;

	std::map<std::wstring, std::pair<std::wstring, std::wstring>> dictionary;
	Dictionary* dictionaryAcc = (lang == LangRU ? &m_DictionaryAccRU : &m_DictionaryAccBY);
	Dictionary* dictionaryDat = (lang == LangRU ? &m_DictionaryDatRU : &m_DictionaryDatBY);

	for (auto iter = dictionaryAcc->begin(); iter != dictionaryAcc->end(); ++iter)
		dictionary[iter->second].second = iter->first;
	for (auto iter = dictionaryDat->begin(); iter != dictionaryDat->end(); ++iter)
		dictionary[iter->second].first = iter->first;

	QTextStream output(&file);
	output.setEncoding(QStringConverter::Utf8);
	output.setGenerateByteOrderMark(true);
	for (const auto& item : dictionary)
		output << QString::fromStdWString(item.first) << ';'
		       << QString::fromStdWString(item.second.first) << ';'
		       << QString::fromStdWString(item.second.second) << '\n';
	return true;
}

bool CProfessionConverter::UpdateDictionary(std::vector<NewDictItem>& newItems)
{
	bool bSaveBY = false, bSaveRU = false;
	for (const auto& item : newItems)
	{
		if (!item.Checked)
			continue;

		if (item.Language == LangBY)
		{
			std::wstring original = item.Original, word = item.Word;
			for (std::wstring* text : { &original, &word })
			{
				std::replace(text->begin(), text->end(), L'i', L'і');
				std::replace(text->begin(), text->end(), L'I', L'І');
			}
			if(item.Case == Dative)
				m_DictionaryDatBY[original] = word;
			else
				m_DictionaryAccBY[original] = word;
			bSaveBY = true;
		}
		else if (item.Language == LangRU)
		{
			if (item.Case == Dative)
				m_DictionaryDatRU[item.Original] = item.Word;
			else
				m_DictionaryAccRU[item.Original] = item.Word;
			bSaveRU = true;
		}
	}

	// the in-memory dictionaries are already updated; report whether saving succeeded
	bool bRes = true;
	if (bSaveBY && !SaveDictionary(m_FilePathBY, LangBY))
		bRes = false;
	if (bSaveRU && !SaveDictionary(m_FilePathRU, LangRU))
		bRes = false;
	return bRes;
}

std::wstring CProfessionConverter::GetNextWord(std::wstring& strSource, size_t& posStart)
{
	posStart = strSource.find_first_not_of(L" ,-–", posStart);
	if (posStart == -1)
		return L"";

	size_t posEnd = strSource.find_first_of(L" ,-", posStart);
	if (posEnd == -1)
		posEnd = strSource.length();

	std::wstring word;
	if(posEnd > posStart)
		word = strSource.substr(posStart, posEnd - posStart);
	return word;
}

bool CProfessionConverter::ConvertProfession(std::wstring& strSource, LangType lang, NounCase nounCase)
{
	bool bResult = false;
	const Dictionary& dictionary = (lang == LangRU ?
		(nounCase == Dative? m_DictionaryDatRU : m_DictionaryAccRU) :
		(nounCase == Dative? m_DictionaryDatBY : m_DictionaryAccBY) );
	// the dictionary has Cyrillic "і" only (LoadDictionary), the decree may have Latin "i"
	const auto lookup = [&](std::wstring word) {
		if (lang == LangBY)
		{
			std::replace(word.begin(), word.end(), L'i', L'і');
			std::replace(word.begin(), word.end(), L'I', L'І');
		}
		return dictionary.find(word);
	};

	// check non-military rank
	size_t posComma = strSource.rfind(L',');
	if (posComma != -1)
	{
		size_t posStart = strSource.find_first_not_of(L" ,", posComma);
		if (posStart != -1)
		{
			std::wstring strCheck = strSource.substr(posStart);
			for (auto& rank : NonMilitatyRanksBY)
			{
				const std::wstring& rankOriginal = (nounCase == Dative ? rank.Dative : rank.Accusative);
				if (rankOriginal.length() > strCheck.length())
					continue;

				if (!wcsncmp(strCheck.c_str(), rankOriginal.c_str(), rankOriginal.length()))
				{
					strSource.erase(posStart, rankOriginal.length());
					strSource.insert(posStart, rank.Nominative);
					break;
				}
			}
		}
	}

	size_t posStart = 0, posMainWord = 0;
	const int maxProfessionWords = 5;

	for (int i = 0; i < maxProfessionWords && posStart < strSource.length(); i++)
	{
		std::wstring word = ToLower(GetNextWord(strSource, posStart));
		if (word.empty())
			break;

		// "исполняющего обязанности начальника": the profession is the adjective
		// before it, the rest is genitive (converted with the rules below)
		if (i > 0 && (word == L"обязанности" || word == L"абавязкі" || word == L"абавязкi"))
		{
			bResult = true;
			posMainWord = posStart;
			break;
		}

		auto iter = lookup(word);
		if (iter != dictionary.end())
		{
			bResult = true;
			posMainWord = posStart; // the adjectives before it are converted below

			// an adjective from the dictionary is followed by the profession, convert
			// the whole chain: "дзяжурнага памочніка", "вядучага навуковага
			// супрацоўніка", "вядучага інжынера-канструктара"
			for (;;)
			{
				const std::wstring nominative = iter->second;
				strSource.erase(posStart + 1, iter->first.length() - 1); // leave first letter (may be capital)
				strSource.insert(posStart + 1, nominative.substr(1));

				// the other parts: "урача-анестэзіёлага-рэаніматолага"
				posStart += nominative.length();
				while (posStart < strSource.length() && strSource[posStart] == L'-')
				{
					word = ToLower(GetNextWord(strSource, posStart));
					if (word.empty())
						break;
					size_t partLength = word.length();
					iter = lookup(word);
					if (iter != dictionary.end())
					{
						strSource.erase(posStart + 1, iter->first.length() - 1); // leave first letter (may be capital)
						strSource.insert(posStart + 1, iter->second.substr(1));
						partLength = iter->second.length();
					}
					else
					{
						int nWordLen = word.length();
						if (AddNewWord(word, lang, nounCase))
						{
							strSource.erase(posStart + 1, nWordLen-1);
							strSource.insert(posStart + 1, word.substr(1));
							partLength = word.length();
						}
					}
					posStart += partLength;
				}

				if (!IsAdjective(nominative) || posStart >= strSource.length() || strSource[posStart] != L' ')
					break;
				size_t posNext = posStart;
				const std::wstring next = ToLower(GetNextWord(strSource, posNext));
				if (next.empty() || posNext != posStart + 1)
					break;
				const auto nextIter = lookup(next);
				// not "следчага следчага аддзялення": the second one belongs to "аддзялення"
				if (nextIter != dictionary.end() && nextIter->second == nominative)
					break;
				if (nextIter == dictionary.end())
				{
					// one adjective that is not in the dictionary, then the profession:
					// "вядучага ветэрынарнага ўрача" (but not "следчага 2-га следчага")
					std::wstring adjective = strSource.substr(posNext, next.length());
					const size_t posNoun = posNext + next.length() + 1;
					if (posNoun < strSource.length() && strSource[posNoun - 1] == L' '
						&& CNameConverter::ApplyRules(adjective, RulesAdjectives, SexAny, lang, nounCase, false))
					{
						size_t posWord = posNoun;
						const std::wstring noun = ToLower(GetNextWord(strSource, posWord));
						const auto nounIter = (noun.empty() || posWord != posNoun) ? dictionary.end() : lookup(noun);
						if (nounIter != dictionary.end() && !IsAdjective(nounIter->second))
						{
							strSource.replace(posNext, next.length(), adjective);
							posStart = posNoun + adjective.length() - next.length();
							iter = lookup(ToLower(GetNextWord(strSource, posStart)));
							continue;
						}
					}
					break;
				}
				posStart = posNext;
				iter = nextIter;
			}

			// the profession in brackets after it: "артыст-вакаліст (саліста)",
			// "артыст балета (салісту)", "начальнік упраўлення (статс-сакратара)"
			const size_t posOpen = strSource.find(L'(', posStart);
			if (posOpen != std::wstring::npos)
			{
				const std::wstring between = strSource.substr(posStart, posOpen - posStart);
				const size_t posClose = strSource.find(L')', posOpen);
				if (posClose != std::wstring::npos && between.find_first_of(L",–«»;") == std::wstring::npos
					&& std::count(between.begin(), between.end(), L' ') <= 3)
				{
					// each part of "статс-сакратара" separately
					// ("урача-тэрапеўта": a part after a converted one is converted by the rules)
					// only the profession itself: "(загадчыка аддзялення)" -> "(загадчык
					// аддзялення)", "(галоўнага ўрача)" -> "(галоўны ўрач)", the rest is genitive
					size_t posPart = posOpen + 1;
					bool converted = false;
					for (;;)
					{
						const size_t posCloseNow = strSource.find(L')', posOpen);
						if (posPart >= posCloseNow)
							break;
						const size_t posPartEnd = std::min(strSource.find_first_of(L"- )", posPart), posCloseNow);
						std::wstring part = ToLower(strSource.substr(posPart, posPartEnd - posPart));
						const size_t originalLength = part.length();
						const bool afterHyphen = strSource[posPart - 1] == L'-';
						if (part.empty() || (converted && !afterHyphen))
							break;
						const auto partIter = lookup(part);
						size_t partLength = originalLength;
						if (partIter != dictionary.end())
						{
							strSource.erase(posPart + 1, originalLength - 1);
							strSource.insert(posPart + 1, partIter->second.substr(1));
							partLength = partIter->second.length();
							converted = true;
						}
						else if (converted && afterHyphen && AddNewWord(part, lang, nounCase))
						{
							strSource.erase(posPart + 1, originalLength - 1);
							strSource.insert(posPart + 1, part.substr(1));
							partLength = part.length();
						}
						else if (!converted && !afterHyphen && posPartEnd < posCloseNow && strSource[posPartEnd] == L' '
							&& CNameConverter::ApplyRules(part, RulesAdjectives, SexAny, lang, nounCase, false))
						{
							// an adjective before the profession
							const size_t posNext = posPartEnd + 1;
							const size_t posNextEnd = std::min(strSource.find_first_of(L"- )", posNext), posCloseNow);
							if (lookup(ToLower(strSource.substr(posNext, posNextEnd - posNext))) == dictionary.end())
								break;
							strSource.replace(posPart, originalLength, part);
							partLength = part.length();
						}
						else if (!converted && posPartEnd < posCloseNow && strSource[posPartEnd] == L'-')
						{
							// "статс-сакратара": only the last part is in the dictionary
						}
						else
							break;
						posPart += partLength + 1;
					}
					posStart = strSource.find(L')', posOpen) + 1;
				}
			}

			// a second profession after a comma: "саліст оперы, вядучага майстра
			// сцэны", "кампазітар, члена Саюза кампазітараў" (not inside quotes)
			for (size_t posComma = strSource.find(L',', posStart); posComma != std::wstring::npos;
				 posComma = strSource.find(L',', posComma + 1))
			{
				const size_t posQuote = strSource.find_first_of(L"«»", posComma);
				if (posQuote != std::wstring::npos && strSource[posQuote] == L'»')
					continue;
				// the profession may follow one or two adjectives: ", галоўнага навуковага супрацоўніка"
				bool profession = false;
				size_t posWord = posComma + 1;
				for (int w = 0; w < 3 && !profession; ++w)
				{
					std::wstring next = ToLower(GetNextWord(strSource, posWord));
					if (next.empty())
						break;
					posWord += next.length();
					// not "механічнай, хіміка-тэрмічнай апрацоўкі": a compound adjective
					bool compoundAdjective = false;
					if (posWord < strSource.length() && strSource[posWord] == L'-')
					{
						size_t posPart = posWord;
						const std::wstring part = ToLower(GetNextWord(strSource, posPart));
						for (const wchar_t* ending : { L"ай", L"ой", L"ых", L"іх", L"их", L"ым", L"ае", L"ое", L"ыя", L"ые" })
						{
							const size_t length = std::wcslen(ending);
							if (part.length() > length + 2 && part.compare(part.length() - length, length, ending) == 0)
								compoundAdjective = true;
						}
					}
					if (lookup(next) != dictionary.end() && !compoundAdjective)
						profession = true;
					else if (!CNameConverter::ApplyRules(next, RulesAdjectives, SexAny, lang, nounCase, false))
						break;
				}
				if (!profession)
					continue;
				std::wstring rest = strSource.substr(posComma + 1);
				ConvertProfession(rest, lang, nounCase);
				strSource.replace(posComma + 1, std::wstring::npos, rest);
				break;
			}
			break;
		}

		posStart += word.length() + 1;
	}

	if (!bResult)
		FindNewWords(strSource, lang, nounCase);

	if (posMainWord < 5)
		return bResult;

	// convert adjectives before the main word
	posStart = 0;
	while (posStart < posMainWord)
	{
		std::wstring word = GetNextWord(strSource, posStart);
		if (word.empty())
			break;

		size_t nWordLen = word.length();
		if (CNameConverter::ApplyRules(word, RulesAdjectives, SexAny, lang, nounCase, false))
		{
			strSource.erase(posStart, nWordLen);
			strSource.insert(posStart, word);
		}
		posStart += word.length() + 1;
	}

	return bResult;
}

void CProfessionConverter::FindNewWords(std::wstring& strSource, LangType lang, NounCase nounCase)
{
	if (strSource.empty())
		return;

	// noun endings by case
	std::vector<std::wstring> strEndings;
	if (nounCase == Dative)
		strEndings = { L"му",L"ай", L"яй" };
	else
		strEndings = { L"га", L"го" ,L"ую", L"юю" };

	// try to check the first word - it could be new profession and we can add it to the dictionary
	size_t posStart = 0;
	std::wstring word = GetNextWord(strSource, posStart);
    if (word.length() <= 2)
        return;

	std::wstring strEnd = word.substr(word.length() - 2);
	if (std::find(strEndings.begin(), strEndings.end(), strEnd) != strEndings.end())
	{
		posStart += word.length() + 1;
		if (posStart >= strSource.length())
			return;
		word = GetNextWord(strSource, posStart);
	}

	AddNewWord(word, lang, nounCase);
}

bool CProfessionConverter::AddNewWord(std::wstring& word, LangType lang, NounCase nounCase)
{
	// dictionary keys are lower-case (lookup lowers the word), callers keep the
	// original first letter when substituting
	word = ToLower(word);

	NewDictItem item;
	item.Original = word;
	item.Language = lang;
	item.Case = nounCase;
	item.Checked = true;
	// a one-letter word would become empty ("у" -> ""): not a new word
	if (CNameConverter::ApplyRules(word, RulesProfession, SexAny, lang, nounCase, false) && !word.empty())
	{
		for (size_t i = 0; i < m_NewWords.size(); i++)
		{
			if (m_NewWords[i].Original == item.Original)	// already exists
				return true;
		}

		item.Word = word;
		m_NewWords.push_back(item);
		return true;
	}

	return false;
}

