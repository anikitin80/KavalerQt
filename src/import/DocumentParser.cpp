#include "DocumentParser.h"
#include "RankConverter.h"
#include "TextUtil.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <cwctype>

#include <QString>

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
			// страну отрезаем вместе с пробелом перед ней
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

	// старые названия, только если награда есть в базе
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
	m_GroupActive = m_GroupWholeDecree = false;
}

namespace {

// текст награды для сравнения с названиями каталога и место каждой
// оставленной буквы в строке указа. Кавычки убираются; «І» в степени
// («ІІІ степени») становится латинской «I»; неразрывный пробел — обычным.
struct AwardText
{
	std::wstring text;
	std::vector<size_t> origin;
};

AwardText normalizeAward(const std::wstring& award)
{
	AwardText out;
	out.text.reserve(award.size());
	out.origin.reserve(award.size());
	for (size_t i = 0; i < award.size(); ++i) {
		const wchar_t c = award[i];
		if (c == L'«' || c == L'»' || c == L'"')
			continue;
		out.origin.push_back(i);
		if (c == L'І')
			out.text.push_back(L'I');
		else if (c == L'\u00A0')
			out.text.push_back(L' ');
		else
			out.text.push_back(c);
	}
	return out;
}

bool atWordEnd(const std::wstring& text, size_t pos)
{
	if (pos >= text.size())
		return true;
	const wchar_t c = text[pos];
	if ((c >= L'А' && c <= L'я') || c == L'Ё' || c == L'ё' || c == L'І' || c == L'і' || c == L'Ў' || c == L'ў')
		return false;
	if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9'))
		return false;
	return true;
}

// индекс за последним символом фразы. Закрывающая кавычка стоит после
// последней буквы и в сравнение не входила.
size_t phraseEnd(const std::wstring& award, const std::vector<size_t>& origin, size_t normEnd)
{
	if (normEnd == 0 || normEnd > origin.size())
		return 0;
	size_t end = origin[normEnd - 1] + 1;
	while (end < award.size() && (award[end] == L'«' || award[end] == L'»' || award[end] == L'"'))
		++end;
	return end;
}

} // namespace

