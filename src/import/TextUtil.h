#pragma once

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

class QString;
// латинские «i»/«I» внутри кириллического слова («Iгар», «Мiхайлавіч») становятся
// белорусскими «і»/«І». Буква заменяется, только если сосед кириллический, поэтому
// римские цифры («I класа», «III ступені») и латинские слова («iPhone») остаются.
QString NormalizeBelarusianI(const QString& text);
bool HasLatinBelarusianI(const QString& text);

// То же для каждой латинской буквы, похожей на кириллическую (a c e o p x y
// A B C E H K M O P T X Y, плюс i/I в белорусском тексте): «Міxaйлаўна»,
// «прядильщицa», «пaдпaлкоўнік». Отдельные строчные между кириллическими
// словами становятся кириллическим союзом или предлогом («a», «c», «o», «y»).
QString NormalizeCyrillicText(const QString& text, bool belarusian);
bool HasLatinLookalikes(const QString& text, bool belarusian);
