#pragma once

// фамилии каталога для CNameConverter::SetKnownSurnames: первое слово ФИО
// с отчеством (три слова и больше), отдельно русские и белорусские

#include "Catalog.h"
#include "NameConverter.h"
#include "TextUtil.h"

#include <unordered_map>

inline void loadKnownSurnames(CNameConverter& converter, const Catalog& catalog)
{
    std::unordered_map<std::wstring, int> russian, belarusian;
    const auto add = [](std::unordered_map<std::wstring, int>& counts, const QString& name, bool by) {
        const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (words.size() < 3)
            return;
        std::wstring surname = words.front().toStdWString();
        if (by)
            ToCyrillicI(surname, true);
        ++counts[ToLower(surname)];
    };
    for (const Person& person : catalog.people()) {
        add(russian, person.name, false);
        add(belarusian, person.nameBy, true);
    }
    converter.SetKnownSurnames(LangRU, std::move(russian));
    converter.SetKnownSurnames(LangBY, std::move(belarusian));
}
