#include "DocumentParser.h"
#include "TextUtil.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>

std::vector<RankInfo> CDocumentParser::MilitatyRanksRU = {
	{ L"рядовой", L"рядовому", L"рядового" },
	{ L"прапорщик", L"прапорщику", L"прапорщика"},
	{ L"старший прапорщик", L"старшему прапорщику", L"старшего прапорщика"},
	{ L"младший сержант", L"младшему сержанту", L"младшего сержанта" },
	{ L"старший сержант", L"старшему сержанту", L"старшего сержанта" },
	{ L"сержант", L"сержанту", L"сержанта" },
	{ L"старшина", L"старшине", L"старшину" },
	{ L"младший лейтенант", L"младшему лейтенанту", L"младшего лейтенанта" },
	{ L"старший лейтенант", L"старшему лейтенанту", L"старшего лейтенанта" },
	{ L"лейтенант", L"лейтенанту", L"лейтенанта" },
	{ L"капитан", L"капитану", L"капитана" },
	{ L"майор", L"майору", L"майора" },
	{ L"подполковник", L"подполковнику", L"подполковника" },
	{ L"полковник", L"полковнику", L"полковника" },
	{ L"генерал-майор", L"генерал-майору", L"генерал-майора" },
	{ L"генерал-лейтенант", L"генерал-лейтенанту", L"генерал-лейтенанта" },
	{ L"генерал-полковник", L"генерал-полковнику", L"генерал-полковника" },
	{ L"ефрейтор", L"ефрейтору", L"ефрейтора" },
	{ L"мичман", L"мичману", L"мичмана" },
	{ L"старший мичман", L"старшему мичману", L"старшего мичмана" }
};

std::vector<RankInfo> CDocumentParser::MilitatyRanksBY = {
	{ L"радавы", L"радавому", L"радавога"},
	{ L"яфрэйтар", L"яфрэйтару", L"яфрэйтара"},
	{ L"прапаршчык", L"прапаршчыку", L"прапаршчыка"},
	{ L"мічман", L"мічману", L"мічмана"},
	{ L"старшы мічман", L"старшаму мічману", L"старшага мічмана"},
	{ L"старшы прапаршчык", L"старшаму прапаршчыку", L"старшага прапаршчыка"},
	{ L"малодшы сяржант", L"малодшаму сяржанту", L"малодшага сяржанта"},
	{ L"старшы сяржант", L"старшаму сяржанту", L"старшага сяржанта" },
	{ L"сяржант", L"сяржанту", L"сяржанта" },
	{ L"старшына", L"старшыне", L"старшыну" },
	{ L"малодшы лейтэнант", L"малодшаму лейтэнанту", L"малодшага лейтэнанта" },
	{ L"старшы лейтэнант", L"старшаму лейтэнанту", L"старшага лейтэнанта" },
	{ L"лейтэнант", L"лейтэнанту", L"лейтэнанта" },
	{ L"капітан", L"капітану", L"капітана" },
	{ L"маёр", L"маёру", L"маёра" },
	{ L"падпалкоўнік", L"падпалкоўніку", L"падпалкоўніка" },
	{ L"палкоўнік", L"палкоўніку", L"палкоўніка" },
	{ L"генерал-маёр", L"генерал-маёру", L"генерал-маёра" },
	{ L"генерал-лейтэнант", L"генерал-лейтэнанту", L"генерал-лейтэнанта" },
	{ L"генерал-палкоўнік", L"генерал-палкоўніку", L"генерал-палкоўніка" },
	{ L"саветнік", L"саветніку", L"саветніка" },
	{ L"старшы саветнік", L"старшаму саветніку", L"старшага саветніка" },
	{ L"малодшы саветнік", L"малодшаму саветніку", L"малодшага саветніка" },
	{ L"дзяржаўны саветнік", L"дзяржаўнаму саветніку", L"дзяржаўнага саветніка" }
};

CDocumentParser::CDocumentParser()
{

}

