#include "TextUtil.h"

#include <QDate>
#include <QString>
#include <algorithm>
#include <cwchar>

bool IsCapital(wchar_t character)
{
    return character >= L'А' && character <= L'Я' || character == L'І' || character == 'I' || character == L'Ё';
}

QDate ParseDate(std::wstring strDate, bool russian)
{
    StrTrim(strDate);
    strDate.erase(std::remove(strDate.begin(), strDate.end(), L' '), strDate.end());
    if (!russian) // «лiпеня» набрано латинской i
        ToCyrillicI(strDate, false);

    const size_t sep1 = strDate.find_first_not_of(L"0123456789");
    const size_t sep2 = strDate.find_last_not_of(L"0123456789");

    static const std::wstring ruMonth[] = {L"янв", L"фев", L"мар", L"апр", L"мая", L"июн", L"июл", L"авг", L"сен", L"окт", L"ноя", L"дек"};
    static const std::wstring byMonth[] = {L"сту", L"лют", L"сак", L"кра", L"мая", L"чэр", L"ліп", L"жні", L"вер", L"кас", L"ліс", L"сне"};

    if (sep1 != std::wstring::npos && sep2 != std::wstring::npos && sep2 >= sep1) {
        const int day = int(std::wcstol(strDate.substr(0, sep1).c_str(), nullptr, 10));
        const int year = int(std::wcstol(strDate.substr(sep2 + 1).c_str(), nullptr, 10));
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

bool FindTokens(const std::wstring& str, const std::vector<std::wstring>& tokens, size_t& start, size_t& end)
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

// ближайшая буква от index шагом +1 или -1, пробелы и знаки пропускаются
bool nearestLetterIsCyrillic(const QString& text, int index, int step)
{
    for (; index >= 0 && index < text.size(); index += step) {
        if (text.at(index).isLetter())
            return isCyrillicLetter(text.at(index));
    }
    return false;
}

bool cyrillicAround(const QString& text, int pos)
{
    return nearestLetterIsCyrillic(text, pos - 1, -1);
}

bool cyrillicAfter(const QString& text, int pos)
{
    return nearestLetterIsCyrillic(text, pos + 1, 1);
}

// кириллическая буква, похожая на латинскую, или пустой QChar
QChar cyrillicTwin(QChar c, bool belarusian, bool onlyI)
{
    switch (c.unicode()) {
    case 'i': return belarusian ? QChar(0x0456) : QChar(); // і
    case 'I': return belarusian ? QChar(0x0406) : QChar(); // І
    default: break;
    }
    if (onlyI)
        return {};
    switch (c.unicode()) {
    case 'a': return QChar(0x0430);
    case 'c': return QChar(0x0441);
    case 'e': return QChar(0x0435);
    case 'o': return QChar(0x043E);
    case 'p': return QChar(0x0440);
    case 'x': return QChar(0x0445);
    case 'y': return QChar(0x0443);
    case 'A': return QChar(0x0410);
    case 'B': return QChar(0x0412);
    case 'C': return QChar(0x0421);
    case 'E': return QChar(0x0415);
    case 'H': return QChar(0x041D);
    case 'K': return QChar(0x041A);
    case 'M': return QChar(0x041C);
    case 'O': return QChar(0x041E);
    case 'P': return QChar(0x0420);
    case 'T': return QChar(0x0422);
    case 'X': return QChar(0x0425);
    case 'Y': return QChar(0x0423);
    default: return {};
    }
}

// позиция латинской буквы, которую надо заменить кириллической парой, или -1:
// - внутри кириллического слова: «Iгар», «Міxaйлаўна», «прядильщицa»;
// - строчная буква одна между кириллическими словами: союзы и предлоги
//   «і», «а», «с», «о», «у», набранные латиницей.
//   Отдельная заглавная остаётся: римские цифры («I класа»),
//   категории водительских прав («категории B»).
// слово вокруг pos — римская цифра, возможно с кириллическими І/Х
// («XXІ», «XІV» в «XXІ стагоддзя»): оно остаётся как есть
bool romanNumeralAt(const QString& text, int pos)
{
    int begin = pos;
    while (begin > 0 && text.at(begin - 1).isLetter())
        --begin;
    int end = pos;
    while (end + 1 < text.size() && text.at(end + 1).isLetter())
        ++end;
    static const QString roman = QStringLiteral("IVXLCDMІХ");
    for (int i = begin; i <= end; ++i) {
        if (!roman.contains(text.at(i)))
            return false;
    }
    return true;
}

int latinInCyrillicWord(const QString& text, int from, bool belarusian, bool onlyI)
{
    for (int i = from; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (cyrillicTwin(c, belarusian, onlyI).isNull() || romanNumeralAt(text, i))
            continue;
        const QChar previous = i > 0 ? text.at(i - 1) : QChar();
        const QChar next = i + 1 < text.size() ? text.at(i + 1) : QChar();
        if (isCyrillicLetter(previous) || isCyrillicLetter(next))
            return i;
        if (c.isLower() && !previous.isLetter() && !next.isLetter()
            && cyrillicAround(text, i) && cyrillicAfter(text, i))
            return i;
    }
    return -1;
}

QString normalize(const QString& text, bool belarusian, bool onlyI)
{
    int pos = latinInCyrillicWord(text, 0, belarusian, onlyI);
    if (pos < 0)
        return text;
    QString result = text;
    // снова с начала: заменённая буква может сделать латинского соседа
    // тоже частью кириллического слова («Mixaіл» → «Міхаіл»)
    while (pos >= 0) {
        result[pos] = cyrillicTwin(result.at(pos), belarusian, onlyI);
        pos = latinInCyrillicWord(result, 0, belarusian, onlyI);
    }
    return result;
}

} // namespace

QString NormalizeBelarusianI(const QString& text)
{
    return normalize(text, true, true);
}

bool HasLatinBelarusianI(const QString& text)
{
    return latinInCyrillicWord(text, 0, true, true) >= 0;
}

QString NormalizeCyrillicText(const QString& text, bool belarusian)
{
    return normalize(text, belarusian, false);
}

bool HasLatinLookalikes(const QString& text, bool belarusian)
{
    return latinInCyrillicWord(text, 0, belarusian, false) >= 0;
}

void ToCyrillicI(std::wstring& text, bool capitalToo)
{
    std::replace(text.begin(), text.end(), L'i', L'\u0456');
    if (capitalToo)
        std::replace(text.begin(), text.end(), L'I', L'\u0406');
}

std::wstring ToLower(const std::wstring& str)
{
    return QString::fromStdWString(str).toLower().toStdWString();
}

bool InsideQuotes(const std::wstring& text, size_t pos)
{
    const size_t quote = text.find_first_of(L"«»", pos);
    return quote != std::wstring::npos && text[quote] == L'»';
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
