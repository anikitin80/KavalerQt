#pragma once

#include <functional>
#include <string>
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
		// "?" is not a letter of the stem: "---ый?" means the nominative is a guess
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

	static bool ApplyRules(std::wstring& name, const RulesList& rules, SexType sex, LangType lang, NounCase nounCase, bool bFullWordMatch);
	// a rule with endings "ую,ею" / "уя,ея" becomes one rule per pair of endings;
	// every rule table is built with it (a table written without it would never
	// match its comma lists)
	static RulesList ExpandRules(const RulesList& rules);

protected:
	static bool ForEachPart(std::wstring& word, const std::function<bool(std::wstring&)>& convert);

	static const RulesList ExceptionsFirstName;
	static const RulesList ExceptionsLastName;
	static const RulesList RulesFirstName;
	static const RulesList RulesLastName;
	static const RulesList RulesMiddleName;
};

