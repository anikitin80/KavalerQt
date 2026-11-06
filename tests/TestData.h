#pragma once

// Shared helpers for the parser tests: fixture paths, award table and a
// parser set up with the test copies of the profession dictionaries.

#include "DocumentParser.h"

#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace TestData {

inline QString path(const QString& relative)
{
    return QStringLiteral(KAVALER_TEST_DATA "/") + relative;
}

// Never read the real user's settings (dictionary paths etc.) from tests.
inline void isolateSettings()
{
    QCoreApplication::setOrganizationName(QStringLiteral("KavalerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("KavalerTests"));
}

// Lines of a UTF-8 text file; "\v" escapes stand for Word's manual line break.
inline QStringList readLines(const QString& file)
{
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream stream(&input);
    stream.setEncoding(QStringConverter::Utf8);
    QStringList lines;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        line.replace(QStringLiteral("\\v"), QStringLiteral("\v"));
        lines.push_back(line);
    }
    return lines;
}

inline std::vector<std::wstring> toWide(const QStringList& lines)
{
    std::vector<std::wstring> result;
    result.reserve(size_t(lines.size()));
    for (const QString& line : lines)
        result.push_back(line.toStdWString());
    return result;
}

// tests/data/awards.tsv: id, type, priority, name, name_by (copy of the Awards table)
inline std::map<long, DecreeAward> awards()
{
    std::map<long, DecreeAward> result;
    for (const QString& line : readLines(path(QStringLiteral("awards.tsv")))) {
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList cells = line.split(QLatin1Char('\t'));
        if (cells.size() < 5)
            continue;
        DecreeAward award;
        award.ID = cells.at(0).toLong();
        award.Type = cells.at(1).toInt();
        award.Priority = cells.at(2).toInt();
        award.Name = cells.at(3).toStdWString();
        award.NameBY = cells.at(4).toStdWString();
        result[award.ID] = award;
    }
    return result;
}

inline void useTestDictionaries(CProfessionConverter& converter)
{
    converter.SetDictionaryPath(path(QStringLiteral("PositionsRU.csv")).toStdWString(), LangRU);
    converter.SetDictionaryPath(path(QStringLiteral("PositionsBY.csv")).toStdWString(), LangBY);
}

template <typename Parser = CDocumentParser>
std::unique_ptr<Parser> makeParser()
{
    auto parser = std::make_unique<Parser>();
    std::map<long, DecreeAward> table = awards();
    parser->Initialize(table);
    useTestDictionaries(parser->m_ProfessionConverter);
    return parser;
}

} // namespace TestData
