#pragma once

#include <initializer_list>
#include <string>
#include <vector>

bool IsCapital(wchar_t character);
class QDate;
QDate ParseDate(std::wstring strDate, bool russian);
bool FindTokens(const std::wstring& str, const std::vector<std::wstring>& tokens, size_t& start, size_t& end);
std::vector<std::wstring> SplitString(const std::wstring& str, wchar_t sep);
void StrTrim(std::wstring& str);
std::wstring ToLower(const std::wstring& str);
// следующая « или » после pos — закрывающая, значит pos внутри кавычек.
// Вложенные кавычки закрываются один раз («НВА «Цэнтр»), более ранние не считаются.
bool InsideQuotes(const std::wstring& text, size_t pos);
// латинская «i» вместо белорусской «і»; capitalToo заменяет и «I»
// (не для целых строк: «I катэгорыі», римские цифры)
void ToCyrillicI(std::wstring& text, bool capitalToo);
// слово кончается suffix и до него не меньше minStem букв
bool EndsWith(const std::wstring& word, const wchar_t* suffix, size_t minStem = 0);
bool EndsWithAny(const std::wstring& word, std::initializer_list<const wchar_t*> suffixes, size_t minStem = 0);

class QString;
// Латинская буква, похожая на кириллическую (a c e o p x y A B C E H K M O P T X Y,
// плюс i/I в белорусском тексте), внутри кириллического слова становится
// кириллической: «Iгар», «Міxaйлаўна», «прядильщицa», «пaдпaлкоўнік». Буква
// заменяется, только если сосед кириллический, поэтому римские цифры («I класа»,
// «III ступені») и латинские слова («iPhone») остаются. Отдельные строчные между
// кириллическими словами становятся союзом или предлогом («a», «c», «o», «y»).
QString NormalizeCyrillicText(const QString& text, bool belarusian);
