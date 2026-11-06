// Catalog on a small SQLite database created by the test: duplicate
// detection, Latin "i" in Belarusian fields, save/remove bookkeeping, backup.

#include "Catalog.h"
#include "ImportMatch.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

namespace {

struct Row
{
    QString name;
    QString nameBy;
    QString personal;
    QString personalBy;
    QString rankBy;
    QString awardedFor;
    int ukaseNo = 10;
    QString date = QStringLiteral("2015-01-28");
    int language = kLangBelarusian;
};

bool createDatabase(const QString& path, const QVector<Row>& rows)
{
    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("setup"));
        db.setDatabaseName(path);
        ok = db.open();
        QSqlQuery query(db);
        ok = ok && query.exec(QStringLiteral(
            "CREATE TABLE Awards (ID INTEGER PRIMARY KEY, Type INTEGER, Priority INTEGER, Name TEXT, NameBY TEXT)"));
        ok = ok && query.exec(QStringLiteral(
            "CREATE TABLE Kavalers (ID INTEGER PRIMARY KEY AUTOINCREMENT, Award_ID INTEGER, Name TEXT, Name_BY TEXT, "
            "Personal_Info TEXT, Personal_Info_BY TEXT, Awarded_For TEXT, Rank TEXT, Rank_BY TEXT, "
            "Ukase_No INTEGER, Ukase_Date TEXT, Posthumous INTEGER, Ukase_Lang INTEGER)"));
        ok = ok && query.exec(QStringLiteral(
            "INSERT INTO Awards VALUES (14, 3, 20, 'Медаль \"За отвагу\"', 'Медаль \"За адвагу\"')"));
        for (const Row& row : rows) {
            query.prepare(QStringLiteral(
                "INSERT INTO Kavalers (Award_ID, Name, Name_BY, Personal_Info, Personal_Info_BY, Awarded_For, "
                "Rank_BY, Ukase_No, Ukase_Date, Posthumous, Ukase_Lang) VALUES (14, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)"));
            query.addBindValue(row.name);
            query.addBindValue(row.nameBy);
            query.addBindValue(row.personal);
            query.addBindValue(row.personalBy);
            query.addBindValue(row.awardedFor);
            query.addBindValue(row.rankBy);
            query.addBindValue(row.ukaseNo);
            query.addBindValue(row.date);
            query.addBindValue(row.language);
            ok = ok && query.exec();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("setup"));
    return ok;
}

// value of one column of one record, read directly from the file
QString stored(const QString& path, int id, const QString& column)
{
    QString value;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("check"));
        db.setDatabaseName(path);
        db.open();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT %1 FROM Kavalers WHERE ID = ?").arg(column));
        query.addBindValue(id);
        if (query.exec() && query.next())
            value = query.value(0).toString();
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("check"));
    return value;
}

// a person as the decree parser returns it (Belarusian decree № 13 of 11.01.2021)
PersonInfo parsed(const QString& name, int awardId = 14, const QString& file = QStringLiteral("a.docx"))
{
    PersonInfo info;
    info.Name = name.toStdWString();
    info.AwardId = awardId;
    info.FileName = file.toStdWString();
    info.Ukaz.Number = 13;
    info.Ukaz.Date = QDate(2021, 1, 11);
    info.Ukaz.RussianLang = false;
    return info;
}

const QString kShulha = QStringLiteral("Шульга Валерый Ігаравіч"); // in the test catalog once

const Person* byNameBy(const Catalog& catalog, const QString& nameBy)
{
    for (const Person& person : catalog.people()) {
        if (person.nameBy == nameBy)
            return &person;
    }
    return nullptr;
}

} // namespace

