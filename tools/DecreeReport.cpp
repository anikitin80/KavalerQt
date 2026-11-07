// decree_report: runs the decree parser over the whole archive and compares the
// result with the catalog, the way the import window would see it.
//
//   decree_report --db D:/Awards/DB/Kavaler.sqlite --docs D:/Awards/AwardsDoc
//                 --out build/reports/2026-11-06 [--compare build/reports/<previous>]
//   decree_report --names --db D:/Awards/DB/Kavaler.sqlite --out build/reports/names
//
// --names does not read decrees. It lists Belarusian full names stored with more
// than one Russian spelling (names.tsv). An empty list means there are none.
//
// The database is opened read-only. Output in --out:
//   summary.txt  totals, by year, which fields differ, comparison with --compare
//   persons.tsv  parsed persons that differ from the catalog (all with --all)
//   files.tsv    per decree file, with the indexes of the persons that matched
//                exactly ("0-12,14"): --compare needs them, persons.tsv omits them
//   changes.tsv  (with --compare) persons whose status or parsed values changed
//
// Status of a parsed person (same matching as ImportMatch: decree, award and
// name without Latin look-alikes; the k-th namesake in a file is matched with
// the k-th catalog record):
//   EXACT        found, description, rank and posthumous equal the catalog
//   DIFF         found, but description, rank or posthumous differ
//   OTHER_AWARD  the decree has this name, but under another award
//   NEW          no record with this decree and name (wrong name form, or
//                really not imported)
//   BAD_HEADER   the decree number or date was not recognised

#include "Catalog.h"
#include "DocumentParser.h"
#include "DocxText.h"
#include "TextUtil.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QSettings>
#include <QTextStream>

#include <cstdio>

