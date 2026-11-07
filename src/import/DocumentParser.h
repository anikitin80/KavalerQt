#pragma once

#include "ParserTypes.h"
#include "NameConverter.h"
#include "ProfessionConverter.h"

#include <map>
#include <vector>

struct GroupInfo
{
	std::wstring FileName;
	std::wstring Info;
	int UkazNumber = 0;
	QDate UkazDate;
	bool Checked = false;
};

class CDocumentParser
{
public:
	void Initialize(std::map<long, DecreeAward>& awards);
	void ResetData();

	bool ProcessFile(const std::wstring& fileName, const std::vector<std::wstring>& arrLines);
	int  GetFileMonth(const std::wstring& fileName) { return m_mapFileMonth[fileName];  }
	bool SpltPersonInfo(PersonInfo& person);
	NounCase GetNounCaseByAwardId(int awardId);

	std::wstring GetPersonDescription(size_t nIndex);
	std::wstring GetPersonDescription(PersonInfo& person, bool bGetRank);
	bool  GetGroupInfo(std::wstring& fileName, GroupInfo& groupInfo);
	void  UpdateGroupInfo(const wchar_t* fileName, const wchar_t* groupInfo);
	void  CheckGroupInfo(const wchar_t* fileName, bool bCheck);

	static bool IsMilitaryAward(int awardId);

	std::vector<AwardInfo> m_AwardsStat;
	std::vector<PersonInfo> m_Persons;
	std::vector<GroupInfo> m_GroupsInfo;
	UkazInfo m_UkazInfo;
	CNameConverter m_NameConverter;
	CProfessionConverter m_ProfessionConverter;
	static const std::vector<RankInfo> MilitatyRanksRU;
	static const std::vector<RankInfo> MilitatyRanksBY;

	std::map<long, DecreeAward> m_Awards;

	typedef std::vector<std::pair<std::wstring, int>> AwardDetector;
	AwardDetector m_MedalDetector;
	AwardDetector m_OrdenDetector;
	AwardDetector m_PochetDetector;

	AwardDetector m_MedalDetectorBY;
	AwardDetector m_OrdenDetectorBY;
	AwardDetector m_PochetDetectorBY;

protected:

	void ParseDecreeHeader(const std::wstring& strLine);
	void ParseGroupInfo(const std::wstring& strLine);
	void ParseSection(const std::wstring& strLine);
	void UpdateAwardStat(int id, std::wstring& award, int nCount);
	void ExtractPersonInfo(std::wstring& curAward, std::wstring& strAwardedInfo);
	void NormalizePersonInfo(PersonInfo& person);
	bool ExtractMilitaryRank(PersonInfo& person, LangType lang, NounCase nounCase);
	void AddGroupInfo(const UkazInfo& ukaz, const std::wstring& fileName, const std::wstring& groupInfo);
	bool GetGroupInfo(const UkazInfo& ukaz, std::wstring& info);

	// phraseLength, если задан, получает, сколько начальных символов award —
	// сама фраза награды (с кавычками и суффиксом страны), дальше идёт человек.
	// 0, если ничего не совпало.
	int  GetAwardId(std::wstring award, bool bRussian, bool bPochetNameOnly = false, size_t* phraseLength = nullptr);

	std::map<std::wstring, int> m_mapFileMonth;
	std::wstring m_FileName;
	std::wstring m_SectionCity; // «г. Мінск» в городском разделе списка
};