class TestCatalog : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void containsIgnoresLatinI();
    void secondAwardInOneDecreeIsNotADuplicate();
    void namesakesAreCounted();
    void importKnownPerson();
    void importNamesakes();
    void importSameDecreeFromTwoFiles();
    void importSecondAward();
    void importNamesakeHiddenByFilter();
    void findLatinLetters();
    void saveFixesBelarusianFieldsOnly();
    void fixWholeDatabaseWithBackup();
    void identityFollowsEditsAndRemoval();
    void signalsOncePerChange();
    void derivedDataFollowsChanges();

private:
    QTemporaryDir m_dir;
    QString m_path;
    Catalog* m_catalog = nullptr;
};

void TestCatalog::init()
{
    QVERIFY(m_dir.isValid());
    m_path = m_dir.filePath(QStringLiteral("catalog-%1.sqlite").arg(QTest::currentTestFunction()));
    const QVector<Row> rows = {
        // Latin capital and small i in a Belarusian name and position
        {QStringLiteral("Вышиванюк Игорь Михайлович"), QStringLiteral("Вышыванюк Iгар Мiхайлавіч"),
         QStringLiteral("контролер"), QStringLiteral("кантралёр аддзялення пагранiчнага кантролю"),
         QStringLiteral("прапаршчык"), QString()},
        // conjunction "i" typed in Latin, Roman numeral must stay
        {QString(), QStringLiteral("Іваноў Ігар Пятровіч"), QString(),
         QStringLiteral("загадчык кафедры лагістыкі i цэнавай палітыкі, юрыст I класа"), QString(),
         QStringLiteral("За асабiсты ўклад")},
        // Russian decree: Russian text is never touched
        {QStringLiteral("Петров Петр Петрович"), QString(), QStringLiteral("водитель i механик"), QString(),
         QString(), QStringLiteral("За заслуги i труд"), 11, QStringLiteral("1999-04-09"), kLangRussian},
        // clean record
        {QString(), QStringLiteral("Шульга Валерый Ігаравіч"), QString(), QStringLiteral("галоўны саветнік"),
         QString(), QString(), 13, QStringLiteral("2021-01-11")},
    };
    QVERIFY(createDatabase(m_path, rows));
    m_catalog = new Catalog;
    QString error;
    QVERIFY2(m_catalog->open(m_path, &error), qPrintable(error));
    QCOMPARE(m_catalog->people().size(), rows.size());
}

void TestCatalog::cleanup()
{
    delete m_catalog;
    m_catalog = nullptr;
}

// a decree imported again must find the person whatever "i" was typed
void TestCatalog::containsIgnoresLatinI()
{
    Person probe;
    probe.ukaseNo = 10;
    probe.ukaseDate = QDate(2015, 1, 28);
    probe.awardId = 14;
    probe.nameBy = QStringLiteral("Вышыванюк Ігар Міхайлавіч"); // stored as "Iгар Мiхайлавіч"
    QVERIFY(m_catalog->contains(probe));
    probe.nameBy = QStringLiteral("Вышыванюк Iгар Мiхайлавiч");
    QVERIFY(m_catalog->contains(probe));
    probe.nameBy = QStringLiteral("Іваноў Iгар Пятровiч"); // stored in Cyrillic
    QVERIFY(m_catalog->contains(probe));

    probe.ukaseNo = 11; // other decree
    QVERIFY(!m_catalog->contains(probe));
    probe.ukaseNo = 10;
    probe.ukaseDate = QDate(2016, 1, 28); // same number, other year
    QVERIFY(!m_catalog->contains(probe));
}

// an athlete gets an order and the title "Заслуженный мастер спорта" in one decree
void TestCatalog::secondAwardInOneDecreeIsNotADuplicate()
{
    Person title = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    QVERIFY(m_catalog->contains(title));
    title.id = 0;
    title.awardId = 34;
    QVERIFY(!m_catalog->contains(title));

    QString error;
    QVERIFY(m_catalog->save(title, &error));
    QVERIFY(m_catalog->contains(title));
    title.awardId = 14;
    QCOMPARE(m_catalog->count(title), 1); // the first award is still counted once
}