namespace {

QTextStream& console()
{
    static QTextStream stream(stdout);
    static bool init = false;
    if (!init) {
        stream.setEncoding(QStringConverter::Utf8);
        init = true;
    }
    return stream;
}

QString cell(QString text)
{
    text.replace(QLatin1Char('\t'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QChar('\v'), QLatin1Char(' '));
    return text;
}

QString nameKey(const QString& name, bool belarusian)
{
    return NormalizeCyrillicText(name, belarusian);
}

QString decreeKey(int number, const QDate& date, const QString& name, bool belarusian)
{
    return QString::number(number) + QChar(0x1F) + QString::number(date.toJulianDay()) + QChar(0x1F)
        + (belarusian ? QStringLiteral("by:") : QStringLiteral("ru:")) + nameKey(name, belarusian);
}

QString awardKey(int awardId, const QString& decree)
{
    return QString::number(awardId) + QChar(0x1E) + decree;
}

struct Row
{
    QString file;
    int year = 0;
    int index = 0;
    QString status;
    int awardId = 0;
    int dbId = 0;
    QString name;
    QString description;
    QString rank;
    bool posthumous = false;
    QString dbDescription;
    QString dbRank;
    bool dbPosthumous = false;
    QString diffFields;
};

const QStringList kStatuses = {QStringLiteral("EXACT"), QStringLiteral("DIFF"), QStringLiteral("OTHER_AWARD"),
                               QStringLiteral("NEW"), QStringLiteral("BAD_HEADER")};

QString rowKey(const QString& file, int index)
{
    return file + QChar(0x1F) + QString::number(index);
}

// "0-3,5" for {0, 1, 2, 3, 5}
QString indexRanges(const QVector<int>& sorted)
{
    QStringList parts;
    for (int i = 0; i < sorted.size();) {
        int j = i;
        while (j + 1 < sorted.size() && sorted.at(j + 1) == sorted.at(j) + 1)
            ++j;
        parts << (i == j ? QString::number(sorted.at(i))
                         : QString::number(sorted.at(i)) + QLatin1Char('-') + QString::number(sorted.at(j)));
        i = j + 1;
    }
    return parts.join(QLatin1Char(','));
}

// files.tsv of an earlier run: keys (file|index) of the persons that matched
// exactly; reports written before the exact_index column give an empty set
QSet<QString> readExactKeys(const QString& path)
{
    QSet<QString> keys;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return keys;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QStringList header = in.readLine().split(QLatin1Char('\t'));
    const int column = header.indexOf(QStringLiteral("exact_index"));
    if (column < 0)
        return keys;
    while (!in.atEnd()) {
        const QStringList c = in.readLine().split(QLatin1Char('\t'));
        if (c.size() <= column)
            continue;
        for (const QString& range : c.at(column).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            const int dash = range.indexOf(QLatin1Char('-'));
            const int from = range.left(dash < 0 ? range.size() : dash).toInt();
            const int to = dash < 0 ? from : range.mid(dash + 1).toInt();
            for (int index = from; index <= to; ++index)
                keys.insert(rowKey(c.at(0), index));
        }
    }
    return keys;
}

// persons.tsv of an earlier run: file|index -> row
QHash<QString, Row> readRows(const QString& path)
{
    QHash<QString, Row> rows;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return rows;
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    in.readLine(); // header
    while (!in.atEnd()) {
        const QStringList c = in.readLine().split(QLatin1Char('\t'));
        if (c.size() < 14)
            continue;
        Row row;
        row.file = c.at(0);
        row.year = c.at(1).toInt();
        row.index = c.at(2).toInt();
        row.status = c.at(3);
        row.awardId = c.at(4).toInt();
        row.dbId = c.at(5).toInt();
        row.name = c.at(6);
        row.description = c.at(7);
        row.rank = c.at(8);
        row.posthumous = c.at(9) == QLatin1String("1");
        row.dbDescription = c.at(10);
        row.dbRank = c.at(11);
        row.dbPosthumous = c.at(12) == QLatin1String("1");
        row.diffFields = c.at(13);
        rows.insert(rowKey(row.file, row.index), row);
    }
    return rows;
}

QString percent(int part, int total)
{
    return total ? QString::number(100.0 * part / total, 'f', 1) + QLatin1Char('%') : QStringLiteral("-");
}

// names.tsv and summary.txt: same Name_BY, several Name. Returns 0, or 1 on write failure.
int reportNameConflicts(const Catalog& catalog, const QString& databasePath, const QString& outDir)
{
    QDir().mkpath(outDir);
    QFile namesFile(QDir(outDir).filePath(QStringLiteral("names.tsv")));
    QFile summaryFile(QDir(outDir).filePath(QStringLiteral("summary.txt")));
    if (!namesFile.open(QIODevice::WriteOnly | QIODevice::Text) || !summaryFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        console() << "cannot write to " << outDir << "\n";
        return 1;
    }
    QTextStream names(&namesFile);
    names.setEncoding(QStringConverter::Utf8);
    names << "rows\tname_by\tname\tcount\n";
    const QVector<Catalog::NameConflict> conflicts = catalog.russianNameConflicts();
    int rows = 0;
    for (const Catalog::NameConflict& conflict : conflicts) {
        rows += conflict.rows;
        for (const Catalog::NameVariant& variant : conflict.names)
            names << conflict.rows << '\t' << cell(conflict.nameBy) << '\t' << cell(variant.name) << '\t' << variant.count << '\n';
    }

    QString summary;
    QTextStream s(&summary);
    s << "Расхождения перевода ФИО\n"
      << "база: " << databasePath << " (" << catalog.people().size() << " записей)\n"
      << "Одинаковое белорусское ФИО с разными русскими: " << conflicts.size() << " групп, " << rows << " записей\n"
      << "подробно: names.tsv\n";
    s.flush();
    QTextStream summaryOut(&summaryFile);
    summaryOut.setEncoding(QStringConverter::Utf8);
    summaryOut << summary;
    console() << summary;
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("decree_report"));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("Parses the decree archive and compares it with the catalog."));
    cli.addHelpOption();
    const QCommandLineOption dbOption(QStringLiteral("db"), QStringLiteral("Catalog database (opened read-only)."), QStringLiteral("file"));
    const QCommandLineOption docsOption(QStringLiteral("docs"), QStringLiteral("Folder with decree .docx files (searched recursively)."), QStringLiteral("folder"));
    const QCommandLineOption outOption(QStringLiteral("out"), QStringLiteral("Folder for the report."), QStringLiteral("folder"));
    const QCommandLineOption compareOption(QStringLiteral("compare"), QStringLiteral("Report folder of an earlier run."), QStringLiteral("folder"));
    const QCommandLineOption dictRuOption(QStringLiteral("dict-ru"), QStringLiteral("Russian profession dictionary (default: from Kavaler settings)."), QStringLiteral("file"));
    const QCommandLineOption dictByOption(QStringLiteral("dict-by"), QStringLiteral("Belarusian profession dictionary (default: from Kavaler settings)."), QStringLiteral("file"));
    const QCommandLineOption filterOption(QStringLiteral("filter"), QStringLiteral("Only files whose path contains this text."), QStringLiteral("text"));
    const QCommandLineOption limitOption(QStringLiteral("limit"), QStringLiteral("Only the first N files."), QStringLiteral("N"));
    const QCommandLineOption allOption(QStringLiteral("all"), QStringLiteral("Write every person to persons.tsv, not only the differing ones."));
    const QCommandLineOption namesOption(QStringLiteral("names"),
                                         QStringLiteral("Report Belarusian full names with more than one Russian spelling. Does not read decrees."));
    for (const auto& option : {dbOption, docsOption, outOption, compareOption, dictRuOption, dictByOption, filterOption, limitOption, allOption, namesOption})
        cli.addOption(option);
    cli.process(app);
    const bool namesOnly = cli.isSet(namesOption);
    if (!cli.isSet(dbOption) || !cli.isSet(outOption) || (!namesOnly && !cli.isSet(docsOption))) {
        console() << (namesOnly ? "--db and --out are required, see --help\n"
                                : "--db, --docs and --out are required, see --help\n");
        return 2;
    }

