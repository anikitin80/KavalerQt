#include "DocumentParser.h"
#include "DecreeLines.h"
#include "GroupText.h"
#include "PersonText.h"
#include "RankConverter.h"
#include "TextUtil.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

#include <QString>

using namespace decree;

void CDocumentParser::Initialize(std::map<long, DecreeAward>& awards)
{
	m_ProfessionConverter.Initialize();

	m_Awards = awards;
	m_AwardDetector.Initialize(m_Awards);

	ResetData();
}

void CDocumentParser::ResetData()
{
	m_AwardsStat.clear();
	m_Persons.clear();
	m_mapFileMonth.clear();
	m_GroupsInfo.clear();
	m_UkazInfo.Clear();
	m_GroupActive = m_GroupWholeDecree = false;
}


int CDocumentParser::GetAwardId(std::wstring award, bool bRussian, bool bPochetNameOnly, size_t* phraseLength)
{
	return m_AwardDetector.Find(std::move(award), bRussian, bPochetNameOnly, phraseLength);
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

namespace {

// слова указа; в белорусских строках «і» кириллическая (латинскую «i» уже заменили)
const std::vector<std::wstring> searchTargetsRU = { L"медалью", L"орденом", L"почетное звание", L"почетные звания" };
const std::vector<std::wstring> searchTargetsBY = { L"медалём", L"ордэнам", L"ганаровае званн", L"ганаровыя званн" };
const std::vector<std::wstring> searchFinishBY = { L"Прэзідэнт Рэспублікі Беларусь" };
const std::vector<std::wstring> searchFinishRU = { L"Президент Республики Беларусь" };
const std::vector<std::wstring> searchUkazStart = { L"УКАЗ ПР", L"УКАЗ ПР", L"УКАЗПР" };
const std::vector<std::wstring> searchActionsBY = { L"знагародзіць", L"прысвоіць", L"Прысвоіць" };
const std::vector<std::wstring> searchActionsRU = { L"наградить", L"Наградить", L"присвоить", L"Присвоить" };
const std::vector<std::wstring> searchGroup = { L"группу", L"группы", L"работников", L"работникам", L"работнікаў", L"супрацоўнікаў",
	L"супрацоўнікам", L"сотрудникам", L"сотрудников", L"артистов", L"артистам", L"артыстаў", L"артыстам",
	L"ваеннаслужачых", L"военнослужащих" };
const std::vector<std::wstring> searchGroupEnd = { L"государственными", L"орденом", L"медалью", L"дзяржаўнымі", L"ордэнам", L"медалём", L"почетные", L"почетное", L"почетных",
	L"почетного", L"ганаровага", L"ганаровае", L"ганаровых", L"ганаровыя" };

} // namespace

// «15 мая 1997 г.№ 282»: номер после «№», дата до «г»
void CDocumentParser::ParseDecreeHeader(const std::wstring& strLine)
{
	size_t pos = strLine.find(L'№');
	if (pos == std::wstring::npos)
		return;
	m_UkazInfo.Number = int(std::wcstol(strLine.substr(pos + 1).c_str(), nullptr, 10));

	pos = strLine.rfind(L'г', pos - 1);
	if (pos != std::wstring::npos)
	{
		m_UkazInfo.Date = ParseDate(strLine.substr(0, pos), m_UkazInfo.RussianLang);
		m_mapFileMonth[m_FileName] = m_UkazInfo.Date.isValid() ? m_UkazInfo.Date.month() : 0;
	}
}

// раздел списка («па Брэсцкай вобласці», «па г. Мінску»): у людей городского
// раздела места в указе может не быть, это город («г. Мінск»)
void CDocumentParser::ParseSection(const std::wstring& strLine)
{
	std::wstring line = strLine;
	std::replace(line.begin(), line.end(), L' ', L' '); // "па г. Мінску"
	StrTrim(line);
	if (line.length() > 40 || line.find(L'–') != std::wstring::npos
		|| (line.compare(0, 3, L"па ") != 0 && line.compare(0, 3, L"по ") != 0))
		return;
	const std::wstring rest = line.substr(3);
	// "па г. Мінску", "па гораду Мінску", "по городу Минску", "по гор. Минску"
	std::size_t cityStart = std::wstring::npos;
	for (const wchar_t* word : { L"г. ", L"гор. ", L"гораду ", L"городу " })
	{
		const std::size_t length = std::wcslen(word);
		if (rest.compare(0, length, word) == 0)
			cityStart = length;
	}
	if (cityStart != std::wstring::npos && rest.length() > cityStart + 1
		&& rest.find(L' ', cityStart) == std::wstring::npos)
	{
		std::wstring city = rest.substr(cityStart); // дательный: «Мінску», «Минску»
		if (city.back() == L'у')
			city.pop_back();
		m_SectionCity = L"г. " + city;
	}
	else if (rest.find(L"вобласці") != std::wstring::npos || rest.find(L"области") != std::wstring::npos)
		m_SectionCity.clear();
}


// сведения о группе: текст после слова группы до конца предложения
// или до вида награды («орденом», «медалью», ...)
bool CDocumentParser::ParseGroupInfo(const std::wstring& strLine)
{
	size_t posStart = 0, posEnd = 0;
	if (!findLastToken(strLine, searchGroup, posStart, posEnd))
		return false;
	const std::wstring role = groupRole(strLine.substr(posStart, posEnd - posStart));
	std::wstring groupInfo = posEnd < strLine.length() ? strLine.substr(posEnd + 1) : std::wstring();
	posEnd = groupSentenceEnd(groupInfo);
	if (posEnd != std::wstring::npos)
		groupInfo.erase(posEnd);
	if (FindTokens(groupInfo, searchGroupEnd, posStart, posEnd) && posStart > 0)
		groupInfo.erase(posStart - 1);
	// цель после организации к ней не относится: ««Мінскі трактарны
	// завод» у развіццё трактарабудавання», «органаў пракуратуры ва ўмацаванне ...»
	for (const wchar_t* preposition : { L" у ", L" ў ", L" в ", L" ва ", L" во ", L" за " })
	{
		for (size_t pos = groupInfo.find(preposition); pos != std::wstring::npos; pos = groupInfo.find(preposition, pos + 1))
		{
			// не внутри кавычек: следующая кавычка открывающая или её нет
			const size_t posQuote = groupInfo.find_first_of(L"«»", pos);
			if (posQuote == std::wstring::npos || groupInfo[posQuote] == L'«')
			{
				groupInfo.erase(pos);
				break;
			}
		}
	}

	stripGroupHead(groupInfo);
	cutTrailingAssociation(groupInfo);
	const std::wstring other = cutOtherOrganisation(groupInfo);
	AddGroupInfo(m_UkazInfo, m_FileName, groupInfo, role, other);
	return !groupInfo.empty();
}

LangType CDocumentParser::lang() const
{
	return m_UkazInfo.RussianLang ? LangRU : LangBY;
}

// человек текущего файла и указа
PersonInfo CDocumentParser::newPerson(int awardId) const
{
	PersonInfo person;
	person.FileName = m_FileName;
	person.Ukaz = m_UkazInfo;
	person.AwardId = awardId;
	return person;
}

// второй человек той же строки: файл, указ, награда, группа и должность пункта — те же
PersonInfo CDocumentParser::samePlace(const PersonInfo& person)
{
	PersonInfo other;
	other.AwardId = person.AwardId;
	other.FileName = person.FileName;
	other.Ukaz = person.Ukaz;
	other.InGroup = person.InGroup;
	other.PointJob = person.PointJob;
	return other;
}

void CDocumentParser::addPerson(PersonInfo person, AwardState& award)
{
	award.count++;
	NormalizePersonInfo(person);
	m_Persons.push_back(std::move(person));
}

// читаемая награда кончилась: считаем её людей
void CDocumentParser::finishAward(AwardState& award)
{
	UpdateAwardStat(award.id, award.text, award.count);
	award.text.clear();
	award.id = award.count = 0;
}

// Строка с видом награды. После «узнагародзіць» в той же строке может стоять
// и сам человек («…Кашкана … ордэнам «…» (пасмяротна).»), а в пункте «1. Узнагародзіць:» —
// по предложению на награду. Истина, если строка целиком разобрана таким предложением.
bool CDocumentParser::readAwardLine(const std::wstring& strLine, size_t posTargetStart, bool actionLine,
	size_t posActionEnd, AwardState& award)
{
	finishAward(award);
	award.text = strLine.substr(posTargetStart);
	award.text.erase(award.text.find_last_not_of(L" :\t\n\r.") + 1);
	// длина фразы в указе, а не названия в каталоге:
	// «медалью «За отвагу»» длиннее, чем «Медаль "За отвагу"»
	size_t awardPhrase = 0;
	award.id = GetAwardId(award.text, m_UkazInfo.RussianLang, false, &awardPhrase);

	// пункт «1. Узнагародзіць:», а дальше по предложению на награду:
	// «ордэнам Пашаны – спартсменаў ... Пунько С.В. і Фядотаву І.А.;»,
	// «трэнера ... Казеку Мікалая Іванавіча ордэнам Айчыны III ступені;»
	const bool sentenceAfterAction = posActionEnd == 0 && !actionLine && award.id > 0
		&& (strLine.back() == L';' || strLine.back() == L'.');

	// одно награждение в строке
	if ((posActionEnd > 0 || sentenceAfterAction) && strLine.back() != L':')
	{
		std::wstring strAwardedInfo;
		// человек и звание до награды («...Кашкана ... орденом «...» (посмертно).»)
		const bool personBeforeAward = posTargetStart - posActionEnd > 10;
		if (personBeforeAward)
			strAwardedInfo = strLine.substr(posActionEnd, posTargetStart - posActionEnd);
		else if (award.id > 0 && awardPhrase > 0 && posTargetStart + awardPhrase < strLine.length())
			strAwardedInfo = strLine.substr(posTargetStart + awardPhrase);

		if (sentenceAfterAction)
		{
			const size_t first = strAwardedInfo.find_first_not_of(L"  –-");
			strAwardedInfo.erase(0, first == std::wstring::npos ? strAwardedInfo.size() : first);
			const size_t last = strAwardedInfo.find_last_not_of(L"  ;.");
			strAwardedInfo.erase(last == std::wstring::npos ? 0 : last + 1);
		}

		if (!strAwardedInfo.empty())
		{
			const size_t addedFrom = m_Persons.size();
			ExtractPersonInfo(award.text, strAwardedInfo);
			// награждённый одной строкой тоже человек награды: иначе она
			// считается пустой и в окне импорта горит красным
			award.count += int(m_Persons.size() - addedFrom);
			if (sentenceAfterAction && m_Persons.size() > addedFrom)
				return true;
			// пометка может стоять после названия награды, вне strAwardedInfo
			if (personBeforeAward && award.id > 0 && awardPhrase > 0 && m_Persons.size() > addedFrom
				&& posTargetStart + awardPhrase < strLine.length())
			{
				const std::wstring tail = strLine.substr(posTargetStart + awardPhrase);
				if (tail.find(posthumousMark(m_UkazInfo.RussianLang)) != std::wstring::npos)
				{
					for (size_t i = addedFrom; i < m_Persons.size(); ++i)
						m_Persons[i].Posthumous = true;
				}
			}
		}
	}

	if (strLine.back() == L':')
	{
		if (award.id)
			award.listAfterColon = true;
		else if (strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[2] : searchTargetsBY[2]) != std::wstring::npos ||
			     strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[3] : searchTargetsBY[3]) != std::wstring::npos)
			award.honoraryList = true;
	}
	return false;
}