void CDocumentParser::Initialize(std::map<long, DecreeAward>& awards)
{
	m_ProfessionConverter.Initialize();

	m_Awards = awards;
	for (const auto& award : m_Awards)
	{
		std::wstring name = award.second.Name;
		name.erase(0, name.find(' ') + 1);
		name.erase(std::remove(name.begin(), name.end(), L'"'), name.end());

		std::wstring nameBY = award.second.NameBY;
		nameBY.erase(0, nameBY.find(' ') + 1);
		nameBY.erase(std::remove(nameBY.begin(), nameBY.end(), L'"'), nameBY.end());

		if (award.second.Type == DecreeAward::Orden)
		{
			m_OrdenDetector.push_back({ name, award.first });
			m_OrdenDetectorBY.push_back({ nameBY, award.first });
		}
		else if (award.second.Type == DecreeAward::Medal)
		{
			m_MedalDetector.push_back({ name, award.first });
			m_MedalDetectorBY.push_back({ nameBY, award.first });
		}
		else if (award.second.Type == DecreeAward::Pochet)
		{
			name.erase(0, name.find(' ') + 1);
			// cut the country suffix together with the space before it
			auto cutFrom = [](std::wstring& text, const wchar_t* token) {
				const size_t pos = text.find(token);
				if (pos != std::wstring::npos)
					text.erase(pos > 0 ? pos - 1 : 0);
			};
			cutFrom(name, L"Республики Беларусь");
			cutFrom(name, L"Беларуси");
			nameBY.erase(0, nameBY.find(' ') + 1);
			cutFrom(nameBY, L"Рэспублікі Беларусь");
			cutFrom(nameBY, L"Беларусі");
			m_PochetDetector.push_back({ name, award.first });
			m_PochetDetectorBY.push_back({ nameBY, award.first });
		}
	}

	// some old names (only when the award exists in the database)
	if (m_Awards.count(42))
		m_PochetDetector.push_back({ L"Заслуженный работник народного образования", 42 });
	if (m_Awards.count(37))
		m_PochetDetector.push_back({ L"Заслуженный работник культуры", 37 });
	if (m_Awards.count(64))
		m_PochetDetectorBY.push_back({ L"Заслужаны супрацоўнік органаў унутраных спраў", 64 });

	ResetData();
}

void CDocumentParser::ResetData()
{
	m_AwardsStat.clear();
	m_Persons.clear();
	m_mapFileMonth.clear();
	m_GroupsInfo.clear();
	m_UkazInfo.Clear();
}

int CDocumentParser::GetAwardId(std::wstring award, bool bRussian, bool bPochetNameOnly /*= false*/)
{
	//award.erase(std::remove(award.begin(), award.end(), L' '), award.end());
	//award.erase(std::remove(award.begin(), award.end(), L' '), award.end());
	award.erase(std::remove(award.begin(), award.end(), L'»'), award.end());
	award.erase(std::remove(award.begin(), award.end(), L'«'), award.end());
	award.erase(std::remove(award.begin(), award.end(), L'"'), award.end());

	// replace Latin letters with Belorussian
	//std::replace(award.begin(), award.end(), L'i', L'і');

	// replace Belorussian letters with Latin for degreee number
	std::replace(award.begin(), award.end(), L'І', L'I');

    // replace no-breakable space with the usual one
    std::replace(award.begin(), award.end(), L' ', L' ');

	AwardDetector *pDetector = nullptr;

	if (bPochetNameOnly)
	{
		pDetector = bRussian ? &m_PochetDetector : &m_PochetDetectorBY;
	}
	else
	{
		std::wstring firstWord = award.substr(0, award.find(' '));
		award.erase(0, award.find(' ') + 1);
		if (firstWord == (bRussian ? L"медалью" : L"медалём"))
			pDetector = bRussian ? &m_MedalDetector : &m_MedalDetectorBY;
		else if (firstWord == (bRussian ? L"орденом" : L"ордэнам"))
			pDetector = bRussian ? &m_OrdenDetector : &m_OrdenDetectorBY;
		else if (firstWord == (bRussian ? L"почетное" : L"ганаровае"))
		{
			award.erase(0, award.find(' ') + 1);
			pDetector = bRussian ? &m_PochetDetector : &m_PochetDetectorBY;
		}
	}

	if (pDetector)
	{
		for (auto iter = pDetector->begin(); iter != pDetector->end(); iter++)
		{
			if (iter->first == award.substr(0, iter->first.length()))
				return iter->second;
		}
	}

	return 0;
}

NounCase CDocumentParser::GetNounCaseByAwardId(int awardId)
{
	auto iter = m_Awards.find(awardId);
	if (iter != m_Awards.end())
	{
		if (iter->second.Type == 4 || iter->second.Type == 1)
			return Dative;
	}

	return Accusative;
}

bool CDocumentParser::IsMilitaryAward(int awardId)
{
	if (awardId >= 6 && awardId <= 8 || awardId >= 20 && awardId <= 22)
		return true;

	return false;
}

