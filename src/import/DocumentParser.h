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
	CDocumentParser();

	void Initialize(std::map<long, DecreeAward>& awards);
	void ResetData();

	bool ProcessFile(const std::wstring& fileName, std::vector<std::wstring>& arrLines);
	int  GetFileMonth(const std::wstring& fileName) { return m_mapFileMonth[fileName];  }
	bool SpltPersonInfo(PersonInfo& person);
	NounCase GetNounCaseByAwardId(int awardId);

	std::wstring GetPersonDescription(size_t nIndex);
	std::wstring GetPersonDescription(PersonInfo& person, bool bGetRank);
	bool  GetGroupInfo(std::wstring& fileName, GroupInfo& groupInfo);
	void  UpdateGroupInfo(const wchar_t* fileName, const wchar_t* groupInfo);
	void  CheckGroupInfo(const wchar_t* fileName, bool bCheck);

	//static bool TranslateRank(std::wstring& strText, std::wstring& strResult);
	static bool IsMilitaryAward(int awardId);

	std::vector<AwardInfo> m_AwardsStat;
	std::vector<PersonInfo> m_Persons;
	std::vector<GroupInfo> m_GroupsInfo;
	UkazInfo m_UkazInfo;
	//CPadegDll m_PadegDll;
	CNameConverter m_NameConverter;
	CProfessionConverter m_ProfessionConverter;
	static std::vector<RankInfo> MilitatyRanksRU;
	static std::vector<RankInfo> MilitatyRanksBY;

	std::map<long, DecreeAward> m_Awards;

	typedef std::vector<std::pair<std::wstring, int>> AwardDetector;
	AwardDetector m_MedalDetector;
	AwardDetector m_OrdenDetector;
	AwardDetector m_PochetDetector;

	AwardDetector m_MedalDetectorBY;
	AwardDetector m_OrdenDetectorBY;
	AwardDetector m_PochetDetectorBY;

protected:

	void UpdateAwardStat(int id, std::wstring& award, int nCount);
	void ExtractPersonInfo(std::wstring& curAward, std::wstring& strAwardedInfo);
	void NormalizePersonInfo(PersonInfo& person);
	bool ExtractMilitaryRank(PersonInfo& person, LangType lang, NounCase nounCase);
	void AddGroupInfo(const UkazInfo& ukaz, const std::wstring& fileName, const std::wstring& groupInfo);
	bool GetGroupInfo(const UkazInfo& ukaz, std::wstring& info);

	int  GetAwardId(std::wstring award, bool bRussian, bool bPochetNameOnly = false);
			
	std::map<std::wstring, int> m_mapFileMonth;
	std::wstring m_FileName;
};