// в списке почётных званий строка звания («ЗАСЛУЖЕННЫЙ СТРОИТЕЛЬ РЕСПУБЛИКИ БЕЛАРУСЬ»,
// указ 2515/1993) начинает следующее звание. Истина, если это была строка звания.
bool CDocumentParser::switchHonoraryTitle(std::wstring& strLine, AwardState& award)
{
	const std::wstring titleLine = sentenceCaseIfUpper(strLine);
	if (award.id <= 0)
	{
		award.id = GetAwardId(titleLine.c_str(), m_UkazInfo.RussianLang, true);
		return award.id > 0;
	}
	if (titleLine.find(L"Заслуж") == std::wstring::npos && titleLine.find(L"Народны") == std::wstring::npos)
		return false;
	const int newAwardId = GetAwardId(titleLine.c_str(), m_UkazInfo.RussianLang, true);
	if (newAwardId <= 0)
		return false;
	UpdateAwardStat(award.id, strLine, award.count);
	award.id = newAwardId;
	award.count = 0;
	return true;
}

// организация отдельной строкой списка, в том числе с тире в названии
// («ІНТЭГРАЛ» – кіруючая кампанія ...): это не человек и должность
bool CDocumentParser::readOrganisationLine(const std::wstring& strLine, AwardState& award)
{
	std::wstring orgLine = strLine;
	std::wstring orgName;
	if (!takeOrganisation(orgLine, orgName))
		return false;
	PersonInfo info = newPerson(award.id);
	assignOrganisation(info, std::move(orgName), orgLine);
	addPerson(std::move(info), award);
	return true;
}