bool CDocumentParser::ProcessFile(bool bPDF, const std::wstring& fileName, std::vector<std::wstring>& arrLines)
{
	bool bResult = false;

	m_FileName = fileName;
	m_bPDF = bPDF;

	std::vector<std::wstring> searchTargetsRU = { L"медалью", L"орденом", L"почетное звание", L"почетные звания" };
	std::vector<std::wstring> searchTargetsBY = { L"медалём", L"ордэнам", L"ганаровае званн", L"ганаровыя званн" };
	std::vector<std::wstring> searchFinishBY = { };
	std::vector<std::wstring> searchFinishRU = { L"Президент Республики Беларусь" };
	std::vector<std::wstring> searchUkazStart = { L"УКАЗ ПР", L"УКАЗ ПР", L"УКАЗПР" };
	std::vector<std::wstring> searchActionsBY = { L"знагародзіць", L"знагародзiць", L"прысвоіць", L"Прысвоіць" };
	std::vector<std::wstring> searchActionsRU = { L"наградить", L"присвоить", L"Присвоить" };
	std::vector<std::wstring> searchGroup = { L"группу", L"группы", L"работников", L"работнiкаў", L"работнікаў", L"супрацоўнiкаў", L"сотрудникам" };
	std::vector<std::wstring> searchGroupEnd = { L"государственными", L"орденом", L"медалью", L"дзяржаўнымi", L"ордэнам", L"медалём", L"почетные", L"почетное", L"почетных" };

	bool bUkazStarted = false, bAwardStarted = false, bAwardTypeFound = false, bPossibleGroupStart = false, bMultiplePochet = false;
	std::wstring curAward;
	int awardId = 0;
	int nAwardedCount = 0;
	size_t posStart = 0, posEnd = 0;

	for (auto itrLine = arrLines.begin(); itrLine != arrLines.end(); itrLine++)
	{
		// new ukaze of president found
		if (FindTokens(*itrLine, searchUkazStart, posStart, posEnd))
		{
			UpdateAwardStat(awardId, curAward, nAwardedCount);
			bUkazStarted = true;
			bAwardStarted = bAwardTypeFound = bPossibleGroupStart = bMultiplePochet = false;
			m_UkazInfo.Clear();
			m_UkazInfo.RussianLang = ((*itrLine)[posEnd] == L'Е');
			curAward.clear();
			awardId = nAwardedCount = 0;
			continue;
		}

		if (!bUkazStarted)
			continue;

		std::wstring strLine = *itrLine;
		size_t posActionEnd = 0, posTargetStart = 0;

		if (0 == m_UkazInfo.Number)
		{
			size_t pos = strLine.find(L'№');
			if (pos != -1)
			{
				m_UkazInfo.Number = _wtoi(strLine.substr(pos + 1).c_str());

				pos = strLine.rfind(L'г', pos - 1);
				if (pos != -1)
				{
					m_UkazInfo.Date = ParseDate(strLine.substr(0, pos), m_UkazInfo.RussianLang);
					m_mapFileMonth[m_FileName] = m_UkazInfo.Date.isValid() ? m_UkazInfo.Date.month() : 0;
				}
			}
		}

		// process line break
		if (bPDF && strLine.length() > 5 && itrLine + 1 != arrLines.end() && IsWordDivision(strLine.back()))
		{
			size_t posNextWordEnd = (itrLine + 1)->find_first_of(L" :");
			if (posNextWordEnd != -1)
			{
				std::wstring nextWord = (itrLine + 1)->substr(0, posNextWordEnd);
				strLine.erase(strLine.end() - 1);
				strLine += nextWord;
				(itrLine + 1)->erase(0, posNextWordEnd + 1);
			}
		}

		// replace latin 'i' with belorussian
		if (!m_UkazInfo.RussianLang)
			std::replace(strLine.begin(), strLine.end(), L'i', L'і');

		// order about awarding found
		if (FindTokens(strLine, m_UkazInfo.RussianLang ? searchActionsRU : searchActionsBY, posStart, posEnd))
		{
			bAwardStarted = true;
			bAwardTypeFound = false;
			UpdateAwardStat(awardId, curAward, nAwardedCount);
			curAward.clear();
			awardId = nAwardedCount = 0;
			posActionEnd = posEnd;
		}

		// find info about group: text after the group token up to the end of
		// the sentence or up to the award type ("орденом", "медалью", ...)
		if (bUkazStarted && m_UkazInfo.Number && FindTokens(strLine, searchGroup, posStart, posEnd))
		{
			std::wstring groupInfo = posEnd < strLine.length() ? strLine.substr(posEnd + 1) : std::wstring();
			posEnd = groupInfo.find_first_of(L":\t\n\r.");
			if (posEnd != std::wstring::npos)
				groupInfo.erase(posEnd);
			if (FindTokens(groupInfo, searchGroupEnd, posStart, posEnd) && posStart > 0)
				groupInfo.erase(posStart - 1);

			AddGroupInfo(m_UkazInfo, m_FileName, groupInfo);
		}

		if (bAwardStarted)
		{
			// no more orders or medals further
			if (bAwardTypeFound && FindTokens(strLine, m_UkazInfo.RussianLang ? searchFinishRU : searchFinishBY, posStart, posEnd))
			{
				bUkazStarted = bAwardStarted = bAwardTypeFound = false;
				UpdateAwardStat(awardId, curAward, nAwardedCount);
				curAward.clear();
				awardId = nAwardedCount = 0;
				continue;
			}

			// find award type
			if (FindTokens(strLine, m_UkazInfo.RussianLang ? searchTargetsRU : searchTargetsBY, posStart, posEnd))
			{
				bAwardTypeFound = true;
				posTargetStart = posStart;
				UpdateAwardStat(awardId, curAward, nAwardedCount);

				awardId = nAwardedCount = 0;
				curAward = strLine.substr(posTargetStart);
				curAward.erase(curAward.find_last_not_of(L" :\t\n\r.") + 1);
				awardId = GetAwardId(curAward.c_str(), m_UkazInfo.RussianLang);

				// single awarding description
				if (posActionEnd > 0 && strLine.back() != L':')
				{
					std::wstring strAwardedInfo;
					if(posTargetStart - posActionEnd > 10)
						strAwardedInfo = strLine.substr(posActionEnd, posTargetStart - posActionEnd);
					else if (awardId > 0)
					{
						auto iter = m_Awards.find(awardId);
						if (iter != m_Awards.end())
						{
							const size_t nAwardLen = m_UkazInfo.RussianLang ? iter->second.Name.length() : iter->second.NameBY.length();
							if (posTargetStart + nAwardLen < strLine.length())
								strAwardedInfo = strLine.substr(posTargetStart + nAwardLen);
						}
					}

					if(!strAwardedInfo.empty())
						ExtractPersonInfo(curAward, strAwardedInfo);
				}

				if (strLine.back() == L':')
				{
					if (awardId)
						bPossibleGroupStart = true;
					else if (strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[2] : searchTargetsBY[2]) != -1 ||
						     strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[3] : searchTargetsBY[3]) != -1 )
					{
						bMultiplePochet = true;
					}
				}
			}

			if (bAwardTypeFound)
			{
				if (bMultiplePochet)
				{
					if (awardId <= 0)
					{
						awardId = GetAwardId(strLine.c_str(), m_UkazInfo.RussianLang, true);
						if (awardId > 0)
							continue;
					}
					else if (strLine.find(L"Заслуж") != -1 || strLine.find(L"Народны") != -1)
					{
						int newAwardId = GetAwardId(strLine.c_str(), m_UkazInfo.RussianLang, true);
						if (newAwardId > 0)
						{
							UpdateAwardStat(awardId, strLine, nAwardedCount);
							awardId = newAwardId;
							nAwardedCount = 0;
							continue;
						}
					}
				}
				bResult = true;
				if (awardId > 0)
				{
					// awarding list
					size_t pos = strLine.find_first_of(L"–-", 0);
					if (pos == -1)
						continue;

					PersonInfo info;
					info.FileName = m_FileName;
					info.Ukaz = m_UkazInfo;
					info.AwardId = awardId;

					if(pos == 0)
					{
						// new format of persons list in the docx/PDF:
						// name on the previous line, description on the next one
						if (itrLine == arrLines.begin() || itrLine + 1 == arrLines.end())
							continue;
						m_UkazInfo.ListOfPersons = true;
						nAwardedCount++;

						info.OriginalText = *(itrLine - 1) + *itrLine + *(itrLine + 1);
						info.Name = *(itrLine - 1);
						std::replace(info.Name.begin(), info.Name.end(), L'\v', L' ');
						info.Description = *(++itrLine);

						NormalizePersonInfo(info);
						m_Persons.push_back(info);
					}
					else if ((bPossibleGroupStart || bMultiplePochet) && strLine[pos] == L'–')
					{
						// old format of persons list in the docx
						m_UkazInfo.ListOfPersons = true;
						nAwardedCount++;

						info.OriginalText = strLine;
						info.Name = strLine.substr(0, pos);
						info.Description = strLine.substr(pos + 1);

						NormalizePersonInfo(info);
						m_Persons.push_back(info);
					}
				}
			}
		}
	}

	UpdateAwardStat(awardId, curAward, nAwardedCount);

	return bResult;
}

