#pragma once

#include "NameConverter.h"
#include "ParserTypes.h"

#include <string>
#include <vector>

// Строки списка указа: имя, организация, должность пункта, пометки.
namespace decree {

// Общая должность нескольких людей во множественном числе:
// «спартсменаў-інструктараў ... Пунько С.В., Сівакову Т.В. і» (после
// последнего имени остался список). Первое слово ставится в единственное
// число того же падежа, в именительный его переводит словарь должностей.
// «спартсменаў-інструктараў» → «спартсмена-інструктара», «членам» → «члену»
void singularFirstWord(std::wstring& text, LangType lang);

void singularSharedJob(std::wstring& text, LangType lang);

std::vector<std::wstring> namesBeforeDash(std::wstring text, LangType lang);

// строка без строчных букв: первая буква заглавная, остальные строчные,
// как названия званий в каталоге. Иначе строка как есть.
std::wstring sentenceCaseIfUpper(const std::wstring& line);

// уровень номера пункта в начале строки: «2. …» — 1, «1.1. …» — 2, нет номера — 0.
// После номера обычный или неразрывный пробел.
int pointLevel(const std::wstring& line);

// Должность всех людей пункта, если она написана в его строке:
// «1. Узнагародзіць спартсменаў-інструктараў нацыянальнай каманды …:» (указ 396/2005) или
// «1. За плённую … дзейнасць дэпутатаў Палаты прадстаўнікоў … склікання, …» (569/2011).
// Первое слово — во множественном числе («…аў», «…ов»), возвращается в единственном.
std::wstring jobFromPointLine(const std::wstring& line, size_t actionEnd, LangType lang);

// «Барданова Александра Ивановича –» без должности после тире
// (указ 217/1999 о Героях Советского Союза): три слова с заглавной
bool nameWithoutJob(const std::wstring& line, size_t dash);

const wchar_t* posthumousMark(bool russian);

// повтор пробелов, в том числе неразрывного: «дывізіёна\u00A0\u00A0841-й», «г.\u00A0 Жлобин».
// одиночный неразрывный остаётся: он склеивает «у\u00A0г.\u00A0Нові» и «1.\u00A0Узнагародзіць»
void collapseRepeatedSpaces(std::wstring& text);

// вырезает пометку и всё после неё
bool cutPosthumous(std::wstring& text, bool russian);

// В указах начала 1990-х перед именем стоит «тов.»:
// «скульптору тов. Гумилевскому Льву Николаевичу». Пробел после точки
// часто неразрывный, фамилия слипается с «тов.», и от имени остаются
// имя и отчество.
void stripComradeTitle(std::wstring& text);

// Имя — два последних слова с заглавной. Отчество забирает фамилию
// перед ними («слесаря цеха Иванова Ивана Ивановича»). Без отчества
// заглавное слово должности остаётся в должности («слесаря Цех Иванова Ивана»).
// «тов.» перед фамилией убирается. Ложь, если такой пары нет.
bool takePersonName(CNameConverter& names, std::wstring& text, LangType lang, NounCase nounCase, std::wstring& name);

// Организация в именительном или винительном: «узнагародзіць адкрытае
// акцыянернае таварыства «...» ордэнам». Родительный («слесара адкрытага
// таварыства») сюда не попадает: это место работы, его забирает takePersonName.
// Первое слово — с заглавной, как в указе 2023 года.
bool takeOrganisation(std::wstring& text, std::wstring& name);

// имя не заполняем: в личных данных название и, если есть, район через запятую
void assignOrganisation(PersonInfo& info, std::wstring title, const std::wstring& place);

// пункт указа: «2. За ...», «2.1. узнагародзіць:»
bool startsWithPointNumber(const std::wstring& line);

} // namespace decree