int CDocumentParser::GetAwardId(std::wstring award, bool bRussian, bool bPochetNameOnly, size_t* phraseLength)
{
	if (phraseLength)
		*phraseLength = 0;

	const AwardText norm = normalizeAward(award);
	const std::wstring& text = norm.text;

	AwardDetector* detector = nullptr;
	size_t nameAt = 0;
	bool honorTitle = bPochetNameOnly;
	if (bPochetNameOnly) {
		detector = bRussian ? &m_PochetDetector : &m_PochetDetectorBY;
	} else {
		const size_t space = text.find(L' ');
		const std::wstring firstWord = text.substr(0, space);
		nameAt = space == std::wstring::npos ? text.size() : space + 1;
		if (firstWord == (bRussian ? L"медалью" : L"медалём"))
			detector = bRussian ? &m_MedalDetector : &m_MedalDetectorBY;
		else if (firstWord == (bRussian ? L"орденом" : L"ордэнам"))
			detector = bRussian ? &m_OrdenDetector : &m_OrdenDetectorBY;
		else if (firstWord == (bRussian ? L"почетное" : L"ганаровае")) {
			honorTitle = true;
			const size_t space2 = text.find(L' ', nameAt);
			nameAt = space2 == std::wstring::npos ? text.size() : space2 + 1;
			detector = bRussian ? &m_PochetDetector : &m_PochetDetectorBY;
		}
	}

	int bestId = 0;
	size_t bestNormEnd = nameAt;
	size_t bestLength = 0;
	if (detector) {
		for (const auto& item : *detector) {
			const std::wstring& key = item.first;
			if (key.size() <= bestLength || nameAt + key.size() > text.size())
				continue;
			if (text.compare(nameAt, key.size(), key) != 0)
				continue;
			bestLength = key.size();
			bestNormEnd = nameAt + key.size();
			bestId = item.second;
		}
		// опечатка в последней букве названия: «За працоўныя заслуг» (указ 687/2001)
		if (bestId == 0) {
			for (const auto& item : *detector) {
				const std::wstring& key = item.first;
				const size_t length = key.size() - 1;
				if (key.size() < 12 || length <= bestLength || nameAt + length > text.size())
					continue;
				if (text.compare(nameAt, length, key, 0, length) != 0 || !atWordEnd(text, nameAt + length))
					continue;
				bestLength = length;
				bestNormEnd = nameAt + length;
				bestId = item.second;
			}
		}
	}
	if (bestId == 0)
		return 0;

	// почётные звания в базе без «Республики Беларусь» / «Беларуси»:
	// в указе суффикс есть, и он часть фразы награды
	if (honorTitle) {
		static const wchar_t* suffixes[] = {
			L" Республики Беларусь", L" Рэспублікі Беларусь", L" Беларуси", L" Беларусі"
		};
		size_t extra = 0;
		for (const wchar_t* suffix : suffixes) {
			const size_t len = std::wcslen(suffix);
			if (len <= extra || bestNormEnd + len > text.size())
				continue;
			if (text.compare(bestNormEnd, len, suffix) == 0 && atWordEnd(text, bestNormEnd + len))
				extra = len;
		}
		bestNormEnd += extra;
	}

	if (phraseLength)
		*phraseLength = phraseEnd(award, norm.origin, bestNormEnd);
	return bestId;
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

// Общая должность нескольких людей во множественном числе:
// «спартсменаў-інструктараў ... Пунько С.В., Сівакову Т.В. і» (после
// последнего имени остался список). Первое слово ставится в единственное
// число того же падежа, в именительный его переводит словарь должностей.
// «спартсменаў-інструктараў» → «спартсмена-інструктара», «членам» → «члену»
void singularFirstWord(std::wstring& text, LangType lang)
{
	const size_t wordEnd = text.find_first_of(L"  ");
	std::wstring word = text.substr(0, wordEnd);
	std::wstring result;
	size_t start = 0;
	while (start <= word.size())
	{
		size_t hyphen = word.find(L'-', start);
		std::wstring part = word.substr(start, hyphen == std::wstring::npos ? std::wstring::npos : hyphen - start);
		auto endsWith = [&](const wchar_t* suffix) {
			const size_t length = std::wcslen(suffix);
			return part.size() > length + 2 && part.compare(part.size() - length, length, suffix) == 0;
		};
		if (lang == LangBY && endsWith(L"аў"))
			part.replace(part.size() - 2, 2, L"а");
		else if (lang == LangRU && endsWith(L"ов"))
			part.replace(part.size() - 2, 2, L"а");
		else if (endsWith(L"ам"))
			part.replace(part.size() - 2, 2, L"у");
		else
			return;
		result += part;
		if (hyphen == std::wstring::npos)
			break;
		result += L'-';
		start = hyphen + 1;
	}
	text.replace(0, word.size(), result);
}

void singularSharedJob(std::wstring& text, LangType lang)
{
	std::wstring trimmed = text;
	StrTrim(trimmed);
	const std::wstring conjunction = lang == LangRU ? L" и" : L" і";
	const bool listed = !trimmed.empty() && (trimmed.back() == L','
		|| (trimmed.size() > conjunction.size()
			&& trimmed.compare(trimmed.size() - conjunction.size(), conjunction.size(), conjunction) == 0));
	if (listed)
		singularFirstWord(text, lang);
}

// имена через запятую и «і» перед тире: «Антоненку Алега Уладзіміравіча,
// Панамарова Сцяпана Анатольевіча і Цыплакова Уладзіміра Віктаравіча» (указ 10/2014).
// Пусто, если хоть одна часть — не три слова с заглавной.
bool nameWithoutJob(const std::wstring& line, size_t dash);

std::vector<std::wstring> namesBeforeDash(std::wstring text, LangType lang)
{
	std::replace(text.begin(), text.end(), L'\u00A0', L' ');
	const std::wstring conjunction = lang == LangRU ? L" и " : L" і ";
	for (size_t at = text.find(conjunction); at != std::wstring::npos; at = text.find(conjunction, at))
		text.replace(at, conjunction.size(), L", ");
	std::vector<std::wstring> names;
	for (std::wstring part : SplitString(text, L','))
	{
		StrTrim(part);
		if (!nameWithoutJob(part + L" –", part.size() + 1))
			return {};
		names.push_back(part);
	}
	return names;
}

// строка без строчных букв: первая буква заглавная, остальные строчные,
// как названия званий в каталоге. Иначе строка как есть.
std::wstring sentenceCaseIfUpper(const std::wstring& line)
{
	QString text = QString::fromStdWString(line);
	bool letters = false;
	for (const QChar character : text)
	{
		if (character.isLower())
			return line;
		letters = letters || character.isLetter();
	}
	if (!letters)
		return line;
	text = text.toLower();
	for (qsizetype i = 0; i < text.size(); ++i)
	{
		if (text[i].isLetter())
		{
			text[i] = text[i].toUpper();
			break;
		}
	}
	return text.toStdWString();
}

// уровень номера пункта в начале строки: «2. …» — 1, «1.1. …» — 2, нет номера — 0.
// После номера обычный или неразрывный пробел.
int pointLevel(const std::wstring& line)
{
	size_t pos = line.find_first_not_of(L" \t ");
	if (pos == std::wstring::npos || !iswdigit(line[pos]))
		return 0;
	int level = 0;
	while (pos < line.size() && iswdigit(line[pos]))
	{
		while (pos < line.size() && iswdigit(line[pos]))
			++pos;
		if (pos >= line.size() || line[pos] != L'.')
			return 0;
		++pos;
		++level;
	}
	return pos < line.size() && (line[pos] == L' ' || line[pos] == L' ') ? level : 0;
}

// Должность всех людей пункта, если она написана в его строке:
// «1. Узнагародзіць спартсменаў-інструктараў нацыянальнай каманды …:» (указ 396/2005) или
// «1. За плённую … дзейнасць дэпутатаў Палаты прадстаўнікоў … склікання, …» (569/2011).
// Первое слово — во множественном числе («…аў», «…ов»), возвращается в единственном.
std::wstring jobFromPointLine(const std::wstring& line, size_t actionEnd, LangType lang)
{
	std::wstring text = line;
	std::replace(text.begin(), text.end(), L' ', L' ');
	const auto plural = [&](std::wstring job) -> std::wstring {
		job.erase(0, job.find_first_not_of(L' '));
		job.erase(job.find_last_not_of(L' ') + 1);
		if (job.empty() || IsCapital(job[0]))
			return {};
		static const wchar_t* awards[] = { L"медал", L"орден", L"ордэн", L"почетн", L"ганаров" };
		const std::wstring lower = ToLower(job);
		for (const wchar_t* award : awards)
		{
			if (lower.find(award) != std::wstring::npos)
				return {};
		}
		std::wstring single = job;
		singularFirstWord(single, lang);
		return single == job ? std::wstring() : single;
	};

	if (actionEnd != std::wstring::npos)
	{
		const size_t last = text.find_last_not_of(L' ');
		if (last != std::wstring::npos && text[last] == L':' && last > actionEnd)
		{
			const std::wstring job = plural(text.substr(actionEnd, last - actionEnd));
			if (!job.empty())
				return job;
		}
	}
	for (const wchar_t* marker : { L"дзейнасць ", L"деятельность " })
	{
		const size_t at = text.find(marker);
		if (at == std::wstring::npos)
			continue;
		const size_t from = at + std::wcslen(marker);
		const size_t end = text.find_first_of(L",:", from);
		const std::wstring job = plural(text.substr(from, end == std::wstring::npos ? std::wstring::npos : end - from));
		if (!job.empty())
			return job;
	}
	return {};
}

// «Барданова Александра Ивановича –» без должности после тире
// (указ 217/1999 о Героях Советского Союза): три слова с заглавной
bool nameWithoutJob(const std::wstring& line, size_t dash)
{
	if (dash == 0 || line.find_first_not_of(L"  ", dash + 1) != std::wstring::npos)
		return false;
	std::wstring name = line.substr(0, dash);
	std::replace(name.begin(), name.end(), L'\u00A0', L' ');
	int words = 0;
	bool start = true;
	for (wchar_t character : name)
	{
		if (character == L' ')
		{
			start = true;
			continue;
		}
		if (start && !IsCapital(character))
			return false;
		if (start)
			++words;
		start = false;
	}
	return words == 3;
}

const wchar_t* posthumousMark(bool russian)
{
	return russian ? L"(посмертно)" : L"(пасмяротна)";
}

// повтор пробелов, в том числе неразрывного: «дывізіёна\u00A0\u00A0841-й», «г.\u00A0 Жлобин».
// одиночный неразрывный остаётся: он склеивает «у\u00A0г.\u00A0Нові» и «1.\u00A0Узнагародзіць»
void collapseRepeatedSpaces(std::wstring& text)
{
	std::wstring collapsed;
	collapsed.reserve(text.size());
	wchar_t blank = 0;
	int run = 0;
	const auto flush = [&]() {
		if (run == 1)
			collapsed.push_back(blank);
		else if (run > 1)
			collapsed.push_back(L' ');
		run = 0;
	};
	for (wchar_t c : text)
	{
		if (c == L' ' || c == L'\u00A0')
		{
			if (run == 0)
				blank = c;
			++run;
			continue;
		}
		flush();
		collapsed.push_back(c);
	}
	flush();
	text.swap(collapsed);
}

// вырезает пометку и всё после неё
bool cutPosthumous(std::wstring& text, bool russian)
{
	const size_t pos = text.find(posthumousMark(russian));
	if (pos == std::wstring::npos)
		return false;
	text.erase(pos);
	return true;
}

// В указах начала 1990-х перед именем стоит «тов.»:
// «скульптору тов. Гумилевскому Льву Николаевичу». Пробел после точки
// часто неразрывный, фамилия слипается с «тов.», и от имени остаются
// имя и отчество.
void stripComradeTitle(std::wstring& text)
{
	const auto gap = [](wchar_t character) { return character == L' ' || character == L'\u00A0'; };
	for (size_t pos = 0; pos < text.size(); )
	{
		const bool title = (text.compare(pos, 3, L"тов") == 0 || text.compare(pos, 3, L"Тов") == 0)
			&& (pos == 0 || gap(text[pos - 1]));
		if (!title) {
			++pos;
			continue;
		}
		size_t end = pos + 3;
		if (end < text.size() && text[end] == L'.')
			++end;
		if (end < text.size() && gap(text[end])) {
			size_t name = end;
			while (name < text.size() && gap(text[name]))
				++name;
			if (name < text.size() && IsCapital(text[name])) {
				// пробел перед «тов.» может быть неразрывным; имя режется по обычному
				if (pos > 0 && text[pos - 1] == L'\u00A0')
					text[pos - 1] = L' ';
				text.erase(pos, name - pos);
				continue;
			}
		}
		pos = end > pos ? end : pos + 1;
	}
}

// «Беларуси», «Республики» — страна в должности, не фамилия.
static bool countryNameWord(const std::wstring& word)
{
	const std::wstring lower = ToLower(word);
	return lower == L"беларуси" || lower == L"беларусі"
		|| lower == L"республики" || lower == L"рэспублікі"
		|| lower == L"беларусь" || lower == L"беларусь";
}

static std::wstring firstWordOf(const std::wstring& text)
{
	const size_t start = text.find_first_not_of(L' ');
	if (start == std::wstring::npos)
		return std::wstring();
	const size_t end = text.find_first_of(L" (", start);
	return text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
}

// страна стояла в конце, настоящее имя с отчеством — левее
static bool takeEarlierPerson(CNameConverter& names, std::wstring& text, LangType lang, NounCase nounCase, std::wstring& name)
{
	size_t patronymic = std::wstring::npos;
	size_t wordBegin = 0;
	for (size_t i = 0; i <= text.size(); ++i)
	{
		const bool boundary = i == text.size() || text[i] == L' ' || text[i] == L',' || text[i] == L'(';
		if (!boundary)
			continue;
		if (i > wordBegin && names.IsMiddleName(text.substr(wordBegin, i - wordBegin), lang, nounCase))
			patronymic = wordBegin;
		wordBegin = i + 1;
	}
	if (patronymic == std::wstring::npos || patronymic == 0)
		return false;

	size_t end = text.find_first_of(L" ,(", patronymic);
	if (end == std::wstring::npos)
		end = text.size();

	size_t start = patronymic;
	int taken = 0;
	while (taken < 2 && start > 0)
	{
		size_t prevEnd = start;
		while (prevEnd > 0 && (text[prevEnd - 1] == L' ' || text[prevEnd - 1] == L','))
			--prevEnd;
		if (prevEnd == 0)
			break;
		size_t prevBegin = prevEnd;
		while (prevBegin > 0 && text[prevBegin - 1] != L' ' && text[prevBegin - 1] != L',')
			--prevBegin;
		if (!IsCapital(text[prevBegin]))
			break;
		start = prevBegin;
		++taken;
	}
	if (taken < 2)
		return false;

	name = text.substr(start, end - start);
	text.erase(start, end - start);
	while (start < text.size() && text[start] == L' ')
		text.erase(start, 1);
	if (start > 0 && start < text.size() && text[start - 1] == L' ' && text[start] == L',')
		text.erase(start - 1, 1);
	StrTrim(text);
	return true;
}

// Имя — два последних слова с заглавной. Отчество забирает фамилию
// перед ними («слесаря цеха Иванова Ивана Ивановича»). Без отчества
// заглавное слово должности остаётся в должности («слесаря Цех Иванова Ивана»).
// «тов.» перед фамилией убирается. Ложь, если такой пары нет.
bool takePersonName(CNameConverter& names, std::wstring& text, LangType lang, NounCase nounCase, std::wstring& name)
{
	StrTrim(text);
	stripComradeTitle(text);

	bool bHasAlias = false;
	size_t posBracket = text.rfind(L'(');
	if (posBracket != std::wstring::npos && text.rfind(L')') > text.length() - 10)
		bHasAlias = true;

	size_t pos1 = bHasAlias ? text.rfind(' ', posBracket - 2) : text.rfind(' ');
	size_t pos2 = text.rfind(' ', pos1 - 1);
	if (pos1 == std::wstring::npos || pos2 == std::wstring::npos)
		return false;

	if (!IsCapital(text[pos1 + 1]) || !IsCapital(text[pos2 + 1]))
		return false;

	size_t sep = pos2;
	bool nameOnly = false;
	if (names.IsMiddleName(text.substr(pos1), lang, nounCase))
	{
		size_t pos3 = text.rfind(' ', pos2 - 1);
		if (pos3 != std::wstring::npos && IsCapital(text[pos3 + 1]))
			sep = pos3;
		else if (pos3 == std::wstring::npos && IsCapital(text[0]))
			nameOnly = true;
	}

	if (nameOnly) {
		name = text;
		text.clear();
		return true;
	}

	name = text.substr(sep + 1);
	// «всея Беларуси Филарета»: страна остаётся в должности.
	// «Республики Беларусь» целиком — не человек, имя с отчеством стоит раньше.
	if (countryNameWord(firstWordOf(name)))
	{
		const size_t space = name.find(L' ');
		const std::wstring rest = space == std::wstring::npos ? std::wstring() : name.substr(space + 1);
		if (!rest.empty() && IsCapital(rest[0]) && !countryNameWord(firstWordOf(rest)))
		{
			name = rest;
			const size_t person = text.find(rest, sep);
			text.erase(person == std::wstring::npos ? sep : person);
			StrTrim(text);
			return true;
		}
		text.erase(sep);
		StrTrim(text);
		return takeEarlierPerson(names, text, lang, nounCase, name);
	}

	text.erase(sep);
	return true;
}

// Организация в именительном или винительном: «узнагародзіць адкрытае
// акцыянернае таварыства «...» ордэнам». Родительный («слесара адкрытага
// таварыства») сюда не попадает: это место работы, его забирает takePersonName.
// Первое слово — с заглавной, как в указе 2023 года.
bool takeOrganisation(std::wstring& text, std::wstring& name)
{
	for (wchar_t& character : text) {
		if (character == L'\u00A0')
			character = L' ';
	}
	StrTrim(text);
	while (!text.empty() && (text.back() == L';' || text.back() == L'.')) {
		text.pop_back();
		StrTrim(text);
	}
	if (text.empty())
		return false;

	static const wchar_t* heads[] = {
		L"адкрытае ", L"закрытае ", L"рэспубліканскае ", L"камунальнае ",
		L"дзяржаўнае ", L"дзяржаўную ўстанову", L"унітарнае ", L"вытворчае ",
		L"навукова-вытворчае ", L"таварыства з ", L"калектыў ",
		L"открытое ", L"закрытое ", L"республиканское ", L"коммунальное ",
		L"государственное ", L"унитарное ", L"общество с ", L"коллектив ",
	};
	const std::wstring lower = ToLower(text);
	bool organisation = false;
	for (const wchar_t* head : heads) {
		const size_t length = std::wcslen(head);
		if (lower.size() >= length && lower.compare(0, length, head) == 0) {
			organisation = true;
			break;
		}
	}
	if (!organisation)
		return false;

	// «..., Пінскі раён» в списке — место, не часть названия
	std::wstring place;
	const size_t comma = text.rfind(L',');
	if (comma != std::wstring::npos && !InsideQuotes(text, comma)) {
		std::wstring tail = text.substr(comma + 1);
		StrTrim(tail);
		const std::wstring lowerTail = ToLower(tail);
		const bool isPlace = lowerTail.find(L"раён") != std::wstring::npos
			|| lowerTail.find(L"район") != std::wstring::npos
			|| lowerTail.find(L"абласц") != std::wstring::npos
			|| lowerTail.find(L"област") != std::wstring::npos
			|| lowerTail.rfind(L"г.", 0) == 0
			|| lowerTail.rfind(L"г ", 0) == 0;
		if (isPlace) {
			place = std::move(tail);
			text.erase(comma);
			StrTrim(text);
		}
	}

	QString title = QString::fromStdWString(text);
	title[0] = title.at(0).toUpper();
	name = title.toStdWString();
	text = std::move(place);
	return true;
}

// имя не заполняем: в личных данных название и, если есть, район через запятую
void assignOrganisation(PersonInfo& info, std::wstring title, const std::wstring& place)
{
	info.Organization = true;
	info.Name.clear();
	if (!place.empty()) {
		title += L", ";
		title += place;
	}
	info.Description = std::move(title);
}

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

namespace {

bool isGroupLetter(wchar_t c)
{
	return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')
		|| (c >= L'А' && c <= L'я') || c == L'Ё' || c == L'ё'
		|| c == L'І' || c == L'і' || c == L'Ў' || c == L'ў';
}

// точка кончает предложение, но не «В.И.Ленина» и не «г. Минска»
size_t groupSentenceEnd(const std::wstring& text)
{
	for (size_t i = 0; i < text.length(); ++i)
	{
		const wchar_t c = text[i];
		if (c == L':' || c == L'\t' || c == L'\n' || c == L'\r')
			return i;
		if (c != L'.')
			continue;
		const bool before = i > 0 && isGroupLetter(text[i - 1]);
		const bool after = i + 1 < text.length() && isGroupLetter(text[i + 1]);
		if (before && after)
			continue;
		const bool singleLetter = before && (i == 1 || !isGroupLetter(text[i - 2]));
		if (singleLetter)
			continue;
		return i;
	}
	return std::wstring::npos;
}

// запятая, после которой учёная степень, а не продолжение должности
size_t academicTitleComma(const std::wstring& text)
{
	static const wchar_t* titles[] = {
		L"профессор", L"доктор", L"кандидат", L"академик", L"доцент", L"лауреат",
		L"прафесар", L"доктар", L"кандыдат", L"акадэмік", L"дацэнт",
	};
	for (size_t comma = text.find(L','); comma != std::wstring::npos; comma = text.find(L',', comma + 1))
	{
		if (InsideQuotes(text, comma))
			continue;
		size_t pos = comma + 1;
		while (pos < text.length() && text[pos] == L' ')
			++pos;
		const std::wstring rest = ToLower(text.substr(pos));
		for (const wchar_t* title : titles)
		{
			const size_t length = std::wcslen(title);
			if (rest.compare(0, length, title) == 0)
				return comma;
		}
	}
	return std::wstring::npos;
}

// пункт указа: «2. За ...», «2.1. узнагародзіць:»
bool startsWithPointNumber(const std::wstring& line)
{
	size_t pos = line.find_first_not_of(L" \t");
	if (pos == std::wstring::npos || !iswdigit(line[pos]))
		return false;
	while (pos < line.length() && (iswdigit(line[pos]) || line[pos] == L'.'))
		++pos;
	return pos > 0 && line[pos - 1] == L'.' && (pos == line.length() || line[pos] == L' ');
}

// самое правое слово группы: «группу работников завода» — завод, а не «работников завода»
bool findLastToken(const std::wstring& str, const std::vector<std::wstring>& tokens, size_t& start, size_t& end)
{
	bool found = false;
	for (const auto& token : tokens)
	{
		for (size_t pos = str.find(token); pos != std::wstring::npos; pos = str.find(token, pos + 1))
		{
			const size_t tokenEnd = pos + token.size();
			if (!found || tokenEnd > end || (tokenEnd == end && pos < start))
			{
				start = pos;
				end = tokenEnd;
				found = true;
			}
		}
	}
	return found;
}

// пустая должность: в каталоге стоит само слово списка в именительном единственном
std::wstring groupRole(const std::wstring& token)
{
	if (token == L"работников" || token == L"работникам")
		return L"работник";
	if (token == L"работнікаў" || token == L"работнікамі")
		return L"работнік";
	if (token == L"военнослужащих" || token == L"военнослужащими")
		return L"военнослужащий";
	if (token == L"ваеннаслужачых" || token == L"ваеннаслужачымі")
		return L"ваеннаслужачы";
	return std::wstring();
}

// у автомобильного завода каталог не держит родительское объединение;
// у рессорного и кузнечного заводов того же объединения — держит
void cutTrailingAssociation(std::wstring& text)
{
	if (text.find(L"«Мінскі аўтамабільны завод»") == std::wstring::npos
		&& text.find(L"«Минский автомобильный завод»") == std::wstring::npos
		&& text.find(L"Минского автомобильного завода") == std::wstring::npos)
		return;
	const wchar_t* tails[] = {
		L" вытворчага аб'яднання «",
		L" вытворчага аб\u2019яднання «",
		L" вытворчага аб\u02BCяднання «",
		L" производственного объединения «",
	};
	for (const wchar_t* tail : tails)
	{
		const size_t pos = text.rfind(tail);
		if (pos == std::wstring::npos || pos == 0)
			continue;
		const size_t open = text.find(L'«', pos);
		const size_t close = open == std::wstring::npos ? std::wstring::npos : text.find(L'»', open);
		if (close == std::wstring::npos)
			continue;
		size_t after = close + 1;
		while (after < text.size() && text[after] == L' ')
			++after;
		if (after != text.size())
			continue;
		text.erase(pos);
		StrTrim(text);
		return;
	}
}

// «завод» і акадэмія: к должности идёт завод, акадэмия остаётся тем, кто её уже назвал
std::wstring cutOtherOrganisation(std::wstring& groupInfo)
{
	bool inQuote = false;
	for (size_t i = 0; i + 3 <= groupInfo.size(); ++i)
	{
		if (groupInfo[i] == L'«')
			inQuote = true;
		else if (groupInfo[i] == L'»')
			inQuote = false;
		if (inQuote)
			continue;
		const bool conjunction = groupInfo.compare(i, 3, L" і ") == 0 || groupInfo.compare(i, 3, L" и ") == 0;
		if (!conjunction)
			continue;
		size_t next = i + 3;
		while (next < groupInfo.size() && groupInfo[next] == L' ')
			++next;
		if (next >= groupInfo.size())
			break;
		const wchar_t character = groupInfo[next];
		if (character != L'«' && !IsCapital(character))
			continue;
		std::wstring other = groupInfo.substr(next);
		groupInfo.erase(i);
		StrTrim(groupInfo);
		StrTrim(other);
		return other;
	}
	return std::wstring();
}

// «работников Белорусского государственного университета»: головное слово —
// это сами награждённые, к должности добавляется организация
void stripGroupHead(std::wstring& groupInfo)
{
	static const wchar_t* heads[] = {
		L"наградить", L"узнагародзіць", L"группы", L"групы", L"творческих", L"творчых",
		L"работников", L"работникам", L"работнікаў", L"сотрудников", L"супрацоўнікаў", L"супрацоўнікам",
		L"артистов", L"артистам", L"артыстаў", L"артыстам", L"і", L"и",
		L"прадпрыемстваў", L"предприятий",
	};
	for (bool stripped = true; stripped;)
	{
		stripped = false;
		StrTrim(groupInfo);
		for (const wchar_t* head : heads)
		{
			const size_t length = std::wcslen(head);
			if (groupInfo.compare(0, length, head) == 0 && groupInfo.length() > length && groupInfo[length] == L' ')
			{
				groupInfo.erase(0, length + 1);
				stripped = true;
			}
		}
	}
}

} // namespace

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

bool CDocumentParser::ProcessFile(const std::wstring& fileName, const std::vector<std::wstring>& arrLines)
{
	bool bResult = false;

	m_FileName = fileName;

	bool bUkazStarted = false, bAwardStarted = false, bAwardTypeFound = false, bPossibleGroupStart = false, bMultiplePochet = false;
	std::wstring curAward;
	int awardId = 0;
	int nAwardedCount = 0;
	size_t posStart = 0, posEnd = 0;
	// читаемая награда кончилась: считаем её людей
	const auto finishAward = [&] {
		UpdateAwardStat(awardId, curAward, nAwardedCount);
		curAward.clear();
		awardId = nAwardedCount = 0;
	};

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

	for (auto itrLine = arrLines.begin(); itrLine != arrLines.end(); itrLine++)
	{
		markGroup();
		// найден новый указ президента
		if (FindTokens(*itrLine, searchUkazStart, posStart, posEnd))
		{
			finishAward();
			m_GroupActive = m_GroupWholeDecree = false;
			bUkazStarted = true;
			bAwardStarted = bAwardTypeFound = bPossibleGroupStart = bMultiplePochet = false;
			m_UkazInfo.Clear();
			m_UkazInfo.RussianLang = ((*itrLine)[posEnd] == L'Е');
			m_SectionCity.clear();
			pointJob.clear();
			continue;
		}

		if (!bUkazStarted)
			continue;

		std::wstring strLine = *itrLine;
		collapseRepeatedSpaces(strLine);
		size_t posActionEnd = 0, posTargetStart = 0;

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
			finishAward();
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
		{
			const int level = pointLevel(strLine);
			if (level > 0 || actionLine)
			{
				const std::wstring job = jobFromPointLine(strLine, actionLine ? posActionEnd : std::wstring::npos,
					m_UkazInfo.RussianLang ? LangRU : LangBY);
				if (!job.empty() || level == 1)
					pointJob = job;
			}
		}

		if (bAwardStarted)
		{
			// дальше орденов и медалей нет
			if (bAwardTypeFound && FindTokens(strLine, m_UkazInfo.RussianLang ? searchFinishRU : searchFinishBY, posStart, posEnd))
			{
				bUkazStarted = bAwardStarted = bAwardTypeFound = false;
				finishAward();
				continue;
			}

			// вид награды
			if (FindTokens(strLine, m_UkazInfo.RussianLang ? searchTargetsRU : searchTargetsBY, posStart, posEnd))
			{
				bAwardTypeFound = true;
				posTargetStart = posStart;
				finishAward();
				curAward = strLine.substr(posTargetStart);
				curAward.erase(curAward.find_last_not_of(L" :\t\n\r.") + 1);
				// длина фразы в указе, а не названия в каталоге:
				// «медалью «За отвагу»» длиннее, чем «Медаль "За отвагу"»
				size_t awardPhrase = 0;
				awardId = GetAwardId(curAward, m_UkazInfo.RussianLang, false, &awardPhrase);

				// пункт «1. Узнагародзіць:», а дальше по предложению на награду:
				// «ордэнам Пашаны – спартсменаў ... Пунько С.В. і Фядотаву І.А.;»,
				// «трэнера ... Казеку Мікалая Іванавіча ордэнам Айчыны III ступені;»
				const bool sentenceAfterAction = posActionEnd == 0 && !actionLine && awardId > 0
					&& (strLine.back() == L';' || strLine.back() == L'.');

				// одно награждение в строке
				if ((posActionEnd > 0 || sentenceAfterAction) && strLine.back() != L':')
				{
					std::wstring strAwardedInfo;
					// человек и звание до награды («...Кашкана ... орденом «...» (посмертно).»)
					const bool personBeforeAward = posTargetStart - posActionEnd > 10;
					if (personBeforeAward)
						strAwardedInfo = strLine.substr(posActionEnd, posTargetStart - posActionEnd);
					else if (awardId > 0 && awardPhrase > 0 && posTargetStart + awardPhrase < strLine.length())
						strAwardedInfo = strLine.substr(posTargetStart + awardPhrase);

					if (sentenceAfterAction)
					{
						const size_t first = strAwardedInfo.find_first_not_of(L"  –-");
						strAwardedInfo.erase(0, first == std::wstring::npos ? strAwardedInfo.size() : first);
						const size_t last = strAwardedInfo.find_last_not_of(L"  ;.");
						strAwardedInfo.erase(last == std::wstring::npos ? 0 : last + 1);
					}

					if(!strAwardedInfo.empty())
					{
						const size_t addedFrom = m_Persons.size();
						ExtractPersonInfo(curAward, strAwardedInfo);
						if (sentenceAfterAction && m_Persons.size() > addedFrom)
						{
							nAwardedCount += int(m_Persons.size() - addedFrom);
							continue;
						}
						// награждённый одной строкой тоже человек награды: иначе она
						// считается пустой и в окне импорта горит красным
						nAwardedCount += int(m_Persons.size() - addedFrom);
						// пометка может стоять после названия награды, вне strAwardedInfo
						if (personBeforeAward && awardId > 0 && awardPhrase > 0 && m_Persons.size() > addedFrom
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
					if (awardId)
						bPossibleGroupStart = true;
					else if (strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[2] : searchTargetsBY[2]) != std::wstring::npos ||
						     strLine.find(m_UkazInfo.RussianLang ? searchTargetsRU[3] : searchTargetsBY[3]) != std::wstring::npos )
					{
						bMultiplePochet = true;
					}
				}
			}

			if (bAwardTypeFound)
			{
				if (bMultiplePochet)
				{
					// «ЗАСЛУЖЕННЫЙ СТРОИТЕЛЬ РЕСПУБЛИКИ БЕЛАРУСЬ» (указ 2515/1993)
					const std::wstring titleLine = sentenceCaseIfUpper(strLine);
					if (awardId <= 0)
					{
						awardId = GetAwardId(titleLine.c_str(), m_UkazInfo.RussianLang, true);
						if (awardId > 0)
							continue;
					}
					else if (titleLine.find(L"Заслуж") != std::wstring::npos || titleLine.find(L"Народны") != std::wstring::npos)
					{
						int newAwardId = GetAwardId(titleLine.c_str(), m_UkazInfo.RussianLang, true);
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
					// организация отдельной строкой списка, в том числе с тире в названии
					// («ІНТЭГРАЛ» – кіруючая кампанія ...): это не человек и должность
					std::wstring orgLine = strLine;
					std::wstring orgName;
					if (takeOrganisation(orgLine, orgName))
					{
						PersonInfo info;
						info.FileName = m_FileName;
						info.Ukaz = m_UkazInfo;
						info.AwardId = awardId;
						assignOrganisation(info, std::move(orgName), orgLine);
						nAwardedCount++;
						NormalizePersonInfo(info);
						m_Persons.push_back(info);
						continue;
					}

					// должность и список имён без тире под званием: «спартсменам-інструктарам …
					// Андрыеўскаму Аляксандру Леанідавічу, Антоненку … і …;» (указы 262/2002, 494/2004)
					{
						std::wstring listLine = strLine;
						std::replace(listLine.begin(), listLine.end(), L'\u00A0', L' ');
						// StrTrim \u0441\u0440\u0435\u0437\u0430\u043B \u0431\u044B \u0438 \u0442\u043E\u0447\u043A\u0443 \u0432 \u043A\u043E\u043D\u0446\u0435, \u0430 \u043F\u043E \u043D\u0435\u0439 \u0443\u0437\u043D\u0430\u0451\u0442\u0441\u044F \u043F\u0440\u0435\u0434\u043B\u043E\u0436\u0435\u043D\u0438\u0435
						listLine.erase(0, listLine.find_first_not_of(L' '));
						listLine.erase(listLine.find_last_not_of(L' ') + 1);
						if (!actionLine && !listLine.empty() && (listLine.back() == L';' || listLine.back() == L'.')
							&& listLine.find(L", ") != std::wstring::npos && listLine.find(L'–') == std::wstring::npos)
						{
							while (!listLine.empty() && (listLine.back() == L';' || listLine.back() == L'.'))
								listLine.pop_back();
							const size_t addedFrom = m_Persons.size();
							std::wstring awardText = curAward;
							ExtractPersonInfo(awardText, listLine, awardId);
							if (m_Persons.size() > addedFrom)
							{
								// без списка в остатке это не перечень людей, а чья-то фраза
								const std::wstring& rest = m_Persons.back().Description;
								const std::wstring conjunction = m_UkazInfo.RussianLang ? L" и" : L" і";
								const bool listed = !rest.empty() && (rest.back() == L','
									|| (rest.size() > conjunction.size()
										&& rest.compare(rest.size() - conjunction.size(), conjunction.size(), conjunction) == 0));
								if (listed)
								{
									nAwardedCount += int(m_Persons.size() - addedFrom);
									continue;
								}
								m_Persons.resize(addedFrom);
							}
						}
					}

					// список награждённых
					size_t pos = strLine.find_first_of(L"–-", 0);
					if (pos == std::wstring::npos)
					{
						// имя одно в строке, должность общая в пункте указа
						// («…дэпутатаў Палаты прадстаўнікоў… ўзнагародзіць:», указ 569/2011);
						// в новом формате за именем идёт строка с тире
						std::wstring next = itrLine + 1 == arrLines.end() ? std::wstring() : *(itrLine + 1);
						StrTrim(next);
						const LangType lang = m_UkazInfo.RussianLang ? LangRU : LangBY;
						std::wstring bare = strLine;
						std::replace(bare.begin(), bare.end(), L'\u00A0', L' ');
						StrTrim(bare);
						if (!actionLine && !bare.empty() && (next.empty() || (next[0] != L'–' && next[0] != L'-'))
							&& nameWithoutJob(bare + L" –", bare.size() + 1)
							&& m_NameConverter.IsMiddleName(bare.substr(bare.rfind(L' ') + 1), lang, GetNounCaseByAwardId(awardId)))
						{
							PersonInfo info;
							info.FileName = m_FileName;
							info.Ukaz = m_UkazInfo;
							info.AwardId = awardId;
							info.Name = bare;
							nAwardedCount++;
							NormalizePersonInfo(info);
							m_Persons.push_back(info);
						}
						continue;
					}

					PersonInfo info;
					info.FileName = m_FileName;
					info.Ukaz = m_UkazInfo;
					info.AwardId = awardId;

					if(pos == 0)
					{
						// новый формат списка в docx:
						// имя на предыдущей строке, должность на следующей
						if (itrLine == arrLines.begin() || itrLine + 1 == arrLines.end())
							continue;
						nAwardedCount++;

						info.Name = *(itrLine - 1);
						std::replace(info.Name.begin(), info.Name.end(), L'\v', L' ');
						info.Description = *(++itrLine);

						NormalizePersonInfo(info);
						m_Persons.push_back(info);
					}
					// одно или несколько имён перед тире, строка награды без двоеточия
					// («Антоненку А.У., Панамарова С.А. – членаў каманды …», указ 10/2014)
					else if (strLine[pos] == L'–' && !actionLine
						&& !namesBeforeDash(strLine.substr(0, pos), m_UkazInfo.RussianLang ? LangRU : LangBY).empty())
					{
						const std::vector<std::wstring> names = namesBeforeDash(strLine.substr(0, pos), m_UkazInfo.RussianLang ? LangRU : LangBY);
						std::wstring job = strLine.substr(pos + 1);
						std::replace(job.begin(), job.end(), L'\u00A0', L' ');
						StrTrim(job);
						while (!job.empty() && (job.back() == L';' || job.back() == L'.'))
							job.pop_back();
						if (names.size() > 1)
							singularFirstWord(job, m_UkazInfo.RussianLang ? LangRU : LangBY);
						for (const std::wstring& name : names)
						{
							PersonInfo person = info;
							person.Name = name;
							person.Description = job;
							nAwardedCount++;
							NormalizePersonInfo(person);
							m_Persons.push_back(person);
						}
					}
					// строка с «узнагародзіць» / «прысвоіць» уже разобрана как одно
					// награждение: тире в ней — внутри должности, а не список
					else if ((bPossibleGroupStart || bMultiplePochet || nameWithoutJob(strLine, pos)) && strLine[pos] == L'–' && !actionLine)
					{
						// старый формат списка в docx
						nAwardedCount++;

						info.Name = strLine.substr(0, pos);
						info.Description = strLine.substr(pos + 1);

						NormalizePersonInfo(info);
						m_Persons.push_back(info);
					}
				}
			}
		}
	}
	markGroup();

	UpdateAwardStat(awardId, curAward, nAwardedCount);

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

	PersonInfo info;
	info.Ukaz = m_UkazInfo;
	info.FileName = m_FileName;
	info.AwardId = awardId;
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
	PersonInfo person2;

	person2.AwardId = person.AwardId;
	person2.FileName = person.FileName;
	person2.Ukaz = person.Ukaz;
	person2.InGroup = person.InGroup;
	person2.PointJob = person.PointJob;

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
		PersonInfo extra;
		extra.AwardId = person.AwardId;
		extra.FileName = person.FileName;
		extra.Ukaz = person.Ukaz;
		extra.InGroup = person.InGroup;
		extra.PointJob = person.PointJob;
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

namespace {

bool isLowerCyrillic(wchar_t c)
{
	return (c >= L'а' && c <= L'я') || c == L'ё' || c == L'і' || c == L'ў';
}

bool isUpperCyrillic(wchar_t c)
{
	return (c >= L'А' && c <= L'Я') || c == L'Ё' || c == L'І' || c == L'Ў';
}

bool isWordLetter(wchar_t c)
{
	return isLowerCyrillic(c) || isUpperCyrillic(c) || c == L'\'' || c == L'\u2019' || c == L'\u02BC';
}

// сколько раз каталог видел слово; слово, которое каталог знает только в другой
// форме («прапаршчыка» при «прапаршчык»), считается один раз
int knownCount(const std::map<std::wstring, int>& words, const std::wstring& word)
{
	const auto exact = words.find(word);
	if (exact != words.end())
		return exact->second;
	if (word.length() < 7)
		return 0;
	const std::wstring stem = word.substr(0, word.length() - 2);
	const auto next = words.lower_bound(stem);
	return next != words.end() && next->first.compare(0, stem.length(), stem) == 0 ? 1 : 0;
}

// латинские двойники в кириллических словах («пaдпaлкоўнiка», «Віктаровічa»)
void toCyrillicLetters(std::wstring& text, LangType lang)
{
	text = NormalizeCyrillicText(QString::fromStdWString(text), lang == LangBY).toStdWString();
}

// «”Ушацкі лясгас“», «“…”»: типографские кавычки — ёлочки, как в каталоге
void toGuillemets(std::wstring& text)
{
	for (size_t i = 0; i < text.size(); ++i)
	{
		const wchar_t c = text[i];
		if (c != L'\u201C' && c != L'\u201D' && c != L'\u201E')
			continue;
		const bool letterAfter = i + 1 < text.size() && (isWordLetter(text[i + 1]) || iswalnum(text[i + 1]));
		const bool letterBefore = i > 0 && (isWordLetter(text[i - 1]) || iswalnum(text[i - 1]));
		if (letterAfter && !letterBefore)
			text[i] = L'«';
		else if (letterBefore && !letterAfter)
			text[i] = L'»';
	}
}

// «АксануУладзіміраўну»: отчество слиплось с именем
void splitGluedPatronymic(CNameConverter& names, std::wstring& name, LangType lang, NounCase nounCase)
{
	for (size_t i = 1; i < name.size(); ++i)
	{
		if (!isLowerCyrillic(name[i - 1]) || !isUpperCyrillic(name[i]))
			continue;
		size_t end = name.find(L' ', i);
		if (end == std::wstring::npos)
			end = name.size();
		if (names.IsMiddleName(name.substr(i, end - i), lang, nounCase))
			name.insert(i, 1, L' ');
	}
}

// «Алексия ІІ»: римская цифра в русском имени латиницей, как в каталоге
void latinRomanNumerals(std::wstring& name)
{
	size_t begin = 0;
	while (begin < name.size())
	{
		size_t end = name.find_first_of(L" ()", begin);
		if (end == std::wstring::npos)
			end = name.size();
		bool roman = end - begin >= 2;
		for (size_t i = begin; i < end && roman; ++i)
			roman = std::wstring(L"ІIVХX").find(name[i]) != std::wstring::npos;
		if (roman)
		{
			for (size_t i = begin; i < end; ++i)
			{
				if (name[i] == L'І')
					name[i] = L'I';
				else if (name[i] == L'Х')
					name[i] = L'X';
			}
		}
		begin = end + 1;
	}
}

} // namespace

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
	if (m_NameConverter.NormalizeName(person.Name, lang, nounCase))
		person.Flags |= PersonInfo::NF_NAME;
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