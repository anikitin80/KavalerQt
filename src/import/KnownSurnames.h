#pragma once

// фамилии каталога для CNameConverter::SetKnownSurnames: первое слово ФИО
// с отчеством (три слова и больше), отдельно русские и белорусские.
// Слова должностей и званий каталога для CDocumentParser::SetKnownWords.

#include "Catalog.h"
#include "DocumentParser.h"
#include "NameConverter.h"
#include "TextUtil.h"

#include <map>
#include <unordered_map>

inline void loadKnownSurnames(CNameConverter& converter, const Catalog& catalog)
{
    // [пол: SexAny, SexMale, SexFemale]
    std::unordered_map<std::wstring, int> russian[3], belarusian[3];
    const auto add = [](std::unordered_map<std::wstring, int>* counts, const QString& name, bool by) {
        const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (words.size() < 3)
            return;
        std::wstring surname = words.front().toStdWString();
        if (by)
            ToCyrillicI(surname, true);
        surname = ToLower(surname);
        ++counts[SexAny][surname];
        // пол — по окончанию отчества: «-віч», «-вич», «-ич» / «-на»
        const QString patronymic = words.at(2).toLower();
        if (patronymic.endsWith(QStringLiteral("ич")) || patronymic.endsWith(QStringLiteral("іч")))
            ++counts[SexMale][surname];
        else if (patronymic.endsWith(QStringLiteral("на")))
            ++counts[SexFemale][surname];
    };
    for (const Person& person : catalog.people()) {
        add(russian, person.name, false);
        add(belarusian, person.nameBy, true);
    }
    for (SexType sex : {SexAny, SexMale, SexFemale}) {
        converter.SetKnownSurnames(LangRU, std::move(russian[sex]), sex);
        converter.SetKnownSurnames(LangBY, std::move(belarusian[sex]), sex);
    }
}

inline void loadKnownWords(CDocumentParser& parser, const Catalog& catalog)
{
    std::map<std::wstring, int> russian, belarusian;
    const auto add = [](std::map<std::wstring, int>& counts, const QString& text, bool by) {
        QString word;
        const auto flush = [&] {
            // дефис внутри слова остаётся: «агульна-адукацыйная» считается отдельно
            while (word.endsWith(QLatin1Char('-')))
                word.chop(1);
            while (word.startsWith(QLatin1Char('-')))
                word.remove(0, 1);
            if (word.size() > 1) {
                std::wstring lower = word.toLower().toStdWString();
                if (by)
                    ToCyrillicI(lower, false);
                ++counts[lower];
            }
            word.clear();
        };
        for (const QChar c : text) {
            if (c.isLetter() || c == QLatin1Char('-') || c == QLatin1Char('\'') || c == QChar(0x2019) || c == QChar(0x02BC))
                word += c;
            else
                flush();
        }
        flush();
    };
    for (const Person& person : catalog.people()) {
        add(russian, person.personal, false);
        add(russian, person.rank, false);
        add(belarusian, person.personalBy, true);
        add(belarusian, person.rankBy, true);
    }
    parser.SetKnownWords(LangRU, std::move(russian));
    parser.SetKnownWords(LangBY, std::move(belarusian));
}