// должность и список имён без тире под званием: «спартсменам-інструктарам …
// Андрыеўскаму Аляксандру Леанідавічу, Антоненку … і …;» (указы 262/2002, 494/2004)
bool CDocumentParser::readListWithoutDash(const std::wstring& strLine, bool actionLine, AwardState& award)
{
	std::wstring listLine = strLine;
	std::replace(listLine.begin(), listLine.end(), L' ', L' ');
	// StrTrim срезал бы и точку в конце, а по ней узнаётся предложение
	listLine.erase(0, listLine.find_first_not_of(L' '));
	listLine.erase(listLine.find_last_not_of(L' ') + 1);
	if (actionLine || listLine.empty() || (listLine.back() != L';' && listLine.back() != L'.')
		|| listLine.find(L", ") == std::wstring::npos || listLine.find(L'–') != std::wstring::npos)
		return false;

	while (!listLine.empty() && (listLine.back() == L';' || listLine.back() == L'.'))
		listLine.pop_back();
	const size_t addedFrom = m_Persons.size();
	std::wstring awardText = award.text;
	ExtractPersonInfo(awardText, listLine, award.id);
	if (m_Persons.size() == addedFrom)
		return false;
	// без списка в остатке это не перечень людей, а чья-то фраза
	const std::wstring& rest = m_Persons.back().Description;
	const std::wstring conjunction = m_UkazInfo.RussianLang ? L" и" : L" і";
	const bool listed = !rest.empty() && (rest.back() == L','
		|| (rest.size() > conjunction.size()
			&& rest.compare(rest.size() - conjunction.size(), conjunction.size(), conjunction) == 0));
	if (!listed)
	{
		m_Persons.resize(addedFrom);
		return false;
	}
	award.count += int(m_Persons.size() - addedFrom);
	return true;
}

// имя одно в строке, должность общая в пункте указа
// («…дэпутатаў Палаты прадстаўнікоў… ўзнагародзіць:», указ 569/2011);
// в новом формате за именем идёт строка с тире
void CDocumentParser::readBareName(const std::wstring& strLine, bool actionLine,
	const std::vector<std::wstring>& lines, size_t index, AwardState& award)
{
	std::wstring next = index + 1 < lines.size() ? lines[index + 1] : std::wstring();
	StrTrim(next);
	std::wstring bare = strLine;
	std::replace(bare.begin(), bare.end(), L' ', L' ');
	StrTrim(bare);
	if (actionLine || bare.empty() || (!next.empty() && (next[0] == L'–' || next[0] == L'-'))
		|| !nameWithoutJob(bare + L" –", bare.size() + 1)
		|| !m_NameConverter.IsMiddleName(bare.substr(bare.rfind(L' ') + 1), lang(), GetNounCaseByAwardId(award.id)))
		return;
	PersonInfo info = newPerson(award.id);
	info.Name = bare;
	addPerson(std::move(info), award);
}