void CDocumentParser::UpdateAwardStat(int id, std::wstring& award, int nCount)
{
	if (award.empty())
		return;

	if(id <= 0)
		id = GetAwardId(award.c_str(), m_UkazInfo.RussianLang);

	bool bFound = false;
	for (AwardInfo& awardInfo : m_AwardsStat)
	{
		if (awardInfo.Id == id && (id != 0 || award == awardInfo.CustomName))
		{
			bFound = true;

			auto iter = awardInfo.CountByFiles.find(m_FileName);
			if (iter == awardInfo.CountByFiles.end())
				awardInfo.CountByFiles[m_FileName] = nCount ? nCount : 1;
			else
			{
				if (nCount)
					iter->second += nCount;
				else
					iter->second++;
			}

			awardInfo.TotalCount += nCount ? nCount : 1;
			//awardInfo.CountByMonth[m_UkazInfo.Date.GetMonth()] += (nCount ? nCount : 1);
			if (0 == nCount)
			{
				awardInfo.CountByFiles[m_FileName] |= kFlagNeedReview;
				//awardInfo.CountByMonth[m_UkazInfo.Date.GetMonth()] |= kFlagNeedReview;
				awardInfo.NeedReview = true;
			}
		}
	}

	if (!bFound)
	{
		AwardInfo awardInfo;
		awardInfo.Id = id;
		if (0 == id)
		{
			awardInfo.NeedReview = true;
			awardInfo.CustomName = award;
		}
		else
		{
			awardInfo.NeedReview = nCount ? false : true;
		}
		awardInfo.TotalCount = nCount ? nCount : 1;
		awardInfo.CountByFiles[m_FileName] = nCount ? nCount : 1;
		//awardInfo.CountByMonth[m_UkazInfo.Date.GetMonth()] += (nCount ? nCount : 1);
		if (!nCount)
		{
			awardInfo.CountByFiles[m_FileName] |= kFlagNeedReview;
			//awardInfo.CountByMonth[m_UkazInfo.Date.GetMonth()] |= kFlagNeedReview;
			awardInfo.NeedReview = true;
		}
		m_AwardsStat.push_back(awardInfo);
	}
}

