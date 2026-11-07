#include "ProfessionConverter.h"
#include "TextUtil.h"

#include <QFile>
#include <QSettings>
#include <QTextStream>
#include <algorithm>
#include <cwchar>

// positions are converted with SexAny: a SexFemale or SexMale rule here would never match
const RulesList CProfessionConverter::RulesAdjectives = CNameConverter::ExpandRules({
	{ SexAny, LangAny, L"ему", L"его", L"---ий"},
	{ SexAny, LangAny, L"ому", L"ого", L"---ый"},
	{ SexAny, LangAny, L"ой", L"ую", L"--ая"},
	{ SexAny, LangAny, L"ей", L"юю", L"--яя"},
	{ SexAny, LangBY, L"каму,кому", L"кага,кога", L"---і"},
	{ SexAny, LangBY, L"аму,ому", L"ага,ога", L"---ы"},	// былога
	// feminine (masculine dative is "-аму", "-ай"/"-яй" is always feminine)
	{ SexAny, LangBY, L"ай,яй", L"ую,юю", L"--ая"},
});

const RulesList CProfessionConverter::RulesProfession = CNameConverter::ExpandRules({
	{ SexAny, LangAny, L"у", L"а", L"-"},
	{ SexAny, LangAny, L"е", L"у", L"-а"},
	{ SexAny, LangAny, L"ю", L"я", L"-ь"}
});

const std::vector<RankInfo> CProfessionConverter::NonMilitatyRanksBY = {
	{ L"прафесар", L"прафесару", L"прафесара"}
};

namespace {

// the word ends with one of the endings and is longer than it by at least three letters
bool EndsWithAny(const std::wstring& word, std::initializer_list<const wchar_t*> endings)
{
	for (const wchar_t* ending : endings)
	{
		const size_t length = std::wcslen(ending);
		if (word.length() > length + 2 && word.compare(word.length() - length, length, ending) == 0)
			return true;
	}
	return false;
}

// nominative of an adjective ("вядучы", "дзяжурны", "старшая", "главный")
bool IsAdjective(const std::wstring& word)
{
	return EndsWithAny(word, { L"ы", L"і", L"ый", L"ий", L"ой", L"ая", L"яя" });
}

// the form Lookup searches for: lower case, and Belarusian "і" rather than Latin "i"
std::wstring dictionaryKey(std::wstring word, LangType lang)
{
	if (lang == LangBY)
		ToCyrillicI(word, true);
	return ToLower(word);
}

} // namespace

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
	(lang == LangRU ? m_NominativesRU : m_NominativesBY).clear();
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
		const QString line = input.readLine();
		const int pos1 = line.indexOf(QLatin1Char(';'));
		const int pos2 = line.indexOf(QLatin1Char(';'), pos1 + 1);
		// empty words are skipped: the converter keeps the first letter of the
		// replacement and an empty one would crash it. The key and the nominative
		// are stored the way Lookup searches, so "Мацi" and "маці" are one word
		const auto add = [lang](Dictionary* dictionary, const QString& form, const QString& nominative) {
			const std::wstring key = dictionaryKey(form.toStdWString(), lang);
			const std::wstring value = dictionaryKey(nominative.toStdWString(), lang);
			if (!key.empty() && !value.empty())
				(*dictionary)[key] = value;
		};
		if (pos2 != -1) {
			add(dictionaryDat, line.mid(pos1 + 1, pos2 - pos1 - 1), line.left(pos1));
			add(dictionaryAcc, line.mid(pos2 + 1), line.left(pos1));
		} else if (pos1 != -1) {
			add(dictionaryAcc, line.left(pos1), line.mid(pos1 + 1));
		}
	}
	rebuildNominatives(lang);
	return true;
}

void CProfessionConverter::rememberNominative(LangType lang, std::wstring nominative)
{
	nominative = dictionaryKey(std::move(nominative), lang);
	if (!nominative.empty())
		(lang == LangRU ? m_NominativesRU : m_NominativesBY).insert(std::move(nominative));
}

void CProfessionConverter::rebuildNominatives(LangType lang)
{
	(lang == LangRU ? m_NominativesRU : m_NominativesBY).clear();
	const Dictionary* maps[] = {
		lang == LangRU ? &m_DictionaryDatRU : &m_DictionaryDatBY,
		lang == LangRU ? &m_DictionaryAccRU : &m_DictionaryAccBY
	};
	for (const Dictionary* dictionary : maps) {
		for (const auto& item : *dictionary)
			rememberNominative(lang, item.second);
	}
}

