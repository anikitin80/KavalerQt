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


	void Initialize();

	bool SetDictionaryPath(const std::wstring& filePath, LangType lang);
	const std::wstring& DictionaryPath(LangType lang) const;
	bool LoadDictionary(const std::wstring& filePath, LangType lang);
	bool SaveDictionary(const std::wstring& filePath, LangType lang);
	bool UpdateDictionary(std::vector<NewDictItem>& newItems);

	// allowUnknownHead: именительный слова, которого ещё нет в словаре.
	// Для части после « – » выключен: там родительный места работы.
	bool ConvertProfession(std::wstring& strSource, LangType lang, NounCase nounCase, bool allowUnknownHead = true);
	// posStart возвращается на первую букву. posEnd, если задан, — разделитель после слова.
	static std::wstring GetNextWord(const std::wstring& strSource, size_t& posStart, size_t* posEnd = nullptr);
	void FindNewWords(std::wstring& strSource, LangType lang, NounCase nounCase);
	bool AddNewWord(std::wstring& word, LangType lang, NounCase nounCase);

	const std::vector<NewDictItem>& newWords() const { return m_NewWords; }
	void clearNewWords() { m_NewWords.clear(); }

protected:
	// шаги ConvertProfession
	const Dictionary& DictionaryFor(LangType lang, NounCase nounCase) const;
	static Dictionary::const_iterator Lookup(const Dictionary& dictionary, std::wstring word, LangType lang);
	static void ReplaceKeepingFirstLetter(std::wstring& text, size_t pos, size_t oldLength, const std::wstring& replacement);
	void rebuildNominatives(LangType lang);
	void rememberNominative(LangType lang, std::wstring nominative);
	// склонённого слова нет в словаре этого падежа, но RulesProfession
	// даёт именительный, уже известный для языка
	bool nominativeFromRules(std::wstring word, LangType lang, NounCase nounCase, std::wstring& nominative) const;
	// правила должности без проверки словаря: уже именительное не укорачивается,
	// родительный на «-ння» не становится «-ннь»
	bool ruleNominative(std::wstring word, LangType lang, NounCase nounCase, std::wstring& nominative) const;
	// первое слово должности, которого нет в словаре: «историка» → «историк»
	bool restoreUnknownHead(std::wstring& strSource, size_t& posMainWord, LangType lang, NounCase nounCase);
	// склонённое слово из словаря или из RulesProfession, если такой именительный уже есть
	struct WordForm
	{
		bool found = false;
		bool inDictionary = false;
		std::wstring nominative;
		size_t declinedLength = 0;
	};
	WordForm findForm(const std::wstring& word, LangType lang, NounCase nounCase) const;
	void ConvertNonMilitaryRank(std::wstring& strSource, NounCase nounCase);
	size_t ConvertWordChain(std::wstring& strSource, size_t posStart, size_t declinedLength, std::wstring nominative, LangType lang, NounCase nounCase);
	size_t ConvertBracketed(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase);
	void ConvertAfterComma(std::wstring& strSource, size_t posStart, LangType lang, NounCase nounCase);
	void ConvertLeadingAdjectives(std::wstring& strSource, size_t posMainWord, LangType lang, NounCase nounCase);

	// дательный, винительный, файл и известные именительные одного языка
	struct LanguageData
	{
		Dictionary dative;
		Dictionary accusative;
		std::wstring path;
		std::set<std::wstring> nominatives;
	};
	LanguageData& languageData(LangType lang);
	const LanguageData& languageData(LangType lang) const;

	LanguageData m_russian;
	LanguageData m_belarusian;

	std::vector<NewDictItem> m_NewWords;
	static const RulesList RulesAdjectives;
	static const RulesList RulesProfession;
	static const std::vector<RankInfo> NonMilitatyRanksBY;
};

