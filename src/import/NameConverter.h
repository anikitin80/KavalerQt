#pragma once

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
	Rule()
		: Sex(SexMale), Language(LangAny)
	{
	};

	Rule(SexType sex, LangType lang, const wchar_t* sDat, const wchar_t* sAcc, const wchar_t* sNom)
		: Sex(sex), Language(lang), SuffixDat(sDat), SuffixAcc(sAcc), SuffixNom(sNom)
	{
	};

	bool Match(const std::wstring& nameLower, SexType sex, LangType lang, NounCase nounCase, bool bFullWordMatch) const
	{
		if (Language != LangAny && Language != lang)
			return false;
		if (Sex != SexAny && sex != Sex) 
			return false;
		std::wstring suffix = (nounCase == Dative ? SuffixDat : SuffixAcc);
		if (suffix.empty())
			return false;
		if (bFullWordMatch)
			return nameLower == suffix;
		if (nameLower.length() < suffix.length())
			return false;
		
		return nameLower.substr(nameLower.length() - suffix.length()) == suffix;
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
};

typedef std::vector<Rule> RulesList;

class CNameConverter
{
public:
	CNameConverter();
	
	bool NormalizeName(std::wstring& fullName, LangType lang, NounCase nounCase);
	SexType GetSex(std::wstring& middleName, NounCase nounCase);
	bool IsMiddleName(const std::wstring& name, LangType lang, NounCase nounCase);

	static bool ApplyRules(std::wstring& name, RulesList& rules, SexType sex, LangType lang, NounCase nounCase, bool bFullWordMatch);

protected:
	void ExpandRules(RulesList& rules);
	
	static RulesList ExceptionsFirstName;
	static RulesList ExceptionsLastName;
	static RulesList RulesFirstName;
	static RulesList RulesLastName;
	static RulesList RulesMiddleName;
	static bool m_bIsInitialized;
};

