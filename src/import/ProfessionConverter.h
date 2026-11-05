#pragma once

#include "NameConverter.h"

#include <map>
#include <set>
#include <string>
#include <vector>

struct RankInfo
{
	std::wstring Nominative;
	std::wstring Dative;
	std::wstring Accusative;
	
};

struct NewDictItem
{
	std::wstring Word;
	std::wstring Original;
	LangType Language;
	NounCase Case;
	bool Checked;
};

typedef std::map<std::wstring, std::wstring> Dictionary;
class CProfessionConverter
{
public:

	CProfessionConverter();

	void Initialize();

	bool SetDictionaryPath(const std::wstring& filePath, LangType lang);
	const std::wstring& DictionaryPath(LangType lang) const;
	bool LoadDictionary(const std::wstring& filePath, LangType lang);
	bool SaveDictionary(const std::wstring& filePath, LangType lang);
	bool UpdateDictionary(std::vector<NewDictItem>& newItems);

	bool ConvertProfession(std::wstring& strSource, LangType lang, NounCase nounCase);
	static std::wstring GetNextWord(std::wstring& strSource, size_t& posStart);
	void FindNewWords(std::wstring& strSource, LangType lang, NounCase nounCase);
	bool AddNewWord(std::wstring& word, LangType lang, NounCase nounCase);

	std::vector<NewDictItem> GetNewWords() { return m_NewWords; }
	const std::vector<NewDictItem>& newWords() const { return m_NewWords; }
	void clearNewWords() { m_NewWords.clear(); }

protected:
	Dictionary m_DictionaryDatRU;
	Dictionary m_DictionaryAccRU;
	std::wstring m_FilePathRU;

	Dictionary m_DictionaryDatBY;
	Dictionary m_DictionaryAccBY;
	std::wstring m_FilePathBY;

	std::vector<NewDictItem> m_NewWords;
	static RulesList RulesAdjectives;
	static RulesList RulesProfession;
	static std::vector<RankInfo> NonMilitatyRanksBY;
};