void CDocumentParser::ExtractPersonInfo(std::wstring& curAward, std::wstring& strAwardedInfo)
{
	if (strAwardedInfo.empty())
		return;

	int awardId = GetAwardId(curAward, m_UkazInfo.RussianLang);
	if (awardId <= 0)
		return;

	NounCase nounCase = GetNounCaseByAwardId(awardId);

	bool bPosthumous = false;
	size_t posPost = strAwardedInfo.find(m_UkazInfo.RussianLang? L"(посмертно)" : L"(пасмяротна)");
	if (posPost != -1)
	{
		bPosthumous = true;
		strAwardedInfo.erase(posPost, strAwardedInfo.length() - posPost);
	}

	StrTrim(strAwardedInfo);

	// check for alias
	bool bHasAlias = false;
	size_t posBracket = strAwardedInfo.rfind(L'(');
	if (posBracket != -1 && strAwardedInfo.rfind(L')') > strAwardedInfo.length() - 10)
		bHasAlias = true;

	// lookup name parts
	size_t pos1 = bHasAlias ? strAwardedInfo.rfind(' ', posBracket-2) : strAwardedInfo.rfind(' ');
	size_t pos2 = strAwardedInfo.rfind(' ', pos1 - 1);
	if (pos1 == -1 || pos2 == -1)
		return;

	if (!IsCapital(strAwardedInfo[pos1 + 1]) || !IsCapital(strAwardedInfo[pos2 + 1]))
		return;

	size_t sep = pos2;

	if (m_NameConverter.IsMiddleName(strAwardedInfo.substr(pos1), m_UkazInfo.RussianLang? LangRU : LangBY, nounCase))
	{
		size_t pos3 = strAwardedInfo.rfind(' ', pos2 - 1);
		if (pos3 != -1 && IsCapital(strAwardedInfo[pos3 + 1]))
			sep = pos3;
	}


	PersonInfo info;
	info.Ukaz = m_UkazInfo;
	info.FileName = m_FileName;
	info.AwardId = awardId;
	info.OriginalText = strAwardedInfo;
	info.Name = strAwardedInfo.substr(sep + 1);
	info.Description = strAwardedInfo.substr(0, sep);
	info.Posthumous = bPosthumous;

	NormalizePersonInfo(info);
	m_Persons.push_back(info);
}