    Catalog catalog;
    QString error;
    if (!catalog.open(cli.value(dbOption), &error, true)) {
        console() << "cannot open the catalog: " << error << "\n";
        return 1;
    }
    if (namesOnly)
        return reportNameConflicts(catalog, cli.value(dbOption), cli.value(outOption));

    // profession dictionaries: explicit, or the ones the application uses
    QString dictRu = cli.value(dictRuOption);
    QString dictBy = cli.value(dictByOption);
    {
        QSettings appSettings(QStringLiteral("Anatoli Nikitin"), QStringLiteral("Kavaler"));
        if (dictRu.isEmpty())
            dictRu = appSettings.value(QStringLiteral("DictionaryRU")).toString();
        if (dictBy.isEmpty())
            dictBy = appSettings.value(QStringLiteral("DictionaryBY")).toString();
    }
    // the parser must not read or write the application's settings
    QCoreApplication::setOrganizationName(QStringLiteral("KavalerTools"));

    // catalog index: decree+name -> records (id order), and per award
    QHash<QString, QVector<const Person*>> byAward;
    QHash<QString, QVector<const Person*>> byName;
    {
        QVector<const Person*> people;
        for (const Person& person : catalog.people())
            people.push_back(&person);
        std::sort(people.begin(), people.end(), [](const Person* a, const Person* b) { return a->id < b->id; });
        for (const Person* person : people) {
            for (bool belarusian : {true, false}) {
                const QString& name = belarusian ? person->nameBy : person->name;
                if (name.isEmpty())
                    continue;
                const QString decree = decreeKey(person->ukaseNo, person->ukaseDate, name, belarusian);
                byName[decree].push_back(person);
                byAward[awardKey(person->awardId, decree)].push_back(person);
            }
        }
    }

