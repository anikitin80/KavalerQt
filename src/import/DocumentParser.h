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
	// именительный единственного числа, когда своей должности нет:
	// «работников ...» → «работник ...», «военнослужащих ...» → «военнослужащий ...»
	std::wstring Role;
	// вторая организация после «і» или «и»: тому, кто уже её назвал, первую не дописываем
	std::wstring Other;
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
	// requirePatronymic: при разборе файла режем только полное имя с отчеством.
	// Кнопка в импорте оставляет и пару без отчества.
	bool SpltPersonInfo(PersonInfo& person, bool requirePatronymic = false);
	NounCase GetNounCaseByAwardId(int awardId);

	std::wstring GetPersonDescription(size_t nIndex);
	std::wstring GetPersonDescription(PersonInfo& person, bool bGetRank);
	bool  GetGroupInfo(std::wstring& fileName, GroupInfo& groupInfo);
	void  UpdateGroupInfo(const wchar_t* fileName, const wchar_t* groupInfo);
	void  CheckGroupInfo(const wchar_t* fileName, bool bCheck);

	static bool IsMilitaryAward(int awardId);

	// слова должностей и званий каталога (нижний регистр → сколько раз): по ним
	// дефис переноса строки («ком-плексу», «пра-паршчыка») отличается от дефиса
	// составного слова. Пусто — дефисы не трогаются.
	void SetKnownWords(LangType lang, std::map<std::wstring, int> counts);
	// убирает дефис переноса, если каталог знает слово слитно и не видел его с дефисом
	void JoinWrappedWords(std::wstring& text, LangType lang) const;

	std::vector<AwardInfo> m_AwardsStat;
	std::vector<PersonInfo> m_Persons;
	std::vector<GroupInfo> m_GroupsInfo;
	UkazInfo m_UkazInfo;
	CNameConverter m_NameConverter;
	CProfessionConverter m_ProfessionConverter;

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
	// истина, если в строке есть сведения о группе
	bool ParseGroupInfo(const std::wstring& strLine);
	void ParseSection(const std::wstring& strLine);
	void UpdateAwardStat(int id, std::wstring& award, int nCount);
	void ExtractPersonInfo(std::wstring& curAward, std::wstring& strAwardedInfo, int knownAwardId = 0);
	void NormalizePersonInfo(PersonInfo& person);
	// «художникам Аракчееву …, Порожняку … и Шутову …»: имя предыдущего человека
	// не должность. Запятая или союз в конце отделяют его в свою запись.
	bool peelListedPeople(PersonInfo& person);
	void AddGroupInfo(const UkazInfo& ukaz, const std::wstring& fileName, const std::wstring& groupInfo,
		const std::wstring& role, const std::wstring& other);
	bool GetGroupInfo(const UkazInfo& ukaz, std::wstring& info, std::wstring& role, std::wstring& other);

	// phraseLength, если задан, получает, сколько начальных символов award —
	// сама фраза награды (с кавычками и суффиксом страны), дальше идёт человек.
	// 0, если ничего не совпало.
	int  GetAwardId(std::wstring award, bool bRussian, bool bPochetNameOnly = false, size_t* phraseLength = nullptr);

	std::map<std::wstring, int> m_mapFileMonth;
	std::wstring m_FileName;
	std::wstring m_SectionCity; // «г. Мінск» в городском разделе списка
	// сведения о группе действуют: из заголовка указа — до его конца,
	// из пункта («1. ... узнагародзіць работнікаў ...») — до следующего пункта
	bool m_GroupActive = false;
	std::map<std::wstring, int> m_KnownWordsRU;
	std::map<std::wstring, int> m_KnownWordsBY;
	bool m_GroupWholeDecree = false;
};