bool CDocumentParser::SpltPersonInfo(PersonInfo& person)
{
	PersonInfo person2;

	person2.AwardId = person.AwardId;
	person2.OriginalText = person.OriginalText;
	person2.FileName = person.FileName;
	person2.Ukaz = person.Ukaz;

	bool bSameProfession = false;
	NounCase nounCase = GetNounCaseByAwardId(person.AwardId);

	size_t posNextPerson = person.Description.rfind(person.Ukaz.RussianLang? L" и " : L" i ");
	if (posNextPerson == -1)
	{
		posNextPerson = person.Description.rfind(person.Ukaz.RussianLang ? L" и" : L" і");
		if (posNextPerson == -1)
			return false;

		bSameProfession = true;
	}

	std::wstring strAwardedInfo = person.Description.substr(0, posNextPerson);

	size_t pos1 = strAwardedInfo.rfind(' ');
	size_t pos2 = strAwardedInfo.rfind(' ', pos1 - 1);
	if (pos1 == -1 || pos2 == -1)
		return false;

	if (!IsCapital(strAwardedInfo[pos1 + 1]) || !IsCapital(strAwardedInfo[pos2 + 1]))
		return false;

	size_t sep = pos2;
	size_t pos3 = strAwardedInfo.rfind(' ', pos2 - 1);
	if (pos3 != -1 && IsCapital(strAwardedInfo[pos3 + 1]))
		sep = pos3;

	person2.Name = strAwardedInfo.substr(sep + 1);;
	person2.Description = strAwardedInfo.substr(0, sep);

	if (bSameProfession)
	{
		size_t posNext = person2.Description.find(L' ');
		std::wstring strFirstWord = person2.Description.substr(0, posNext);

		static RulesList rulesPlural = { { SexAny, LangAny, L"ам", L"ов", L"--"} };
		if (CNameConverter::ApplyRules(strFirstWord, rulesPlural, SexAny, person.Ukaz.RussianLang ? LangRU : LangBY, nounCase, false))
		{
			person2.Description.erase(0, posNext);
			person2.Description.insert(0, strFirstWord);
		}

		person.Description = person2.Description;
		person.Flags |= PersonInfo::NF_PROFESSION;
		person2.Flags |= PersonInfo::NF_PROFESSION;
	}
	else
	{
		person.Description.erase(0, posNextPerson + 3);
		if (m_ProfessionConverter.ConvertProfession(person.Description, person.Ukaz.RussianLang ? LangRU : LangBY, nounCase))
			person.Flags |= PersonInfo::NF_PROFESSION;
	}

	NormalizePersonInfo(person2);
	m_Persons.push_back(person2);

	return true;
}