    std::map<long, DecreeAward> awards;
    for (const Award& award : catalog.awards()) {
        DecreeAward item;
        item.ID = award.id;
        item.Type = award.type;
        item.Priority = award.priority;
        item.Name = award.name.toStdWString();
        item.NameBY = award.nameBy.toStdWString();
        awards[item.ID] = item;
    }
    CDocumentParser parser;
    parser.Initialize(awards);
    parser.m_ProfessionConverter.SetDictionaryPath(dictRu.toStdWString(), LangRU);
    parser.m_ProfessionConverter.SetDictionaryPath(dictBy.toStdWString(), LangBY);

    QStringList files;
    const QDir docs(cli.value(docsOption));
    for (QDirIterator it(docs.path(), {QStringLiteral("*.docx")}, QDir::Files, QDirIterator::Subdirectories); it.hasNext();) {
        const QString path = it.next();
        if (QFileInfo(path).fileName().startsWith(QStringLiteral("~$")))
            continue;
        if (cli.isSet(filterOption) && !path.contains(cli.value(filterOption), Qt::CaseInsensitive))
            continue;
        files.push_back(path);
    }
    files.sort();
    if (cli.isSet(limitOption))
        files = files.mid(0, cli.value(limitOption).toInt());

    const QString outDir = cli.value(outOption);
    QDir().mkpath(outDir);
    QFile personsFile(QDir(outDir).filePath(QStringLiteral("persons.tsv")));
    QFile filesFile(QDir(outDir).filePath(QStringLiteral("files.tsv")));
    if (!personsFile.open(QIODevice::WriteOnly | QIODevice::Text) || !filesFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        console() << "cannot write to " << outDir << "\n";
        return 1;
    }
    QTextStream persons(&personsFile);
    persons.setEncoding(QStringConverter::Utf8);
    QTextStream filesOut(&filesFile);
    filesOut.setEncoding(QStringConverter::Utf8);
    persons << "file\tyear\tindex\tstatus\taward\tdb_id\tname\tdescription\trank\tposthumous"
               "\tdb_description\tdb_rank\tdb_posthumous\tdiff_fields\tdecree_no\tdecree_date\tlanguage\tsource_name\tsource_case\n";
    filesOut << "file\tyear\tpersons\texact\tdiff\tother_award\tnew\tbad_header\terror\texact_index\tgroup_used\tgroup_info\n";
    const bool allPersons = cli.isSet(allOption);

    QMap<QString, int> totals;
    QMap<int, QMap<QString, int>> perYear;
    QMap<QString, int> diffFields;
    QHash<QString, Row> current;
    int emptyFiles = 0;
    int groupFiles = 0; // files where the group info is added to the description
    QElapsedTimer timer;
    timer.start();