bool CProfessionConverter::nominativeFromRules(std::wstring word, LangType lang, NounCase nounCase, std::wstring& nominative) const
{
	if (!CNameConverter::ApplyRules(word, RulesProfession, SexAny, lang, nounCase, false) || word.empty())
		return false;
	word = ToLower(word);
	const auto& known = lang == LangRU ? m_NominativesRU : m_NominativesBY;
	if (known.find(word) == known.end())
		return false;
	nominative = word;
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
				ToCyrillicI(*text, true);
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
	if (bSaveBY)
		rebuildNominatives(LangBY);
	if (bSaveRU)
		rebuildNominatives(LangRU);
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
	if (posStart == std::wstring::npos)
		return L"";

	size_t posEnd = strSource.find_first_of(L" ,-", posStart);
	if (posEnd == std::wstring::npos)
		posEnd = strSource.length();

	std::wstring word;
	if(posEnd > posStart)
		word = strSource.substr(posStart, posEnd - posStart);
	return word;
}

const Dictionary& CProfessionConverter::DictionaryFor(LangType lang, NounCase nounCase) const
{
	return lang == LangRU ? (nounCase == Dative ? m_DictionaryDatRU : m_DictionaryAccRU)
	                      : (nounCase == Dative ? m_DictionaryDatBY : m_DictionaryAccBY);
}

// the decree may still have a capital or Latin "i"; the dictionary key does not
Dictionary::const_iterator CProfessionConverter::Lookup(const Dictionary& dictionary, std::wstring word, LangType lang)
{
	return dictionary.find(dictionaryKey(std::move(word), lang));
}

// replaces the word of oldLength at pos, keeping its first letter (it may be capital)
void CProfessionConverter::ReplaceKeepingFirstLetter(std::wstring& text, size_t pos, size_t oldLength, const std::wstring& replacement)
{
	text.replace(pos + 1, oldLength - 1, replacement, 1, std::wstring::npos);
}

// "загадчыку кафедры, прафесару": a non-military rank after the last comma
void CProfessionConverter::ConvertNonMilitaryRank(std::wstring& strSource, NounCase nounCase)
{
	const size_t posComma = strSource.rfind(L',');
	if (posComma == std::wstring::npos)
		return;
	const size_t posStart = strSource.find_first_not_of(L" ,", posComma);
	if (posStart == std::wstring::npos)
		return;
	const std::wstring strCheck = strSource.substr(posStart);
	for (const auto& rank : NonMilitatyRanksBY)
	{
		const std::wstring& rankOriginal = (nounCase == Dative ? rank.Dative : rank.Accusative);
		if (rankOriginal.length() > strCheck.length())
			continue;
		if (strCheck.compare(0, rankOriginal.length(), rankOriginal) == 0)
		{
			strSource.replace(posStart, rankOriginal.length(), rank.Nominative);
			return;
		}
	}
}

// converts the dictionary word at posStart (iter is its entry) and what belongs to it:
// the other parts of a compound ("урача-анестэзіёлага-рэаніматолага") and, after an
// adjective, the next words of the profession ("дзяжурнага памочніка", "вядучага
// навуковага супрацоўніка", "вядучага ветэрынарнага ўрача"); returns the position
// right after the converted words
size_t CProfessionConverter::ConvertWordChain(std::wstring& strSource, size_t posStart, Dictionary::const_iterator iter,
                                              LangType lang, NounCase nounCase)
{
	return ConvertWordChain(strSource, posStart, iter->first.length(), iter->second, lang, nounCase);
}

