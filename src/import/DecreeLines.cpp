#include "DecreeLines.h"

#include "TextUtil.h"

#include <QString>

#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace decree {

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

} // namespace decree