    for (int f = 0; f < files.size(); ++f) {
        const QString path = files.at(f);
        const QString relative = docs.relativeFilePath(path);
        QString readError;
        std::vector<std::wstring> lines;
        for (const QString& paragraph : readDocxParagraphs(path, &readError))
            lines.push_back(paragraph.toStdWString());
        parser.ResetData();
        if (!lines.empty())
            parser.ProcessFile(QFileInfo(path).fileName().toStdWString(), lines);

        // group info ("работнікаў ... завода") is added to the description in the
        // import dialog by a check box per file: use it when more people match
        const std::wstring fileName = QFileInfo(path).fileName().toStdWString();
        bool useGroup = false;
        QString groupText;
        {
            GroupInfo group;
            std::wstring name = fileName;
            if (parser.GetGroupInfo(name, group))
                groupText = QString::fromStdWString(group.Info);
            if (parser.GetGroupInfo(name, group) && !group.Info.empty()) {
                int with = 0, without = 0;
                QHash<QString, int> seen;
                for (PersonInfo& info : parser.m_Persons) {
                    const bool belarusian = !info.Ukaz.RussianLang;
                    const QString key = awardKey(info.AwardId, decreeKey(info.Ukaz.Number, info.Ukaz.Date,
                                                                          QString::fromStdWString(info.Name), belarusian));
                    const int n = seen[key]++;
                    if (n >= byAward.value(key).size())
                        continue;
                    const QString& db = belarusian ? byAward.value(key).at(n)->personalBy : byAward.value(key).at(n)->personal;
                    for (bool checked : {false, true}) {
                        parser.CheckGroupInfo(fileName.c_str(), checked);
                        const QString description = NormalizeCyrillicText(
                            QString::fromStdWString(parser.GetPersonDescription(info, false)), belarusian);
                        if (description == db)
                            ++(checked ? with : without);
                    }
                }
                useGroup = with > without;
            }
            parser.CheckGroupInfo(fileName.c_str(), useGroup);
        }
        if (useGroup)
            ++groupFiles;

        QMap<QString, int> fileCounts;
        QHash<QString, int> ordinal;
        QVector<int> exactIndexes;
        for (size_t i = 0; i < parser.m_Persons.size(); ++i) {
            PersonInfo& info = parser.m_Persons[i];
            const bool belarusian = !info.Ukaz.RussianLang;
            Row row;
            row.file = relative;
            row.year = info.Ukaz.Date.isValid() ? info.Ukaz.Date.year() : 0;
            row.index = int(i);
            row.awardId = info.AwardId;
            row.name = QString::fromStdWString(info.Name);
            row.description = QString::fromStdWString(parser.GetPersonDescription(info, false));
            row.rank = QString::fromStdWString(info.MilitaryRank);
            row.posthumous = info.Posthumous;

            const QString decree = decreeKey(info.Ukaz.Number, info.Ukaz.Date, row.name, belarusian);
            const QString key = awardKey(info.AwardId, decree);
            const int n = ordinal[key]++;
            if (info.Ukaz.Number == 0 || !info.Ukaz.Date.isValid()) {
                row.status = QStringLiteral("BAD_HEADER");
            } else if (n < byAward.value(key).size()) {
                const Person* db = byAward.value(key).at(n);
                row.dbId = db->id;
                row.dbDescription = belarusian ? db->personalBy : db->personal;
                row.dbRank = belarusian ? db->rankBy : db->rank;
                row.dbPosthumous = db->posthumous != 0;
                QStringList differs;
                if (NormalizeCyrillicText(row.description, belarusian) != row.dbDescription)
                    differs << QStringLiteral("description");
                if (NormalizeCyrillicText(row.rank, belarusian) != row.dbRank)
                    differs << QStringLiteral("rank");
                if (row.posthumous != row.dbPosthumous)
                    differs << QStringLiteral("posthumous");
                row.diffFields = differs.join(QLatin1Char(','));
                row.status = differs.isEmpty() ? QStringLiteral("EXACT") : QStringLiteral("DIFF");
                for (const QString& field : differs)
                    ++diffFields[field];
            } else if (!byName.value(decree).isEmpty()) {
                row.status = QStringLiteral("OTHER_AWARD");
                row.dbId = byName.value(decree).first()->id;
            } else {
                row.status = QStringLiteral("NEW");
            }

            ++totals[row.status];
            ++perYear[row.year][row.status];
            ++fileCounts[row.status];
            current.insert(rowKey(row.file, row.index), row);
            if (row.status == QLatin1String("EXACT")) {
                exactIndexes.push_back(row.index);
                if (!allPersons)
                    continue;
            }
            persons << cell(row.file) << '\t' << row.year << '\t' << row.index << '\t' << row.status << '\t'
                    << row.awardId << '\t' << row.dbId << '\t' << cell(row.name) << '\t' << cell(row.description)
                    << '\t' << cell(row.rank) << '\t' << (row.posthumous ? 1 : 0) << '\t' << cell(row.dbDescription)
                    << '\t' << cell(row.dbRank) << '\t' << (row.dbPosthumous ? 1 : 0) << '\t' << row.diffFields
                    << '\t' << info.Ukaz.Number << '\t' << info.Ukaz.Date.toString(Qt::ISODate) << '\t'
                    << (belarusian ? "by" : "ru") << '\t' << cell(QString::fromStdWString(info.SourceName)) << '\t'
                    << (info.SourceCase == Dative ? "dat" : info.SourceCase == Accusative ? "acc" : "nom") << '\n';
        }
        if (parser.m_Persons.empty())
            ++emptyFiles;
        const QString year = docs.relativeFilePath(QFileInfo(path).path()).section(QLatin1Char('/'), 0, 0);
        filesOut << cell(relative) << '\t' << year << '\t' << parser.m_Persons.size() << '\t'
                 << fileCounts.value(QStringLiteral("EXACT")) << '\t' << fileCounts.value(QStringLiteral("DIFF")) << '\t'
                 << fileCounts.value(QStringLiteral("OTHER_AWARD")) << '\t' << fileCounts.value(QStringLiteral("NEW")) << '\t'
                 << fileCounts.value(QStringLiteral("BAD_HEADER")) << '\t' << cell(readError) << '\t'
                 << indexRanges(exactIndexes) << '\t' << (useGroup ? 1 : 0) << '\t' << cell(groupText) << '\n';
        if ((f + 1) % 100 == 0 || f + 1 == files.size())
            console() << (f + 1) << "/" << files.size() << " files, " << timer.elapsed() / 1000 << " s\n" << Qt::flush;
    }
    personsFile.close();
    filesFile.close();

