#include "TextUtil.h"

#include <QDate>
#include <QString>
#include <algorithm>

bool IsWordDivision(wchar_t character)
{
    static const std::wstring divisions = {L'-', 57389, L'*', L';', L'('};
    return divisions.find(character) != std::wstring::npos;
}

bool IsCapital(wchar_t character)
{
    return character >= L'А' && character <= L'Я' || character == L'І' || character == 'I' || character == L'Ё';
}

QDate ParseDate(std::wstring strDate, bool russian)
{
    StrTrim(strDate);
    strDate.erase(std::remove(strDate.begin(), strDate.end(), L' '), strDate.end());
    if (!russian) // "лiпеня" typed with Latin i
        std::replace(strDate.begin(), strDate.end(), L'i', L'і');

    const size_t sep1 = strDate.find_first_not_of(L"0123456789");
    const size_t sep2 = strDate.find_last_not_of(L"0123456789");

    static const std::wstring ruMonth[] = {L"янв", L"фев", L"мар", L"апр", L"мая", L"июн", L"июл", L"авг", L"сен", L"окт", L"ноя", L"дек"};
    static const std::wstring byMonth[] = {L"сту", L"лют", L"сак", L"кра", L"мая", L"чэр", L"ліп", L"жні", L"вер", L"кас", L"ліс", L"сне"};

    if (sep1 != std::wstring::npos && sep2 != std::wstring::npos && sep2 >= sep1) {
        const int day = _wtoi(strDate.substr(0, sep1).c_str());
        const int year = _wtoi(strDate.substr(sep2 + 1).c_str());
        const QString monthName = QString::fromStdWString(strDate.substr(sep1, sep2 - sep1 + 1)).trimmed();
        int month = 0;
        for (int i = 0; i < 12; ++i) {
            const std::wstring& token = russian ? ruMonth[i] : byMonth[i];
            if (monthName.startsWith(QString::fromStdWString(token))) {
                month = i + 1;
                break;
            }
        }
        const QDate date(year, month, day);
        if (date.isValid())
            return date;
    }
    return {};
}

bool FindTokens(const std::wstring& str, std::vector<std::wstring>& tokens, size_t& start, size_t& end)
{
    for (const auto& token : tokens) {
        const size_t pos = str.find(token);
        if (pos != std::wstring::npos) {
            start = pos;
            end = start + token.size();
            return true;
        }
    }
    return false;
}

std::vector<std::wstring> SplitString(const std::wstring& str, wchar_t sep)
{
    std::vector<std::wstring> result;
    size_t posStart = 0;
    while (posStart <= str.size()) {
        const size_t posNext = str.find(sep, posStart);
        if (posNext == std::wstring::npos) {
            result.emplace_back(str.substr(posStart));
            break;
        }
        result.emplace_back(str.substr(posStart, posNext - posStart));
        posStart = posNext + 1;
    }
    return result;
}

namespace {

bool isCyrillicLetter(QChar c)
{
    const char16_t code = c.unicode();
    return code >= 0x0400 && code <= 0x04FF && c.isLetter();
}

// nearest letter before/after position (skipping spaces and punctuation) is Cyrillic
bool cyrillicAround(const QString& text, int pos)
{
    for (int i = pos - 1; i >= 0; --i) {
        if (text.at(i).isLetter())
            return isCyrillicLetter(text.at(i));
    }
    return false;
}

bool cyrillicAfter(const QString& text, int pos)
{
    for (int i = pos + 1; i < text.size(); ++i) {
        if (text.at(i).isLetter())
            return isCyrillicLetter(text.at(i));
    }
    return false;
}

// position of a Latin i/I to replace, or -1:
// - inside a Cyrillic word: "Iгар", "Мiхайлавіч";
// - lower-case "i" as a word between Cyrillic words: the conjunction "і"
//   ("лагістыкі i цэнавай"); a standalone capital "I" is a Roman numeral.
int latinIInCyrillicWord(const QString& text, int from)
{
    for (int i = from; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c != QLatin1Char('i') && c != QLatin1Char('I'))
            continue;
        const QChar previous = i > 0 ? text.at(i - 1) : QChar();
        const QChar next = i + 1 < text.size() ? text.at(i + 1) : QChar();
        if (isCyrillicLetter(previous) || isCyrillicLetter(next))
            return i;
        if (c == QLatin1Char('i') && !previous.isLetter() && !next.isLetter()
            && cyrillicAround(text, i) && cyrillicAfter(text, i))
            return i;
    }
    return -1;
}

} // namespace

QString NormalizeBelarusianI(const QString& text)
{
    int pos = latinIInCyrillicWord(text, 0);
    if (pos < 0)
        return text;
    QString result = text;
    // rescan from the start: a replaced letter can make its Latin neighbour
    // ("IIванаў") part of the Cyrillic word too
    while (pos >= 0) {
        result[pos] = result.at(pos) == QLatin1Char('i') ? QChar(0x0456) : QChar(0x0406);
        pos = latinIInCyrillicWord(result, 0);
    }
    return result;
}

bool HasLatinBelarusianI(const QString& text)
{
    return latinIInCyrillicWord(text, 0) >= 0;
}

std::wstring ToLower(const std::wstring& str)
{
    return QString::fromStdWString(str).toLower().toStdWString();
}

void StrTrim(std::wstring& str)
{
    if (str.empty())
        return;
    const size_t first = str.find_first_not_of(L" . ");
    if (first == std::wstring::npos) {
        str.clear();
        return;
    }
    if (first > 0)
        str.erase(0, first);
    const size_t last = str.find_last_not_of(L" . ");
    if (last != std::wstring::npos && last + 1 < str.length())
        str.erase(last + 1);
}
