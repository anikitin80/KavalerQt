#pragma once

#include "NameConverter.h"

#include <map>
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

	const std::vector<NewDictItem>& newWords() const { return m_NewWords; }
	void clearNewWords() { m_NewWords.clear(); }

protected:
	// the steps of ConvertProfession
	const Dictionary& DictionaryFor(LangType lang, NounCase nounCase) const;
	static Dictionary::const_iterator Lookup(const Dictionary& dictionary, std::wstring word, LangType lang);
	static void ReplaceKeepingFirstLetter(std::wstring& text, size_t pos, size_t oldLength, const std::wstring& replacement);
	void ConvertNonMilitaryRank(std::wstring& strSource, NounCase nounCase);
	size_t ConvertWordChain(std::wstring& strSource, size_t posStart, Dictionary::const_iterator iter, LangType lang, NounCase nounCase);
	size_t ConvertBracketed(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase);
	void ConvertAfterComma(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase);
	void ConvertLeadingAdjectives(std::wstring& strSource, size_t posMainWord, LangType lang, NounCase nounCase);

	Dictionary m_DictionaryDatRU;
	Dictionary m_DictionaryAccRU;
	std::wstring m_FilePathRU;

	Dictionary m_DictionaryDatBY;
	Dictionary m_DictionaryAccBY;
	std::wstring m_FilePathBY;

	std::vector<NewDictItem> m_NewWords;
	static const RulesList RulesAdjectives;
	static const RulesList RulesProfession;
	static const std::vector<RankInfo> NonMilitatyRanksBY;
};