    // summary
    int total = 0;
    for (int value : totals)
        total += value;
    QString summary;
    QTextStream s(&summary);
    s << "Отчёт парсера указов\n"
      << "база: " << cli.value(dbOption) << " (" << catalog.people().size() << " записей)\n"
      << "указы: " << docs.path() << " (" << files.size() << " файлов, без людей: " << emptyFiles
      << ", со сведениями о группе: " << groupFiles << ")\n"
      << "словари: " << (dictRu.isEmpty() ? QStringLiteral("-") : dictRu) << ", " << (dictBy.isEmpty() ? QStringLiteral("-") : dictBy) << "\n\n"
      << "Людей разобрано: " << total << "\n";
    for (const QString& status : kStatuses)
        s << "  " << status.leftJustified(12) << QString::number(totals.value(status)).rightJustified(7) << "  " << percent(totals.value(status), total) << "\n";
    s << "\nРасхождения в найденных записях (DIFF), по полям:\n";
    for (auto it = diffFields.cbegin(); it != diffFields.cend(); ++it)
        s << "  " << it.key().leftJustified(12) << QString::number(it.value()).rightJustified(7) << "\n";
    s << "\nПо годам (год: людей, EXACT, DIFF, OTHER_AWARD, NEW, BAD_HEADER):\n";
    for (auto it = perYear.cbegin(); it != perYear.cend(); ++it) {
        int yearTotal = 0;
        for (int value : it.value())
            yearTotal += value;
        s << "  " << (it.key() ? QString::number(it.key()) : QStringLiteral("----")) << ": " << QString::number(yearTotal).rightJustified(6);
        for (const QString& status : kStatuses)
            s << QString::number(it.value().value(status)).rightJustified(7);
        s << "   EXACT " << percent(it.value().value(QStringLiteral("EXACT")), yearTotal) << "\n";
    }