void CDocumentParser::NormalizePersonInfo(PersonInfo& person)
{
	wchar_t altName[255] = L"";
	int iLen = 255;

	StrTrim(person.Name);
	StrTrim(person.Description);
	std::replace(person.Description.begin(), person.Description.end(), L' ', L' ');

	/*if (m_PadegDll.IsAvailable() && m_UkazInfo.RussianLang)
		m_PadegDll.GetNominativePadeg(person.Name.c_str(), (wchar_t*)altName, iLen);*/

	NounCase nounCase = GetNounCaseByAwardId(person.AwardId);

	LangType lang = person.Ukaz.RussianLang ? LangRU : LangBY;
	person.SourceName = person.Name;
	person.SourceCase = nounCase;
	bool bRes = m_NameConverter.NormalizeName(person.Name, lang, nounCase);
	if (*altName)
	{
		if (0 == person.Name.compare(altName))
			person.Flags |= PersonInfo::NF_NAME;
		else
			person.Name += L" (" + std::wstring(altName) + L")";
	}
	else if(bRes)
		person.Flags |= PersonInfo::NF_NAME;

	if (ExtractMilitaryRank(person, lang, nounCase))
		person.Flags |= PersonInfo::NF_RANK;

	// "наладчык – брыгадзір ...": the second profession after a dash, but not a
	// dash inside quotes («Дзіцячы сад – сярэдняя школа»): there the next quote
	// is a closing one. Nested quotes are closed once («НВА «Цэнтр»), so the
	// quotes before the dash are not counted.
	// every part after a dash is a profession of its own: "намесніка начальніка –
	// начальніка аддзела – урача-фтызіятра"
	std::vector<std::wstring> professions;
	std::wstring rest = person.Description;
	for (;;)
	{
		size_t posSep = rest.find(L" – ");
		while (posSep != std::wstring::npos)
		{
			const size_t posQuote = rest.find_first_of(L"«»", posSep);
			if (posQuote == std::wstring::npos || rest[posQuote] == L'«')
				break;
			posSep = rest.find(L" – ", posSep + 3);
		}
		if (posSep == std::wstring::npos)
			break;
		professions.push_back(rest.substr(0, posSep));
		rest.erase(0, posSep + 3);
	}
	professions.push_back(rest);

	for (size_t i = 0; i < professions.size(); i++)
	{
		if (m_ProfessionConverter.ConvertProfession(professions[i], lang, nounCase) && i == 0)
			person.Flags |= PersonInfo::NF_PROFESSION;
	}
	person.Description = professions[0];
	for (size_t i = 1; i < professions.size(); i++)
		person.Description += L" – " + professions[i];
}

bool CDocumentParser::ExtractMilitaryRank(PersonInfo& person, LangType lang, NounCase nounCase)
{
	bool bPosthumous = false;
	size_t posPost = person.Description.find(lang == LangRU ? L"(посмертно)" : L"(пасмяротна)");
	if (posPost != -1)
	{
		person.Posthumous = true;
		person.Description.erase(posPost, person.Description.length() - posPost);
	}

	size_t posComma = person.Description.rfind(L',');
	size_t posStart = 0;
	if (posComma != -1)
		posStart = person.Description.find_first_not_of(L" ,", posComma);

	if (posStart == -1 || posStart >= person.Description.length())
		return false;

	std::wstring strCheck = person.Description.substr(posStart);
	std::replace(strCheck.begin(), strCheck.end(), L'i', L'і');

	// check longer ranks first: "старшему прапорщику" contains "прапорщику",
	// "генерал-майору" contains "майору" etc.
	const auto& allRanks = (lang == LangBY ? MilitatyRanksBY : MilitatyRanksRU);
	std::vector<const RankInfo*> ranks;
	ranks.reserve(allRanks.size());
	for (const auto& rank : allRanks)
		ranks.push_back(&rank);
	std::stable_sort(ranks.begin(), ranks.end(), [nounCase](const RankInfo* a, const RankInfo* b) {
		const size_t lenA = (nounCase == Dative ? a->Dative : a->Accusative).length();
		const size_t lenB = (nounCase == Dative ? b->Dative : b->Accusative).length();
		return lenA > lenB;
	});

	for (const RankInfo* pRank : ranks)
	{
		const RankInfo& rank = *pRank;
		const std::wstring& rankOriginal = (nounCase == Dative ? rank.Dative : rank.Accusative);
		if (rankOriginal.length() > strCheck.length())
			continue;

		// a rank is followed by a few words at most ("міліцыі", "унутранай службы ў адстаўцы",
		// "медыцынскай службы запасу"): "капітана-механіка", "капітана – зменнага
		// механіка цеплахода", "капітана нацыянальнай каманды па тэнісу" are positions
		{
			const size_t posRank = strCheck.find(rankOriginal);
			if (posRank != std::wstring::npos)
			{
				const std::wstring rest = strCheck.substr(posRank + rankOriginal.length());
				if (!rest.empty() && (rest[0] == L'-' || rest.compare(0, 2, L" –") == 0
					|| std::count(rest.begin(), rest.end(), L' ') > 5))
					continue;
			}
		}

		// "саветнік" is a rank only as "саветнік юстыцыі" or with a class
		// ("дзяржаўны саветнік ... 3 класа"); "саветнік упраўлення ..." is a position
		if (rank.Nominative.find(L"саветнік") != std::wstring::npos)
		{
			const size_t posRank = strCheck.find(rankOriginal);
			if (posRank == std::wstring::npos)
				continue;
			const std::wstring rest = strCheck.substr(posRank + rankOriginal.length());
			if (rest.compare(0, 8, L" юстыцыі") != 0 && rest.find(L"класа") == std::wstring::npos)
				continue;
		}

		if (posComma != -1)
		{
			if (!wcsncmp(strCheck.c_str(), rankOriginal.c_str(), rankOriginal.length()))
			{
				person.MilitaryRank = rank.Nominative + strCheck.substr(rankOriginal.length());
				StrTrim(person.MilitaryRank);
				person.Description.erase(posComma, person.Description.length() - posComma);
				return true;
			}
		}
		else
		{
			posStart = strCheck.find(rankOriginal.c_str());
			if (posStart != -1)
			{
				person.MilitaryRank = rank.Nominative + strCheck.substr(posStart + rankOriginal.length());
				StrTrim(person.MilitaryRank);
				person.Description.erase(posStart, person.Description.length() - posStart);
				StrTrim(person.Description);
				return true;
			}
		}
	}

	return false;
}