// новый формат списка в docx: имя на предыдущей строке, должность на следующей
// (сама строка — тире); index переходит на строку должности
void CDocumentParser::readNameAndJobLines(const std::vector<std::wstring>& lines, size_t& index, AwardState& award)
{
	if (index == 0 || index + 1 == lines.size())
		return;
	PersonInfo info = newPerson(award.id);
	info.Name = lines[index - 1];
	std::replace(info.Name.begin(), info.Name.end(), L'\v', L' ');
	info.Description = lines[++index];
	addPerson(std::move(info), award);
}

// одно или несколько имён перед тире, строка награды без двоеточия
// («Антоненку А.У., Панамарова С.А. – членаў каманды …», указ 10/2014)
bool CDocumentParser::readNamesBeforeDash(const std::wstring& strLine, size_t dash, bool actionLine, AwardState& award)
{
	if (strLine[dash] != L'–' || actionLine)
		return false;
	const std::vector<std::wstring> names = namesBeforeDash(strLine.substr(0, dash), lang());
	if (names.empty())
		return false;
	std::wstring job = strLine.substr(dash + 1);
	std::replace(job.begin(), job.end(), L' ', L' ');
	StrTrim(job);
	while (!job.empty() && (job.back() == L';' || job.back() == L'.'))
		job.pop_back();
	if (names.size() > 1)
		singularFirstWord(job, lang());
	for (const std::wstring& name : names)
	{
		PersonInfo person = newPerson(award.id);
		person.Name = name;
		person.Description = job;
		addPerson(std::move(person), award);
	}
	return true;
}

// строка списка под наградой: организация, перечень без тире, одно имя,
// имя и должность через тире (в одной строке или на соседних)
void CDocumentParser::readListLine(const std::wstring& strLine, bool actionLine,
	const std::vector<std::wstring>& lines, size_t& index, AwardState& award)
{
	if (readOrganisationLine(strLine, award) || readListWithoutDash(strLine, actionLine, award))
		return;

	const size_t dash = strLine.find_first_of(L"–-", 0);
	if (dash == std::wstring::npos)
	{
		readBareName(strLine, actionLine, lines, index, award);
		return;
	}
	if (dash == 0)
	{
		readNameAndJobLines(lines, index, award);
		return;
	}
	if (readNamesBeforeDash(strLine, dash, actionLine, award))
		return;
	// старый формат списка в docx: «имя – должность». Строка с «узнагародзіць» /
	// «прысвоіць» уже разобрана как одно награждение: тире в ней — внутри должности
	if ((award.listAfterColon || award.honoraryList || nameWithoutJob(strLine, dash)) && strLine[dash] == L'–' && !actionLine)
	{
		PersonInfo info = newPerson(award.id);
		info.Name = strLine.substr(0, dash);
		info.Description = strLine.substr(dash + 1);
		addPerson(std::move(info), award);
	}
}