    if (cli.isSet(compareOption)) {
        const QHash<QString, Row> previous = readRows(QDir(cli.value(compareOption)).filePath(QStringLiteral("persons.tsv")));
        // persons that matched exactly last time are not in its persons.tsv
        const QSet<QString> previousExact = readExactKeys(QDir(cli.value(compareOption)).filePath(QStringLiteral("files.tsv")));
        QFile changesFile(QDir(outDir).filePath(QStringLiteral("changes.tsv")));
        changesFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream changes(&changesFile);
        changes.setEncoding(QStringConverter::Utf8);
        changes << "kind\tfile\tindex\twas_status\tnow_status\twas_name\tnow_name\twas_description\tnow_description\twas_rank\tnow_rank\n";
        int better = 0, worse = 0, other = 0, appeared = 0, vanished = 0;
        QMap<QString, int> transitions;
        const auto rank = [](const QString& status) { return status == QLatin1String("EXACT") ? 2 : status == QLatin1String("DIFF") ? 1 : 0; };
        for (auto it = current.cbegin(); it != current.cend(); ++it) {
            const Row& now = it.value();
            const auto was = previous.constFind(it.key());
            if (was == previous.cend() && previousExact.contains(it.key())) {
                if (now.status == QLatin1String("EXACT"))
                    continue;
                // matched the catalog last time: the old values were the catalog's
                ++worse;
                ++transitions[QStringLiteral("EXACT -> ") + now.status];
                changes << "worse\t" << cell(now.file) << '\t' << now.index << "\tEXACT\t" << now.status << "\t\t"
                        << cell(now.name) << '\t' << cell(now.dbDescription) << '\t' << cell(now.description) << '\t'
                        << cell(now.dbRank) << '\t' << cell(now.rank) << '\n';
                continue;
            }
            if (was == previous.cend()) {
                ++appeared;
                changes << "appeared\t" << cell(now.file) << '\t' << now.index << "\t\t" << now.status << "\t\t" << cell(now.name) << "\t\t" << cell(now.description) << "\t\t" << cell(now.rank) << '\n';
                continue;
            }
            if (was->status == now.status && was->name == cell(now.name) && was->description == cell(now.description) && was->rank == cell(now.rank))
                continue;
            QString kind;
            if (rank(now.status) > rank(was->status)) {
                kind = QStringLiteral("better");
                ++better;
            } else if (rank(now.status) < rank(was->status)) {
                kind = QStringLiteral("worse");
                ++worse;
            } else {
                kind = QStringLiteral("changed");
                ++other;
            }
            if (was->status != now.status)
                ++transitions[was->status + QStringLiteral(" -> ") + now.status];
            changes << kind << '\t' << cell(now.file) << '\t' << now.index << '\t' << was->status << '\t' << now.status << '\t'
                    << was->name << '\t' << cell(now.name) << '\t' << was->description << '\t' << cell(now.description) << '\t'
                    << was->rank << '\t' << cell(now.rank) << '\n';
        }
        for (const QString& key : previousExact) {
            if (!current.contains(key)) {
                ++vanished;
                changes << "vanished\t" << key.section(QChar(0x1F), 0, 0) << '\t' << key.section(QChar(0x1F), 1) << "\tEXACT\t\t\t\t\t\t\t\n";
            }
        }
        for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
            if (!current.contains(it.key())) {
                ++vanished;
                changes << "vanished\t" << it->file << '\t' << it->index << '\t' << it->status << "\t\t" << it->name << "\t\t" << it->description << "\t\t" << it->rank << "\t\n";
            }
        }
        s << "\nСравнение с " << cli.value(compareOption) << ":\n"
          << "  стало лучше (к EXACT/DIFF):   " << better << "\n"
          << "  стало хуже:                    " << worse << "\n"
          << "  изменилось без смены статуса:  " << other << "\n"
          << "  новые строки / пропавшие:      " << appeared << " / " << vanished << "\n";
        for (auto it = transitions.cbegin(); it != transitions.cend(); ++it)
            s << "    " << it.key() << ": " << it.value() << "\n";
        s << "  подробно: changes.tsv\n";
    }
    s.flush();

    QFile summaryFile(QDir(outDir).filePath(QStringLiteral("summary.txt")));
    if (summaryFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&summaryFile);
        out.setEncoding(QStringConverter::Utf8);
        out << summary;
    }
    console() << "\n" << summary;
    return 0;
}
