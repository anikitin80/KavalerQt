#include "ImportMatch.h"

#include "Catalog.h"
#include "TextUtil.h"

#include <QHash>
#include <QString>

namespace {

// decree, award and name; Belarusian names compared like the catalog does
QString identityKey(const PersonInfo& info)
{
    const QString name = QString::fromStdWString(info.Name);
    return QString::number(info.Ukaz.Number) + QChar(0x1F) + QString::number(info.Ukaz.Date.toJulianDay())
        + QChar(0x1F) + QString::number(info.AwardId) + QChar(0x1F)
        + (info.Ukaz.RussianLang ? QStringLiteral("ru:") : QStringLiteral("by:"))
        + NormalizeCyrillicText(name, !info.Ukaz.RussianLang);
}

QString fileKey(const PersonInfo& info)
{
    return QString::fromStdWString(info.FileName) + QChar(0x1E) + identityKey(info);
}

Person probe(const PersonInfo& info)
{
    Person person;
    person.ukaseNo = info.Ukaz.Number;
    person.ukaseDate = info.Ukaz.Date;
    person.awardId = info.AwardId;
    (info.Ukaz.RussianLang ? person.name : person.nameBy) = QString::fromStdWString(info.Name);
    return person;
}

} // namespace

QVector<ImportMatch> matchImported(const std::vector<PersonInfo>& persons, const Catalog& catalog)
{
    QHash<QString, int> inFile;
    QVector<int> ordinal(int(persons.size()));
    for (size_t i = 0; i < persons.size(); ++i)
        ordinal[int(i)] = inFile[fileKey(persons[i])]++;

    QVector<ImportMatch> result(int(persons.size()));
    for (size_t i = 0; i < persons.size(); ++i) {
        ImportMatch& match = result[int(i)];
        match.namesakes = inFile.value(fileKey(persons[i]));
        match.known = ordinal.at(int(i)) < catalog.count(probe(persons[i]));
    }
    return result;
}

QVector<int> personsToImport(const std::vector<PersonInfo>& persons, const QVector<bool>& wanted,
                             const Catalog& catalog)
{
    QVector<int> result;
    QHash<QString, int> inFile;  // persons with the key seen so far in the same file
    QHash<QString, int> planned; // persons with the key already chosen for import
    for (size_t i = 0; i < persons.size(); ++i) {
        const PersonInfo& info = persons[i];
        const int ordinal = inFile[fileKey(info)]++;
        if (!wanted.value(int(i), true))
            continue;
        const QString key = identityKey(info);
        // the catalog plus what is planned already has this person: known, or
        // the same decree parsed from another file
        if (catalog.count(probe(info)) + planned.value(key) > ordinal)
            continue;
        result.push_back(int(i));
        ++planned[key];
    }
    return result;
}