bool CDocumentParser::ProcessFile(const std::wstring& fileName, const std::vector<std::wstring>& arrLines)
{
	bool bResult = false;

	m_FileName = fileName;

	bool bUkazStarted = false, bAwardStarted = false, bAwardTypeFound = false;
	AwardState award;
	size_t posStart = 0, posEnd = 0;

	// люди, добавленные прошлыми строками, получают сведения о группе, действовавшие
	// на их строке: состояние меняется только в начале разбора строки
	size_t markedPersons = m_Persons.size();
	const size_t markedFrom = markedPersons;
	// должность пункта, действующая на строке, где человек стоит
	std::wstring pointJob;
	const auto markGroup = [&] {
		for (; markedPersons < m_Persons.size(); ++markedPersons)
		{
			m_Persons[markedPersons].InGroup = m_GroupActive;
			m_Persons[markedPersons].PointJob = pointJob;
		}
	};

	for (size_t index = 0; index < arrLines.size(); ++index)
	{
		markGroup();
		// найден новый указ президента
		if (FindTokens(arrLines[index], searchUkazStart, posStart, posEnd))
		{
			finishAward(award);
			m_GroupActive = m_GroupWholeDecree = false;
			bUkazStarted = true;
			bAwardStarted = bAwardTypeFound = false;
			award.listAfterColon = award.honoraryList = false;
			m_UkazInfo.Clear();
			m_UkazInfo.RussianLang = (arrLines[index][posEnd] == L'Е');
			m_SectionCity.clear();
			pointJob.clear();
			continue;
		}

		if (!bUkazStarted)
			continue;

		std::wstring strLine = arrLines[index];
		collapseRepeatedSpaces(strLine);
		size_t posActionEnd = 0;

		if (0 == m_UkazInfo.Number)
			ParseDecreeHeader(strLine);

		// латинскую «i» заменяем белорусской
		if (!m_UkazInfo.RussianLang)
			ToCyrillicI(strLine, false);

		// найден приказ о награждении
		const bool groupBeforeAction = !bAwardStarted;
		const bool actionLine = FindTokens(strLine, m_UkazInfo.RussianLang ? searchActionsRU : searchActionsBY, posStart, posEnd);
		if (actionLine)
		{
			bAwardStarted = true;
			bAwardTypeFound = false;
			finishAward(award);
			posActionEnd = posEnd;
		}

		// группа из пункта кончается на следующем пункте или «узнагародзіць» без группы
		if (m_GroupActive && !m_GroupWholeDecree && (actionLine || startsWithPointNumber(strLine)))
			m_GroupActive = false;
		if (m_UkazInfo.Number && ParseGroupInfo(strLine))
		{
			m_GroupActive = true;
			// заголовок указа («Аб узнагароджанні групы работнікаў ...») — на весь указ
			if (groupBeforeAction && !actionLine)
				m_GroupWholeDecree = true;
		}
		ParseSection(strLine);

		// новый пункт указа («2.») сбрасывает должность, подпункт («1.1.») — только если даёт свою
		const int level = pointLevel(strLine);
		if (level > 0 || actionLine)
		{
			const std::wstring job = jobFromPointLine(strLine, actionLine ? posActionEnd : std::wstring::npos, lang());
			if (!job.empty() || level == 1)
				pointJob = job;
		}

		if (!bAwardStarted)
			continue;

		// дальше орденов и медалей нет
		if (bAwardTypeFound && FindTokens(strLine, m_UkazInfo.RussianLang ? searchFinishRU : searchFinishBY, posStart, posEnd))
		{
			bUkazStarted = bAwardStarted = bAwardTypeFound = false;
			finishAward(award);
			continue;
		}

		// вид награды
		if (FindTokens(strLine, m_UkazInfo.RussianLang ? searchTargetsRU : searchTargetsBY, posStart, posEnd))
		{
			bAwardTypeFound = true;
			if (readAwardLine(strLine, posStart, actionLine, posActionEnd, award))
				continue;
		}

		if (!bAwardTypeFound)
			continue;
		if (award.honoraryList && switchHonoraryTitle(strLine, award))
			continue;
		bResult = true;
		if (award.id > 0)
			readListLine(strLine, actionLine, arrLines, index, award);
	}
	markGroup();

	finishAward(award);

	// «Матусевичу ... и директору совхоза»: второй человек в той же строке.
	// «Минского и Слуцкого» не режется: перед «и» нет имени.
	for (size_t i = 0; i < m_Persons.size(); ++i)
	{
		while (SpltPersonInfo(m_Persons[i], true))
			;
		peelListedPeople(m_Persons[i]);
	}
	fillMissingJobs(markedFrom);

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

			awardInfo.CountByFiles[m_FileName] += nCount ? nCount : 1; // новый файл начинается с 0
			awardInfo.TotalCount += nCount ? nCount : 1;
			if (0 == nCount)
			{
				awardInfo.CountByFiles[m_FileName] |= kFlagNeedReview;
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
		if (!nCount)
		{
			awardInfo.CountByFiles[m_FileName] |= kFlagNeedReview;
			awardInfo.NeedReview = true;
		}
		m_AwardsStat.push_back(awardInfo);
	}
}

void CDocumentParser::ExtractPersonInfo(std::wstring& curAward, std::wstring& strAwardedInfo, int knownAwardId)
{
	if (strAwardedInfo.empty())
		return;

	// в списке званий общая фраза «ганаровыя званні», звание уже известно
	int awardId = knownAwardId > 0 ? knownAwardId : GetAwardId(curAward, m_UkazInfo.RussianLang);
	if (awardId <= 0)
		return;

	NounCase nounCase = GetNounCaseByAwardId(awardId);

	const bool bPosthumous = cutPosthumous(strAwardedInfo, m_UkazInfo.RussianLang);

	const LangType lang = m_UkazInfo.RussianLang ? LangRU : LangBY;
	std::wstring name;
	bool organisation = false;
	if (!takePersonName(m_NameConverter, strAwardedInfo, lang, nounCase, name)) {
		if (!takeOrganisation(strAwardedInfo, name))
			return;
		organisation = true;
	}

	if (!organisation)
		singularSharedJob(strAwardedInfo, lang);

	PersonInfo info = newPerson(awardId);
	info.Posthumous = bPosthumous;
	if (organisation)
		assignOrganisation(info, std::move(name), strAwardedInfo);
	else {
		info.Name = std::move(name);
		info.Description = std::move(strAwardedInfo);
	}

	NormalizePersonInfo(info);
	m_Persons.push_back(info);
}

