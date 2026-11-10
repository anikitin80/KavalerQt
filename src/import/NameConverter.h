#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

enum SexType
{
	SexAny = 0,
	SexMale,
	SexFemale
};

enum LangType
{
	LangAny = 0,
	LangRU,
	LangBY
};

enum NounCase
{
	Nominative = 0,
	Dative,
	Accusative
};

struct Rule
{
	Rule(SexType sex, LangType lang, const wchar_t* sDat, const wchar_t* sAcc, const wchar_t* sNom)
		: Sex(sex), Language(lang), SuffixDat(sDat), SuffixAcc(sAcc), SuffixNom(sNom)
	{
		// «?» — не буква основы: «---ый?» значит, что именительный угадан
		if (!SuffixNom.empty() && SuffixNom.back() == L'?')
		{
			Uncertain = true;
			SuffixNom.pop_back();
		}
	};

	bool Match(const std::wstring& nameLower, SexType sex, LangType lang, NounCase nounCase, bool bFullWordMatch) const
	{
		if (Language != LangAny && Language != lang)
			return false;
		if (Sex != SexAny && sex != Sex)
			return false;
		const std::wstring& suffix = (nounCase == Dative ? SuffixDat : SuffixAcc);
		if (suffix.empty())
			return false;
		if (bFullWordMatch)
			return nameLower == suffix;
		if (nameLower.length() < suffix.length())
			return false;

		return nameLower.compare(nameLower.length() - suffix.length(), suffix.length(), suffix) == 0;
	};

	bool Apply(std::wstring& name) const
	{
		if (SuffixNom.empty())
			return true;

		for (auto c : SuffixNom)
		{
			if (c == L'-')
			{
				if (name.empty())
					return false;
				name.erase(name.length() - 1, 1);
			}
			else
				name += c;
		}

		return true;
	};

	SexType Sex;
	LangType Language;
	std::wstring SuffixDat;
	std::wstring SuffixAcc;
	std::wstring SuffixNom;
	bool Uncertain = false;
};

typedef std::vector<Rule> RulesList;

class CNameConverter
{
public:
	bool NormalizeName(std::wstring& fullName, LangType lang, NounCase nounCase);
	SexType GetSex(std::wstring& middleName, NounCase nounCase);
	bool IsMiddleName(const std::wstring& name, LangType lang, NounCase nounCase);

	// фамилии каталога (нижний регистр → сколько записей): косвенная форма часто
	// даёт несколько именительных («Склюту» — Склют или Склюта, «Лобанку» —
	// Лобанок), и тогда берётся тот, что каталог уже знает. Пусто — только правила.
	void SetKnownSurnames(LangType lang, std::unordered_map<std::wstring, int> counts);
	// именительные, которые может дать косвенная фамилия oblique, кроме ruleResult
	static std::vector<std::wstring> SurnameVariants(const std::wstring& oblique, const std::wstring& ruleResult, SexType sex);

	static bool ApplyRules(std::wstring& name, const RulesList& rules, SexType sex, LangType lang, NounCase nounCase, bool bFullWordMatch);
	// правило с окончаниями «ую,ею» / «уя,ея» становится отдельным правилом на каждую пару;
	// им собирается каждая таблица (без него списки через запятую никогда не совпадут)
	static RulesList ExpandRules(const RulesList& rules);

protected:
	static bool ForEachPart(std::wstring& word, const std::function<bool(std::wstring&)>& convert);
	// фамилия правила неизвестна каталогу, а один из вариантов известен: берётся он
	bool ChooseKnownSurname(std::wstring& surname, const std::wstring& oblique, SexType sex, LangType lang) const;

	std::unordered_map<std::wstring, int> m_KnownSurnamesRU;
	std::unordered_map<std::wstring, int> m_KnownSurnamesBY;

	static const RulesList ExceptionsFirstName;
	static const RulesList ExceptionsLastName;
	static const RulesList RulesFirstName;
	static const RulesList RulesLastName;
	static const RulesList RulesMiddleName;
};

