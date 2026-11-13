#pragma once

#include "NameConverter.h"

#include <map>
#include <string>

// Чистка ФИО и должности человека перед сравнением с каталогом.
namespace decree {

bool isLowerCyrillic(wchar_t c);

bool isUpperCyrillic(wchar_t c);

bool isWordLetter(wchar_t c);

// сколько раз каталог видел слово; слово, которое каталог знает только в другой
// форме («прапаршчыка» при «прапаршчык»), считается один раз
int knownCount(const std::map<std::wstring, int>& words, const std::wstring& word);

// латинские двойники в кириллических словах («пaдпaлкоўнiка», «Віктаровічa»)
void toCyrillicLetters(std::wstring& text, LangType lang);

// «”Ушацкі лясгас“», «“…”»: типографские кавычки — ёлочки, как в каталоге
void toGuillemets(std::wstring& text);

// «АксануУладзіміраўну»: отчество слиплось с именем
void splitGluedPatronymic(CNameConverter& names, std::wstring& name, LangType lang, NounCase nounCase);

// «Алексия ІІ»: римская цифра в русском имени латиницей, как в каталоге
void latinRomanNumerals(std::wstring& name);

} // namespace decree