bool CDocumentParser::SpltPersonInfo(PersonInfo& person, bool requirePatronymic)
{
	PersonInfo person2 = samePlace(person);

	bool bSameProfession = false;
	NounCase nounCase = GetNounCaseByAwardId(person.AwardId);

	const wchar_t* spaced = person.Ukaz.RussianLang ? L" и " : L" і ";
	const wchar_t* closed = person.Ukaz.RussianLang ? L" и" : L" і";
	size_t posNextPerson = person.Description.rfind(spaced);
	if (posNextPerson == std::wstring::npos)
	{
		// « и» только в конце: иначе «имени» и «імя» режутся как союз
		const size_t length = std::wcslen(closed);
		if (person.Description.size() < length
			|| person.Description.compare(person.Description.size() - length, length, closed) != 0)
			return false;
		posNextPerson = person.Description.size() - length;
		bSameProfession = true;
	}

	std::wstring strAwardedInfo = person.Description.substr(0, posNextPerson);
	const LangType lang = person.Ukaz.RussianLang ? LangRU : LangBY;
	if (!takePersonName(m_NameConverter, strAwardedInfo, lang, nounCase, person2.Name))
		return false;
	if (requirePatronymic)
	{
		const size_t space = person2.Name.rfind(L' ');
		const std::wstring last = space == std::wstring::npos ? person2.Name : person2.Name.substr(space + 1);
		if (!m_NameConverter.IsMiddleName(last, lang, nounCase))
			return false;
	}
	person2.Description = std::move(strAwardedInfo);

	if (bSameProfession)
	{
		size_t posNext = person2.Description.find(L' ');
		std::wstring strFirstWord = person2.Description.substr(0, posNext);

		static const RulesList rulesPlural = { { SexAny, LangAny, L"ам", L"ов", L"--"} };
		if (CNameConverter::ApplyRules(strFirstWord, rulesPlural, SexAny, lang, nounCase, false))
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
		if (m_ProfessionConverter.ConvertProfession(person.Description, lang, nounCase))
			person.Flags |= PersonInfo::NF_PROFESSION;
	}

	NormalizePersonInfo(person2);
	m_Persons.push_back(person2);

	return true;
}

bool CDocumentParser::peelListedPeople(PersonInfo& person)
{
	if (person.Organization || person.Description.empty())
		return false;

	const LangType lang = person.Ukaz.RussianLang ? LangRU : LangBY;
	const NounCase nounCase = GetNounCaseByAwardId(person.AwardId);
	const wchar_t* conjunction = person.Ukaz.RussianLang ? L" и" : L" і";
	const size_t conjunctionLength = std::wcslen(conjunction);

	std::wstring text = person.Description;
	std::vector<std::wstring> names;
	for (;;)
	{
		std::wstring trial = text;
		StrTrim(trial);
		bool separator = false;
		while (!trial.empty() && (trial.back() == L',' || trial.back() == L';'))
		{
			trial.pop_back();
			StrTrim(trial);
			separator = true;
		}
		// « и» уже с пробелом, поэтому «имени» не отрезается
		if (trial.size() > conjunctionLength
			&& trial.compare(trial.size() - conjunctionLength, conjunctionLength, conjunction) == 0)
		{
			trial.erase(trial.size() - conjunctionLength);
			StrTrim(trial);
			separator = true;
		}

		std::wstring name;
		std::wstring rest = trial;
		if (!takePersonName(m_NameConverter, rest, lang, nounCase, name))
			break;
		const size_t space = name.rfind(L' ');
		const std::wstring last = space == std::wstring::npos ? name : name.substr(space + 1);
		if (!m_NameConverter.IsMiddleName(last, lang, nounCase))
			break;
		if (!separator)
		{
			const size_t pos = trial.rfind(name);
			if (pos == std::wstring::npos || pos == 0)
				break;
			size_t before = pos;
			while (before > 0 && trial[before - 1] == L' ')
				--before;
			const bool comma = before > 0 && (trial[before - 1] == L',' || trial[before - 1] == L';');
			if (!comma)
				break;
		}

		names.push_back(name);
		text = std::move(rest);
	}

	if (names.empty())
		return false;

	StrTrim(text);
	person.Description = text;
	// копии собираем отдельно: push_back сдвигает person, он ссылается внутрь m_Persons
	std::vector<PersonInfo> added;
	for (const std::wstring& name : names)
	{
		PersonInfo extra = samePlace(person);
		extra.Name = name;
		extra.Description = text;
		NormalizePersonInfo(extra);
		const auto same = [&](const PersonInfo& existing) {
			return existing.AwardId == extra.AwardId && existing.Name == extra.Name
				&& existing.Ukaz.Number == extra.Ukaz.Number && existing.Ukaz.Date == extra.Ukaz.Date;
		};
		bool exists = extra.Name == person.Name;
		for (const PersonInfo& existing : m_Persons)
			exists = exists || same(existing);
		for (const PersonInfo& existing : added)
			exists = exists || same(existing);
		if (!exists)
			added.push_back(std::move(extra));
	}
	for (PersonInfo& extra : added)
		m_Persons.push_back(std::move(extra));
	return true;
}

void CDocumentParser::fillMissingJobs(size_t from)
{
	for (size_t i = from; i < m_Persons.size(); ++i)
	{
		PersonInfo& person = m_Persons[i];
		if (person.Organization || !person.Description.empty())
			continue;

		// «2. Прысвоіць Алімбекавай Дзінары …» после пункта 1, где у неё есть должность (указ 77/2018)
		bool copied = false;
		for (size_t j = from; j < m_Persons.size() && !copied; ++j)
		{
			const PersonInfo& other = m_Persons[j];
			if (j == i || other.Description.empty() || other.Name != person.Name
				|| other.Ukaz.Number != person.Ukaz.Number || other.Ukaz.Date != person.Ukaz.Date)
				continue;
			person.Description = other.Description;
			if (person.MilitaryRank.empty())
				person.MilitaryRank = other.MilitaryRank;
			person.Flags |= other.Flags & (PersonInfo::NF_PROFESSION | PersonInfo::NF_RANK);
			copied = true;
		}
		if (copied || person.PointJob.empty())
			continue;

		// имя уже в именительном: нормализуем только должность. Она в строке пункта
		// в родительном («дэпутатаў» → «дэпутата») и при звании, где люди в дательном:
		// без награды падеж винительный, у одушевлённых он совпадает с родительным
		PersonInfo job;
		job.Ukaz = person.Ukaz;
		job.Description = person.PointJob;
		NormalizePersonInfo(job);
		person.Description = job.Description;
		if (person.MilitaryRank.empty())
			person.MilitaryRank = job.MilitaryRank;
		person.Flags |= job.Flags & (PersonInfo::NF_PROFESSION | PersonInfo::NF_RANK);
	}
}