// two different people with the same name and award in one decree
void TestCatalog::namesakesAreCounted()
{
    Person first = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    QCOMPARE(m_catalog->count(first), 1);

    Person second = first;
    second.id = 0;
    second.personalBy = QStringLiteral("міліцыянер-вадзіцель");
    QString error;
    QVERIFY(m_catalog->save(second, &error));
    QCOMPARE(m_catalog->count(first), 2);

    QVERIFY(m_catalog->remove({second.id}, &error));
    QCOMPARE(m_catalog->count(first), 1);
}

void TestCatalog::importKnownPerson()
{
    const std::vector<PersonInfo> persons = {parsed(kShulha), parsed(QStringLiteral("Новы Чалавек Іванавіч"))};
    const QVector<ImportMatch> matches = matchImported(persons, *m_catalog);
    QVERIFY(matches.at(0).known);
    QVERIFY(!matches.at(1).known);
    QCOMPARE(matches.at(0).namesakes, 1);
    QCOMPARE(personsToImport(persons, {}, *m_catalog), QVector<int>({1}));

    // Latin "I" in the decree, Cyrillic in the catalog (and vice versa)
    const std::vector<PersonInfo> latin = {parsed(QStringLiteral("Шульга Валерый Iгаравiч"))};
    QVERIFY(matchImported(latin, *m_catalog).at(0).known);
}

// two people "Шульга Валерый Ігаравіч" in one decree, the catalog has one of them
void TestCatalog::importNamesakes()
{
    const std::vector<PersonInfo> persons = {parsed(kShulha), parsed(QStringLiteral("Іншы Чалавек Іванавіч")),
                                             parsed(kShulha)};
    const QVector<ImportMatch> matches = matchImported(persons, *m_catalog);
    QVERIFY(matches.at(0).known);
    QVERIFY(!matches.at(2).known);
    QCOMPARE(matches.at(0).namesakes, 2);
    QCOMPARE(matches.at(2).namesakes, 2);
    QCOMPARE(matches.at(1).namesakes, 1);
    QCOMPARE(personsToImport(persons, {}, *m_catalog), QVector<int>({1, 2}));
}

// a folder with two copies of the same decree must not import it twice
void TestCatalog::importSameDecreeFromTwoFiles()
{
    const QString newcomer = QStringLiteral("Новы Чалавек Іванавіч");
    const std::vector<PersonInfo> persons = {
        parsed(newcomer), parsed(newcomer), // namesakes in a.docx
        parsed(newcomer, 14, QStringLiteral("b.docx")), parsed(newcomer, 14, QStringLiteral("b.docx")),
    };
    QCOMPARE(personsToImport(persons, {}, *m_catalog), QVector<int>({0, 1}));
    QCOMPARE(matchImported(persons, *m_catalog).at(2).namesakes, 2);
}

// order and honorary title for one person in one decree
void TestCatalog::importSecondAward()
{
    const std::vector<PersonInfo> persons = {parsed(kShulha, 14), parsed(kShulha, 34)};
    const QVector<ImportMatch> matches = matchImported(persons, *m_catalog);
    QVERIFY(matches.at(0).known);
    QVERIFY(!matches.at(1).known);
    QCOMPARE(matches.at(1).namesakes, 1); // other award: not a namesake
    QCOMPARE(personsToImport(persons, {}, *m_catalog), QVector<int>({1}));
}

// "import selected": a namesake outside the selection still counts
void TestCatalog::importNamesakeHiddenByFilter()
{
    const std::vector<PersonInfo> persons = {parsed(kShulha), parsed(kShulha)};
    QCOMPARE(personsToImport(persons, {false, true}, *m_catalog), QVector<int>({1}));
    QCOMPARE(personsToImport(persons, {true, false}, *m_catalog), QVector<int>());
}