size_t CProfessionConverter::ConvertWordChain(std::wstring& strSource, size_t posStart, size_t declinedLength,
                                              std::wstring nominative, LangType lang, NounCase nounCase)
{
	const Dictionary& dictionary = DictionaryFor(lang, nounCase);
	for (;;)
	{
		ReplaceKeepingFirstLetter(strSource, posStart, declinedLength, nominative);

		// the other parts: "урача-анестэзіёлага-рэаніматолага"
		posStart += nominative.length();
		while (posStart < strSource.length() && strSource[posStart] == L'-')
		{
			std::wstring word = ToLower(GetNextWord(strSource, posStart));
			if (word.empty())
				break;
			size_t partLength = word.length();
			const auto partIter = Lookup(dictionary, word, lang);
			std::wstring restored;
			if (partIter != dictionary.end())
			{
				ReplaceKeepingFirstLetter(strSource, posStart, partIter->first.length(), partIter->second);
				partLength = partIter->second.length();
			}
			else if (nominativeFromRules(word, lang, nounCase, restored))
			{
				ReplaceKeepingFirstLetter(strSource, posStart, word.length(), restored);
				partLength = restored.length();
			}
			else
			{
				const size_t wordLength = word.length();
				if (AddNewWord(word, lang, nounCase))
				{
					ReplaceKeepingFirstLetter(strSource, posStart, wordLength, word);
					partLength = word.length();
				}
			}
			posStart += partLength;
		}

		if (!IsAdjective(nominative) || posStart >= strSource.length() || strSource[posStart] != L' ')
			return posStart;
		size_t posNext = posStart;
		const std::wstring next = ToLower(GetNextWord(strSource, posNext));
		if (next.empty() || posNext != posStart + 1)
			return posStart;

		std::wstring nextNominative;
		size_t nextDeclined = next.length();
		const auto nextIter = Lookup(dictionary, next, lang);
		bool nextFound = false;
		if (nextIter != dictionary.end())
		{
			nextNominative = nextIter->second;
			nextDeclined = nextIter->first.length();
			nextFound = true;
		}
		else if (nominativeFromRules(next, lang, nounCase, nextNominative))
			nextFound = true;
		// not "следчага следчага аддзялення": the second one belongs to "аддзялення"
		if (nextFound && nextNominative == nominative)
			return posStart;
		if (!nextFound)
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
				std::wstring nounNominative;
				size_t nounDeclined = noun.length();
				bool nounFound = false;
				if (!noun.empty() && posWord == posNoun)
				{
					const auto nounIter = Lookup(dictionary, noun, lang);
					if (nounIter != dictionary.end())
					{
						nounNominative = nounIter->second;
						nounDeclined = nounIter->first.length();
						nounFound = true;
					}
					else if (nominativeFromRules(noun, lang, nounCase, nounNominative))
						nounFound = true;
				}
				if (nounFound && !IsAdjective(nounNominative))
				{
					strSource.replace(posNext, next.length(), adjective);
					posStart = posNoun + adjective.length() - next.length();
					declinedLength = nounDeclined;
					nominative = nounNominative;
					continue;
				}
			}
			return posStart;
		}
		posStart = posNext;
		declinedLength = nextDeclined;
		nominative = nextNominative;
	}
}

// the profession in brackets after the main one: "артыст-вакаліст (саліста)",
// "артыст балета (салісту)", "начальнік упраўлення (статс-сакратара)"; only the
// profession itself: "(загадчыка аддзялення)" -> "(загадчык аддзялення)",
// "(галоўнага ўрача)" -> "(галоўны ўрач)", the rest is genitive. Returns the
// position after the brackets (posStart if there are none)
size_t CProfessionConverter::ConvertBracketed(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase)
{
	const Dictionary& dictionary = DictionaryFor(lang, nounCase);
	const size_t posOpen = strSource.find(L'(', posStart);
	if (posOpen == std::wstring::npos)
		return posStart;
	const std::wstring between = strSource.substr(posStart, posOpen - posStart);
	const size_t posClose = strSource.find(L')', posOpen);
	if (posClose == std::wstring::npos || between.find_first_of(L",–«»;") != std::wstring::npos
		|| std::count(between.begin(), between.end(), L' ') > 3)
		return posStart;

	// each part of "статс-сакратара" separately ("урача-тэрапеўта": a part after a
	// converted one is converted by the rules)
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
		const auto partIter = Lookup(dictionary, part, lang);
		size_t partLength = originalLength;
		std::wstring restored;
		if (partIter != dictionary.end())
		{
			ReplaceKeepingFirstLetter(strSource, posPart, originalLength, partIter->second);
			partLength = partIter->second.length();
			converted = true;
		}
		else if (nominativeFromRules(part, lang, nounCase, restored))
		{
			ReplaceKeepingFirstLetter(strSource, posPart, originalLength, restored);
			partLength = restored.length();
			converted = true;
		}
		else if (converted && afterHyphen && AddNewWord(part, lang, nounCase))
		{
			ReplaceKeepingFirstLetter(strSource, posPart, originalLength, part);
			partLength = part.length();
		}
		else if (!converted && !afterHyphen && posPartEnd < posCloseNow && strSource[posPartEnd] == L' '
			&& CNameConverter::ApplyRules(part, RulesAdjectives, SexAny, lang, nounCase, false))
		{
			// an adjective before the profession
			const size_t posNext = posPartEnd + 1;
			const size_t posNextEnd = std::min(strSource.find_first_of(L"- )", posNext), posCloseNow);
			std::wstring nextNominative;
			const std::wstring nextWord = ToLower(strSource.substr(posNext, posNextEnd - posNext));
			if (Lookup(dictionary, nextWord, lang) == dictionary.end()
				&& !nominativeFromRules(nextWord, lang, nounCase, nextNominative))
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
	return strSource.find(L')', posOpen) + 1;
}

// a second profession after a comma: "саліст оперы, вядучага майстра сцэны",
// "кампазітар, члена Саюза кампазітараў" (not inside quotes)
void CProfessionConverter::ConvertAfterComma(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase)
{
	const Dictionary& dictionary = DictionaryFor(lang, nounCase);
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
				compoundAdjective = EndsWithAny(part, { L"ай", L"ой", L"ых", L"іх", L"их", L"ым", L"ае", L"ое", L"ыя", L"ые" });
			}
			// an adjective from the dictionary before "і"/"и" is part of a list of
			// adjectives, not a profession: "грамадзянскага, працоўнага і сацыяльнага
			// заканадаўства" (but "заведующего кафедрой" is one)
			const auto found = Lookup(dictionary, next, lang);
			std::wstring restored;
			const bool knownByRules = found == dictionary.end()
				&& nominativeFromRules(next, lang, nounCase, restored);
			bool beforeConjunction = false;
			if (found != dictionary.end() && IsAdjective(found->second))
			{
				size_t posAfter = posWord;
				const std::wstring after = ToLower(GetNextWord(strSource, posAfter));
				beforeConjunction = after == L"і" || after == L"и" || after == L"i";
			}
			if (beforeConjunction)
				break;
			if ((found != dictionary.end() || knownByRules) && !compoundAdjective)
				profession = true;
			else if (!CNameConverter::ApplyRules(next, RulesAdjectives, SexAny, lang, nounCase, false))
				break;
		}
		if (!profession)
			continue;
		std::wstring rest = strSource.substr(posComma + 1);
		ConvertProfession(rest, lang, nounCase);
		strSource.replace(posComma + 1, std::wstring::npos, rest);
		return;
	}
}