void CDocumentParser::SetKnownWords(LangType lang, std::map<std::wstring, int> counts)
{
	(lang == LangRU ? m_KnownWordsRU : m_KnownWordsBY) = std::move(counts);
}


void CDocumentParser::JoinWrappedWords(std::wstring& text, LangType lang) const
{
	const auto& words = lang == LangRU ? m_KnownWordsRU : m_KnownWordsBY;
	if (words.empty())
		return;
	for (size_t i = 1; i + 1 < text.size(); ++i)
	{
		if (text[i] != L'-' || !isLowerCyrillic(text[i - 1]) || !isLowerCyrillic(text[i + 1]))
			continue;
		size_t begin = i;
		while (begin > 0 && isWordLetter(text[begin - 1]))
			--begin;
		size_t end = i + 1;
		while (end < text.size() && isWordLetter(text[end]))
			++end;
		if (i - begin < 2 || end - i - 1 < 2)
			continue;
		const std::wstring left = ToLower(text.substr(begin, i - begin));
		const std::wstring right = ToLower(text.substr(i + 1, end - i - 1));
		// каталог часто хранит перенос как в указе («ста-цыянарнага») и составные
		// слова с дефисом («агульна-адукацыйнай»): такое слово остаётся
		if (words.count(left + L"-" + right))
			continue;
		if (knownCount(words, left + right) > 0)
			text.erase(i, 1);
	}
}

void CDocumentParser::NormalizePersonInfo(PersonInfo& person)
{
	{
		const LangType textLang = person.Ukaz.RussianLang ? LangRU : LangBY;
		// мягкий перенос Word в тексте не виден
		person.Name.erase(std::remove(person.Name.begin(), person.Name.end(), L'\u00AD'), person.Name.end());
		person.Description.erase(std::remove(person.Description.begin(), person.Description.end(), L'\u00AD'), person.Description.end());
		toCyrillicLetters(person.Name, textLang);
		toCyrillicLetters(person.Description, textLang);
		toGuillemets(person.Description);
		JoinWrappedWords(person.Description, textLang);
	}
	StrTrim(person.Name);
	stripComradeTitle(person.Name);
	StrTrim(person.Name);
	collapseRepeatedSpaces(person.Name);
	std::replace(person.Name.begin(), person.Name.end(), L'\u00A0', L' ');
	StrTrim(person.Description);
	collapseRepeatedSpaces(person.Description);
	std::replace(person.Description.begin(), person.Description.end(), L'\u00A0', L' ');

	NounCase nounCase = GetNounCaseByAwardId(person.AwardId);

	LangType lang = person.Ukaz.RussianLang ? LangRU : LangBY;
	person.SourceName = person.Name;
	person.SourceCase = nounCase;
	// название организации не склоняется как фамилия и не ищется среди должностей
	if (person.Organization) {
		person.Name.clear();
		if (person.SourceName.empty())
			person.SourceName = person.Description;
		person.Flags |= PersonInfo::NF_NAME;
		if (!person.Description.empty())
			person.Flags |= PersonInfo::NF_PROFESSION;
		return;
	}
	splitGluedPatronymic(m_NameConverter, person.Name, lang, nounCase);
	bool surnameSure = true;
	if (m_NameConverter.NormalizeName(person.Name, lang, nounCase, &surnameSure, &person.SurnameSpellings))
		person.Flags |= PersonInfo::NF_NAME;
	if (!surnameSure)
		person.Flags |= PersonInfo::NF_SURNAME_UNSURE;
	if (lang == LangRU)
		latinRomanNumerals(person.Name);

	if (cutPosthumous(person.Description, lang == LangRU))
	{
		person.Posthumous = true;
		StrTrim(person.Description);
	}

	if (CRankConverter::Extract(person.Description, person.MilitaryRank, lang, nounCase))
		person.Flags |= PersonInfo::NF_RANK;

	// «наладчык – брыгадзір ...»: вторая должность после тире, но не
	// тире внутри кавычек («Дзіцячы сад – сярэдняя школа»): там следующая
	// кавычка закрывающая. Вложенные кавычки закрываются один раз («НВА «Цэнтр»),
	// кавычки до тире не считаются.
	// каждая часть после тире — своя должность: «намесніка начальніка –
	// начальніка аддзела – урача-фтызіятра»
	std::vector<std::wstring> professions;
	std::wstring rest = person.Description;
	for (;;)
	{
		size_t posSep = rest.find(L" – ");
		while (posSep != std::wstring::npos && InsideQuotes(rest, posSep))
			posSep = rest.find(L" – ", posSep + 3);
		if (posSep == std::wstring::npos)
			break;
		professions.push_back(rest.substr(0, posSep));
		rest.erase(0, posSep + 3);
	}
	professions.push_back(rest);

	for (size_t i = 0; i < professions.size(); i++)
	{
		if (m_ProfessionConverter.ConvertProfession(professions[i], lang, nounCase, i == 0) && i == 0)
			person.Flags |= PersonInfo::NF_PROFESSION;
	}
	person.Description = professions[0];
	for (size_t i = 1; i < professions.size(); i++)
		person.Description += L" – " + professions[i];

	// у домохозяйки в городском разделе списка места в указе нет:
	// «па г. Мінску ... хатнюю гаспадыню» → «хатняя гаспадыня, г. Мінск»
	if (!m_SectionCity.empty())
	{
		const std::wstring description = ToLower(person.Description);
		if (description == L"хатняя гаспадыня" || description == L"домохозяйка")
			person.Description += L", " + m_SectionCity;
	}
}