void TestCatalog::findLatinLetters()
{
    QVector<Person> fixed;
    const Catalog::LetterFix found = m_catalog->findLatinLetters(&fixed);
    QCOMPARE(found.records, 2);
    QCOMPARE(fixed.size(), 2);
    QCOMPARE(found.fields.value(QStringLiteral("ФИО (белор.)")), 1);
    QCOMPARE(found.fields.value(QStringLiteral("Личные данные (белор.)")), 2);
    QCOMPARE(found.fields.value(QStringLiteral("Награжден за")), 1);
    QCOMPARE(found.fields.value(QStringLiteral("Звание (белор.)")), 0);

    QCOMPARE(fixed.at(0).nameBy, QStringLiteral("Вышыванюк Ігар Міхайлавіч"));
    QCOMPARE(fixed.at(0).personalBy, QStringLiteral("кантралёр аддзялення пагранічнага кантролю"));
    QCOMPARE(fixed.at(0).name, QStringLiteral("Вышиванюк Игорь Михайлович"));
    QCOMPARE(fixed.at(1).personalBy, QStringLiteral("загадчык кафедры лагістыкі і цэнавай палітыкі, юрыст I класа"));
    QCOMPARE(fixed.at(1).awardedFor, QStringLiteral("За асабісты ўклад"));
    QVERIFY(!found.samples.isEmpty());

    // nothing is written by the search
    QCOMPARE(stored(m_path, fixed.at(0).id, QStringLiteral("Name_BY")), QStringLiteral("Вышыванюк Iгар Мiхайлавіч"));
}

