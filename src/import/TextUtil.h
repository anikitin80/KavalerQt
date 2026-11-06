#pragma once

#include <string>
#include <vector>

bool IsWordDivision(wchar_t character);
bool IsCapital(wchar_t character);
class QDate;
QDate ParseDate(std::wstring strDate, bool russian);
bool FindTokens(const std::wstring& str, std::vector<std::wstring>& tokens, size_t& start, size_t& end);
std::vector<std::wstring> SplitString(const std::wstring& str, wchar_t sep);
void StrTrim(std::wstring& str);
std::wstring ToLower(const std::wstring& str);

class QString;
// Latin "i"/"I" typed inside a Cyrillic word ("Iгар", "Мiхайлавіч") becomes the
// Belarusian "і"/"І". A letter is replaced only when a neighbour is Cyrillic, so
// Roman numerals ("I класа", "III ступені") and Latin words ("iPhone") stay.
QString NormalizeBelarusianI(const QString& text);
bool HasLatinBelarusianI(const QString& text);

// The same for every Latin letter that looks like a Cyrillic one (a c e o p x y
// A B C E H K M O P T X Y, plus i/I for Belarusian text): "Міxaйлаўна",
// "прядильщицa", "пaдпaлкоўнік". Standalone lower-case letters between Cyrillic
// words become the Cyrillic conjunction or preposition ("a", "c", "o", "y").
QString NormalizeCyrillicText(const QString& text, bool belarusian);
bool HasLatinLookalikes(const QString& text, bool belarusian);
