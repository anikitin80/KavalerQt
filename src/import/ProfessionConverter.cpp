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
	{ SexAny, LangBY, L"каму", L"кага", L"---і"},
	{ SexAny, LangBY, L"аму", L"ага", L"---ы"},
	{ SexFemale, LangBY, L"ай", L"ую", L"--ая"},
	{ SexFemale, LangBY, L"яй", L"юю", L"--яя"},
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
		const QString line = input.readLine();
		const int pos1 = line.indexOf(QLatin1Char(';'));
		const int pos2 = line.indexOf(QLatin1Char(';'), pos1 + 1);
		if (pos2 != -1) {
			(*dictionaryDat)[line.mid(pos1 + 1, pos2 - pos1 - 1).toStdWString()] = line.left(pos1).toStdWString();
			(*dictionaryAcc)[line.mid(pos2 + 1).toStdWString()] = line.left(pos1).toStdWString();
		} else if (pos1 != -1) {
			(*dictionaryAcc)[line.left(pos1).toStdWString()] = line.mid(pos1 + 1).toStdWString();
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
			if(item.Case == Dative)
				m_DictionaryDatBY[item.Original] = item.Word;
			else
				m_DictionaryAccBY[item.Original] = item.Word;
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

		auto iter = dictionary.find(word);
		if (iter != dictionary.end())
		{
			strSource.erase(posStart + 1, iter->first.length() - 1); // leave first letter (may be capital)
			strSource.insert(posStart + 1, iter->second.substr(1));
			
			bResult = true;
			posMainWord = posStart;

			// check second part
			posStart += iter->second.length();
			if (posStart < strSource.length() && strSource[posStart] == L'-')
			{
				word = ToLower(GetNextWord(strSource, posStart));
				iter = dictionary.find(word);
				if (iter != dictionary.end())
				{
					strSource.erase(posStart + 1, iter->first.length() - 1); // leave first letter (may be capital)
					strSource.insert(posStart + 1, iter->second.substr(1));
				}
				else
				{
					int nWordLen = word.length();
					if (AddNewWord(word, lang, nounCase))
					{
						strSource.erase(posStart + 1, nWordLen-1);
						strSource.insert(posStart + 1, word.substr(1));
					}
				}
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
	if (CNameConverter::ApplyRules(word, RulesProfession, SexAny, lang, nounCase, false))
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