std::wstring CDocumentParser::GetPersonDescription(size_t nIndex)
{
	if (nIndex >= m_Persons.size())
		return L"";

	return GetPersonDescription(m_Persons[nIndex], true);
}

std::wstring CDocumentParser::GetPersonDescription(PersonInfo& person, bool bGetRank)
{
	static RulesList rulesPlural = {
	{ SexAny, LangAny, L"ам", L"ов", L"--"}
	};


	std::wstring strDesc = person.Description.c_str();
	std::wstring strGroupInfo;
	if (GetGroupInfo(person.Ukaz, strGroupInfo))
	{
		if (!strGroupInfo.empty())
		{
			StrTrim(strGroupInfo);
			if(!strDesc.empty())
				strDesc += L' ';
			strDesc += strGroupInfo;
		}
	}

	if (!person.MilitaryRank.empty() && bGetRank)
	{
		if (!strDesc.empty())
			strDesc += L", ";
		strDesc += person.MilitaryRank;
	}

	return strDesc;
}

/*bool CDocumentParser::TranslateRank(std::wstring& strText,std::wstring& strResult)
{
	bool bResult = false;
	size_t posEnd = -1;
	strResult = strText;

	for (size_t i = 0; i < MilitatyRanksBY.size(); i++)
	{
		const auto& rank = MilitatyRanksBY[i];
		if (rank.Converted.length() > strText.length())
			continue;

		if (!wcsncmp(strText.c_str(), rank.Converted.c_str(), rank.Converted.length()) && i < MilitatyRanksRU.size())
		{
			strResult.erase(0, rank.Converted.length());
			strResult.insert(0, MilitatyRanksRU[i].Converted.c_str());
			posEnd = MilitatyRanksRU[i].Converted.length() + 1;
			bResult = true;
			break;
		}
	}

	if (bResult && posEnd < strResult.length() - 5)
	{
		std::wstring sWord = CProfessionConverter::GetNextWord(strResult, posEnd);
		if (sWord == L"міліцыі")
		{
			strResult.erase(posEnd, sWord.length());
			strResult.insert(posEnd, L"милиции");
		}
	}

	return bResult;
}*/

// decree numbers restart every year, so a decree is identified by number and date
void CDocumentParser::AddGroupInfo(const UkazInfo& ukaz, const std::wstring& fileName, const std::wstring& groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->UkazNumber == ukaz.Number && iter->UkazDate == ukaz.Date)
		{
			iter->Info = groupInfo;
			return;
		}
	}

	GroupInfo info;
	info.FileName = fileName;
	info.UkazNumber = ukaz.Number;
	info.UkazDate = ukaz.Date;
	info.Info = groupInfo;
	info.Checked = false;
	m_GroupsInfo.push_back(info);
}

bool CDocumentParser::GetGroupInfo(const UkazInfo& ukaz, std::wstring& groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->UkazNumber == ukaz.Number && iter->UkazDate == ukaz.Date)
		{
			groupInfo = iter->Info;
			return iter->Checked;
		}
	}

	return false;
}

void  CDocumentParser::CheckGroupInfo(const wchar_t* fileName, bool bCheck)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->FileName == fileName)
		{
			iter->Checked = bCheck;
			break;
		}
	}
}

void  CDocumentParser::UpdateGroupInfo(const wchar_t* fileName, const wchar_t* groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->FileName == fileName)
		{
			iter->Info = groupInfo;
			break;
		}
	}
}

bool CDocumentParser::GetGroupInfo(std::wstring& fileName, GroupInfo& groupInfo)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->FileName == fileName)
		{
			groupInfo = *iter;
			return true;
		}
	}

	return false;
}