std::wstring CDocumentParser::GetPersonDescription(size_t nIndex)
{
	if (nIndex >= m_Persons.size())
		return L"";

	return GetPersonDescription(m_Persons[nIndex], true);
}

std::wstring CDocumentParser::GetPersonDescription(PersonInfo& person, bool bGetRank)
{
	std::wstring strDesc = person.Description.c_str();
	std::wstring strGroupInfo, role, other;
	// сведения о группе — только людям того пункта указа, где они стоят
	if (person.InGroup && GetGroupInfo(person.Ukaz, strGroupInfo, role, other))
	{
		StrTrim(strGroupInfo);
		// «старшину милиции» при «группе работников ...»: в каталоге «работник ...»
		if (strDesc.empty() && !role.empty())
		{
			strDesc = role;
			if (!strGroupInfo.empty())
				strDesc += L" " + strGroupInfo;
		}
		// вторая организация списка уже стоит в должности
		else if (!other.empty() && strDesc.find(other) != std::wstring::npos)
			;
		// второй раз не добавляем: «дырэктар ААТ «Белкард»» уже называет организацию
		else if (!strGroupInfo.empty() && strDesc.find(strGroupInfo) == std::wstring::npos)
		{
			// «заведующий кафедрой, профессор» — институт стоит перед степенью
			const size_t comma = academicTitleComma(strDesc);
			if (comma != std::wstring::npos)
				strDesc.insert(comma, L" " + strGroupInfo);
			else
			{
				if (!strDesc.empty())
					strDesc += L' ';
				strDesc += strGroupInfo;
			}
		}
	}

	if (!person.MilitaryRank.empty() && bGetRank)
	{
		if (!strDesc.empty())
			strDesc += L", ";
		strDesc += person.MilitaryRank;
	}

	// \u0440\u0430\u0437\u0440\u044B\u0432 \u0441\u0442\u0440\u043E\u043A\u0438 \u0432\u043D\u0443\u0442\u0440\u0438 \u0430\u0431\u0437\u0430\u0446\u0430 (\u00AB\u043A\u0430\u0434\u0440\u0430\u045E<w:br/>\u0456 \u0456\u0434\u044D\u0430\u043B\u0430\u0433\u0456\u0447\u043D\u0430\u0439\u00BB) \u2014 \u0442\u043E\u0436\u0435 \u043F\u0440\u043E\u0431\u0435\u043B
	for (wchar_t& c : strDesc)
		if (c == L'\u00A0' || c == L'\n' || c == L'\r' || c == L'\t' || c == L'\v')
			c = L' ';
	collapseRepeatedSpaces(strDesc);
	return strDesc;
}

// номера указов каждый год с начала, указ узнаём по номеру и дате
void CDocumentParser::AddGroupInfo(const UkazInfo& ukaz, const std::wstring& fileName, const std::wstring& groupInfo,
	const std::wstring& role, const std::wstring& other)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->UkazNumber == ukaz.Number && iter->UkazDate == ukaz.Date)
		{
			// заголовок уже дал организацию, следующая строка лишь повторяет её
			// с объединением и целью: «завода» и «завода производственного объединения ...»
			const bool already = !iter->Info.empty() && groupInfo.compare(0, iter->Info.size(), iter->Info) == 0
				&& (groupInfo.size() == iter->Info.size() || groupInfo[iter->Info.size()] == L' ');
			if (already)
				return;
			iter->Info = groupInfo;
			iter->Role = role;
			iter->Other = other;
			return;
		}
	}

	GroupInfo info;
	info.FileName = fileName;
	info.UkazNumber = ukaz.Number;
	info.UkazDate = ukaz.Date;
	info.Info = groupInfo;
	info.Role = role;
	info.Other = other;
	info.Checked = false;
	m_GroupsInfo.push_back(info);
}

bool CDocumentParser::GetGroupInfo(const UkazInfo& ukaz, std::wstring& groupInfo, std::wstring& role, std::wstring& other)
{
	for (auto iter = m_GroupsInfo.begin(); iter != m_GroupsInfo.end(); iter++)
	{
		if (iter->UkazNumber == ukaz.Number && iter->UkazDate == ukaz.Date)
		{
			groupInfo = iter->Info;
			role = iter->Role;
			other = iter->Other;
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