// the words before the main one far enough from the start are adjectives:
// "старшага інспектара" -> "старшы інспектар"
void CProfessionConverter::ConvertLeadingAdjectives(std::wstring& strSource, size_t posMainWord, LangType lang, NounCase nounCase)
{
	size_t posStart = 0;
	while (posStart < posMainWord)
	{
		std::wstring word = GetNextWord(strSource, posStart);
		if (word.empty())
			break;

		const size_t wordLength = word.length();
		if (CNameConverter::ApplyRules(word, RulesAdjectives, SexAny, lang, nounCase, false))
			strSource.replace(posStart, wordLength, word);
		posStart += word.length() + 1;
	}
}

bool CProfessionConverter::ConvertProfession(std::wstring& strSource, LangType lang, NounCase nounCase)
{
	bool bResult = false;
	const Dictionary& dictionary = DictionaryFor(lang, nounCase);

	ConvertNonMilitaryRank(strSource, nounCase);

	// the main word is searched among the first words
	const int maxProfessionWords = 5;
	size_t posStart = 0, posMainWord = 0;
	for (int i = 0; i < maxProfessionWords && posStart < strSource.length(); i++)
	{
		const std::wstring word = ToLower(GetNextWord(strSource, posStart));
		if (word.empty())
			break;

		// "исполняющего обязанности начальника": the profession is the adjective
		// before it, the rest is genitive (converted as leading adjectives)
		if (i > 0 && (word == L"обязанности" || word == L"абавязкі" || word == L"абавязкi"))
		{
			bResult = true;
			posMainWord = posStart;
			break;
		}

		const auto iter = Lookup(dictionary, word, lang);
		std::wstring restored;
		const bool knownByRules = iter == dictionary.end()
			&& nominativeFromRules(word, lang, nounCase, restored);
		if (iter != dictionary.end() || knownByRules)
		{
			bResult = true;
			posMainWord = posStart;
			posStart = iter != dictionary.end()
				? ConvertWordChain(strSource, posStart, iter, lang, nounCase)
				: ConvertWordChain(strSource, posStart, word.length(), restored, lang, nounCase);
			posStart = ConvertBracketed(strSource, posStart, lang, nounCase);
			ConvertAfterComma(strSource, posStart, lang, nounCase);
			break;
		}

		posStart += word.length() + 1;
	}

	if (!bResult)
		FindNewWords(strSource, lang, nounCase);

	// a main word at the very start has no adjectives before it
	const size_t minAdjectivesLength = 5;
	if (posMainWord >= minAdjectivesLength)
		ConvertLeadingAdjectives(strSource, posMainWord, lang, nounCase);

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
		// the other case already stores this nominative: substitute, do not propose it
		const auto& known = lang == LangRU ? m_NominativesRU : m_NominativesBY;
		if (known.find(word) != known.end())
			return true;

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