void TestCatalog::saveFixesBelarusianFieldsOnly()
{
    const Person* russian = nullptr;
    for (const Person& person : m_catalog->people()) {
        if (person.language == kLangRussian)
            russian = &person;
    }
    QVERIFY(russian);
    Person ru = *russian;
    ru.rank = QStringLiteral("кaпитaн"); // Latin "a" twice
    QString error;
    QVERIFY(m_catalog->save(ru, &error));
    QCOMPARE(stored(m_path, ru.id, QStringLiteral("Rank")), QStringLiteral("капитан"));
    // a Latin "i" is not a Russian letter: Russian text keeps it (fixed by hand)
    QCOMPARE(stored(m_path, ru.id, QStringLiteral("Personal_Info")), QStringLiteral("водитель i механик"));
    QCOMPARE(stored(m_path, ru.id, QStringLiteral("Awarded_For")), QStringLiteral("За заслуги i труд"));

    // Russian names are compared without look-alikes too
    Person probe = ru;
    probe.id = 0;
    probe.name = QStringLiteral("Петрoв Петр Петрoвич");
    QVERIFY(m_catalog->contains(probe));

    Person by = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    by.rankBy = QStringLiteral("палкоўнiк");
    by.nameBy = QStringLiteral("Шульга Валерый Iгаравiч");
    QVERIFY(m_catalog->save(by, &error));
    QCOMPARE(by.rankBy, QStringLiteral("палкоўнік")); // the saved copy is fixed too
    QCOMPARE(stored(m_path, by.id, QStringLiteral("Rank_BY")), QStringLiteral("палкоўнік"));
    QCOMPARE(stored(m_path, by.id, QStringLiteral("Name_BY")), QStringLiteral("Шульга Валерый Ігаравіч"));
    QVERIFY(byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч")));
}

void TestCatalog::fixWholeDatabaseWithBackup()
{
    const QString backupPath = m_dir.filePath(QStringLiteral("backup.sqlite"));
    QString error;
    QVERIFY2(m_catalog->backup(backupPath, &error), qPrintable(error));
    QVERIFY(QFile::exists(backupPath));
    QVERIFY(!m_catalog->backup(backupPath, &error)); // never overwrites

    QVector<Person> fixed;
    m_catalog->findLatinLetters(&fixed);
    QVERIFY(m_catalog->saveMany(fixed, &error));
    QCOMPARE(m_catalog->findLatinLetters().records, 0);
    QCOMPARE(stored(m_path, fixed.at(0).id, QStringLiteral("Name_BY")), QStringLiteral("Вышыванюк Ігар Міхайлавіч"));

    // the backup keeps the old text
    QCOMPARE(stored(backupPath, fixed.at(0).id, QStringLiteral("Name_BY")), QStringLiteral("Вышыванюк Iгар Мiхайлавіч"));

    // and the catalog reopens with the same content
    delete m_catalog;
    m_catalog = new Catalog;
    QVERIFY(m_catalog->open(m_path, &error));
    QCOMPARE(m_catalog->findLatinLetters().records, 0);
    QCOMPARE(m_catalog->people().size(), 4);
}

void TestCatalog::identityFollowsEditsAndRemoval()
{
    QString error;
    Person original = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    QVERIFY(m_catalog->contains(original));

    Person renamed = original;
    renamed.nameBy = QStringLiteral("Шульга Валерый Пятровіч");
    QVERIFY(m_catalog->save(renamed, &error));
    QVERIFY(m_catalog->contains(renamed));
    QVERIFY(!m_catalog->contains(original));

    // two records with the same key: changing one keeps the other found
    Person twin = renamed;
    twin.id = 0;
    QVERIFY(m_catalog->save(twin, &error));
    QVERIFY(twin.id > 0);
    Person other = twin;
    other.nameBy = QStringLiteral("Іншы Чалавек Іванавіч");
    QVERIFY(m_catalog->save(other, &error));
    QVERIFY(m_catalog->contains(renamed));
    QVERIFY(m_catalog->contains(other));

    QVERIFY(m_catalog->remove({other.id}, &error));
    QVERIFY(!m_catalog->contains(other));
    QVERIFY(m_catalog->contains(renamed));
    QCOMPARE(m_catalog->people().size(), 4);
}

void TestCatalog::signalsOncePerChange()
{
    int about = 0;
    int changed = 0;
    int peopleAtAbout = -1;
    connect(m_catalog, &Catalog::aboutToChange, this, [&] {
        ++about;
        peopleAtAbout = m_catalog->people().size();
    });
    connect(m_catalog, &Catalog::dataChanged, this, [&] { ++changed; });

    QString error;
    Person person;
    person.awardId = 14;
    person.nameBy = QStringLiteral("Новы Чалавек Іванавіч");
    person.ukaseNo = 1;
    person.ukaseDate = QDate(2024, 1, 1);
    QVERIFY(m_catalog->save(person, &error));
    QCOMPARE(about, 1);
    QCOMPARE(changed, 1);
    QCOMPARE(peopleAtAbout, 4); // views still see the old data at aboutToChange

    QVector<Person> batch = m_catalog->people();
    QVERIFY(m_catalog->saveMany(batch, &error));
    QCOMPARE(about, 2);
    QCOMPARE(changed, 2);

    QVector<Person> nothing;
    QVERIFY(m_catalog->saveMany(nothing, &error));
    QCOMPARE(changed, 2);
}

// statistics and the name dictionary are rebuilt lazily after a change
void TestCatalog::derivedDataFollowsChanges()
{
    int total = 0;
    for (const StatMonth& row : m_catalog->stats())
        total += row.total;
    QCOMPARE(total, 4);
    QVERIFY(m_catalog->translateName(QStringLiteral("Вышыванюк Ігар Міхайлавіч")).isEmpty());

    QString error;
    Person person = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    person.name = QStringLiteral("Шульга Валерий Игоревич");
    QVERIFY(m_catalog->save(person, &error));
    QCOMPARE(m_catalog->translateName(QStringLiteral("Шульга Валерый Ігаравіч")),
             QStringList({QStringLiteral("Шульга Валерий Игоревич")}));

    person.id = 0;
    person.ukaseDate = QDate(2021, 3, 1);
    QVERIFY(m_catalog->save(person, &error));
    total = 0;
    for (const StatMonth& row : m_catalog->stats())
        total += row.total;
    QCOMPARE(total, 5);
}

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
