// Catalog на маленькой базе SQLite, которую создаёт тест: поиск
// дублей, латинская «i» в белорусских полях, учёт сохранения и удаления, резервная копия.

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

// значение одного столбца одной записи, прочитанное прямо из файла
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

// человек, как его возвращает парсер указов (белорусский указ № 13 от 11.01.2021)
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

const QString kShulha = QStringLiteral("Шульга Валерый Ігаравіч"); // в тестовом каталоге один раз

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
    void translateNamePrefersTheWholeName();
    void translateNameMatchesIdentityKey();
    void professionRuPrefersTheFrequentSpelling();
    void rankRuPrefersTheFrequentSpelling();
    void fixQuotesFollowsTheDecree();
    void russianNameConflicts();
    void savingDeletedRecordFails();
    void failedOpenKeepsCurrentDatabase();
    void replaceTextSavesLikeAnEdit();
    void organizationFlagIsStored();

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
        // латинские заглавная и строчная i в белорусском имени и должности
        {QStringLiteral("Вышиванюк Игорь Михайлович"), QStringLiteral("Вышыванюк Iгар Мiхайлавіч"),
         QStringLiteral("контролер"), QStringLiteral("кантралёр аддзялення пагранiчнага кантролю"),
         QStringLiteral("прапаршчык"), QString()},
        // союз «i», набранный латиницей; римская цифра должна остаться
        {QString(), QStringLiteral("Іваноў Ігар Пятровіч"), QString(),
         QStringLiteral("загадчык кафедры лагістыкі i цэнавай палітыкі, юрыст I класа"), QString(),
         QStringLiteral("За асабiсты ўклад")},
        // русский указ: русский текст не трогается
        {QStringLiteral("Петров Петр Петрович"), QString(), QStringLiteral("водитель i механик"), QString(),
         QString(), QStringLiteral("За заслуги i труд"), 11, QStringLiteral("1999-04-09"), kLangRussian},
        // чистая запись
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

// повторный импорт указа должен найти человека, какой бы «i» ни набрали
void TestCatalog::containsIgnoresLatinI()
{
    Person probe;
    probe.ukaseNo = 10;
    probe.ukaseDate = QDate(2015, 1, 28);
    probe.awardId = 14;
    probe.nameBy = QStringLiteral("Вышыванюк Ігар Міхайлавіч"); // в базе лежит «Iгар Мiхайлавіч»
    QVERIFY(m_catalog->contains(probe));
    probe.nameBy = QStringLiteral("Вышыванюк Iгар Мiхайлавiч");
    QVERIFY(m_catalog->contains(probe));
    probe.nameBy = QStringLiteral("Іваноў Iгар Пятровiч"); // в базе кириллицей
    QVERIFY(m_catalog->contains(probe));

    probe.ukaseNo = 11; // другой указ
    QVERIFY(!m_catalog->contains(probe));
    probe.ukaseNo = 10;
    probe.ukaseDate = QDate(2016, 1, 28); // тот же номер, другой год
    QVERIFY(!m_catalog->contains(probe));
}

// спортсмен получает орден и звание «Заслуженный мастер спорта» в одном указе
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
    QCOMPARE(m_catalog->count(title), 1); // первая награда по-прежнему считается один раз
}

// двое разных людей с одним именем и наградой в одном указе
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

    // латинская «I» в указе, кириллическая в каталоге (и наоборот)
    const std::vector<PersonInfo> latin = {parsed(QStringLiteral("Шульга Валерый Iгаравiч"))};
    QVERIFY(matchImported(latin, *m_catalog).at(0).known);
}

// двое «Шульга Валерый Ігаравіч» в одном указе, в каталоге один из них
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

// папка с двумя копиями одного указа не должна импортировать его дважды
void TestCatalog::importSameDecreeFromTwoFiles()
{
    const QString newcomer = QStringLiteral("Новы Чалавек Іванавіч");
    const std::vector<PersonInfo> persons = {
        parsed(newcomer), parsed(newcomer), // тёзки в a.docx
        parsed(newcomer, 14, QStringLiteral("b.docx")), parsed(newcomer, 14, QStringLiteral("b.docx")),
    };
    QCOMPARE(personsToImport(persons, {}, *m_catalog), QVector<int>({0, 1}));
    QCOMPARE(matchImported(persons, *m_catalog).at(2).namesakes, 2);
}

// орден и почётное звание одному человеку в одном указе
void TestCatalog::importSecondAward()
{
    const std::vector<PersonInfo> persons = {parsed(kShulha, 14), parsed(kShulha, 34)};
    const QVector<ImportMatch> matches = matchImported(persons, *m_catalog);
    QVERIFY(matches.at(0).known);
    QVERIFY(!matches.at(1).known);
    QCOMPARE(matches.at(1).namesakes, 1); // другая награда: не тёзка
    QCOMPARE(personsToImport(persons, {}, *m_catalog), QVector<int>({1}));
}

// «импорт выбранных»: тёзка вне выделения всё равно считается
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

    // поиск ничего не записывает
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
    ru.rank = QStringLiteral("кaпитaн"); // латинская «a» дважды
    QString error;
    QVERIFY(m_catalog->save(ru, &error));
    QCOMPARE(stored(m_path, ru.id, QStringLiteral("Rank")), QStringLiteral("капитан"));
    // латинская «i» — не русская буква: русский текст её оставляет (правят вручную)
    QCOMPARE(stored(m_path, ru.id, QStringLiteral("Personal_Info")), QStringLiteral("водитель i механик"));
    QCOMPARE(stored(m_path, ru.id, QStringLiteral("Awarded_For")), QStringLiteral("За заслуги i труд"));

    // русские имена тоже сравниваются без букв-двойников
    Person probe = ru;
    probe.id = 0;
    probe.name = QStringLiteral("Петрoв Петр Петрoвич");
    QVERIFY(m_catalog->contains(probe));

    Person by = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    by.rankBy = QStringLiteral("палкоўнiк");
    by.nameBy = QStringLiteral("Шульга Валерый Iгаравiч");
    QVERIFY(m_catalog->save(by, &error));
    QCOMPARE(by.rankBy, QStringLiteral("палкоўнік")); // сохранённая копия тоже исправлена
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
    QVERIFY(!m_catalog->backup(backupPath, &error)); // никогда не перезаписывает

    QVector<Person> fixed;
    m_catalog->findLatinLetters(&fixed);
    QVERIFY(m_catalog->saveMany(fixed, &error));
    QCOMPARE(m_catalog->findLatinLetters().records, 0);
    QCOMPARE(stored(m_path, fixed.at(0).id, QStringLiteral("Name_BY")), QStringLiteral("Вышыванюк Ігар Міхайлавіч"));

    // резервная копия хранит старый текст
    QCOMPARE(stored(backupPath, fixed.at(0).id, QStringLiteral("Name_BY")), QStringLiteral("Вышыванюк Iгар Мiхайлавіч"));

    // и каталог открывается снова с тем же содержимым
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

    // две записи с одним ключом: правка одной оставляет другую найденной
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
    QCOMPARE(peopleAtAbout, 4); // виды на aboutToChange ещё видят старые данные

    QVector<Person> batch = m_catalog->people();
    QVERIFY(m_catalog->saveMany(batch, &error));
    QCOMPARE(about, 2);
    QCOMPARE(changed, 2);

    QVector<Person> nothing;
    QVERIFY(m_catalog->saveMany(nothing, &error));
    QCOMPARE(changed, 2);
}

// статистика и словарь имён после изменения пересобираются лениво
void TestCatalog::derivedDataFollowsChanges()
{
    int total = 0;
    for (const StatMonth& row : m_catalog->stats())
        total += row.total;
    QCOMPARE(total, 4);
    QCOMPARE(m_catalog->translateName(QStringLiteral("Вышыванюк Ігар Міхайлавіч")),
             QStringList({QStringLiteral("Вышиванюк Игорь Михайлович")}));

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

// сохранённое полное имя побеждает фамилию, которую другие пишут иначе;
// невиданное имя составляется из частей; варианты — от самых частых написаний
void TestCatalog::translateNamePrefersTheWholeName()
{
    QString error;
    Person bobr;
    bobr.awardId = 14;
    bobr.ukaseDate = QDate(2020, 1, 1);
    bobr.ukaseNo = 1;
    bobr.nameBy = QStringLiteral("Бобр Адам Рыгоравіч");
    bobr.name = QStringLiteral("Бобер Адам Григорьевич");
    QVERIFY(m_catalog->save(bobr, &error));

    Person fox = bobr;
    fox.id = 0;
    fox.ukaseNo = 2;
    fox.ukaseDate = QDate(2020, 1, 2);
    fox.nameBy = QStringLiteral("Ліс Марк Рыгоравіч");
    fox.name = QStringLiteral("Лис Марк Григорьевич");
    QVERIFY(m_catalog->save(fox, &error));
    QCOMPARE(m_catalog->translateName(QStringLiteral("Бобр Марк Рыгоравіч")),
             QStringList({QStringLiteral("Бобер Марк Григорьевич")}));

    Person other = bobr;
    other.id = 0;
    other.ukaseNo = 3;
    other.ukaseDate = QDate(2020, 1, 3);
    other.nameBy = QStringLiteral("Бобр Янка Рыгоравіч");
    other.name = QStringLiteral("Бобров Ян Григорьевич");
    QVERIFY(m_catalog->save(other, &error));
    QCOMPARE(m_catalog->translateName(QStringLiteral("Бобр Адам Рыгоравіч")),
             QStringList({QStringLiteral("Бобер Адам Григорьевич")}));
    // у фамилии два написания поровну: первым то, что встретилось раньше, выбор за пользователем
    QCOMPARE(m_catalog->translateName(QStringLiteral("Бобр Марк Рыгоравіч")),
             QStringList({QStringLiteral("Бобер Марк Григорьевич"), QStringLiteral("Бобров Марк Григорьевич")}));

    Person repeat = bobr;
    repeat.id = 0;
    repeat.ukaseNo = 4;
    repeat.ukaseDate = QDate(2020, 2, 1);
    QVERIFY(m_catalog->save(repeat, &error));
    Person alt = bobr;
    alt.id = 0;
    alt.ukaseNo = 5;
    alt.ukaseDate = QDate(2020, 2, 2);
    alt.name = QStringLiteral("Бобров Адам Григорьевич");
    QVERIFY(m_catalog->save(alt, &error));
    QCOMPARE(m_catalog->translateName(QStringLiteral("Бобр Адам Рыгоравіч")),
             QStringList({QStringLiteral("Бобер Адам Григорьевич"), QStringLiteral("Бобров Адам Григорьевич")}));

    // частое написание фамилии побеждает: «Бобер» трижды, «Бобров» дважды
    QCOMPARE(m_catalog->translateName(QStringLiteral("Бобр Марк Рыгоравіч")),
             QStringList({QStringLiteral("Бобер Марк Григорьевич"), QStringLiteral("Бобров Марк Григорьевич")}));
    Person rival = other;
    rival.id = 0;
    rival.ukaseNo = 6;
    rival.ukaseDate = QDate(2020, 3, 1);
    QVERIFY(m_catalog->save(rival, &error));
    Person rival2 = rival;
    rival2.id = 0;
    rival2.ukaseNo = 7;
    QVERIFY(m_catalog->save(rival2, &error));
    QCOMPARE(m_catalog->translateName(QStringLiteral("Бобр Марк Рыгоравіч")),
             QStringList({QStringLiteral("Бобров Марк Григорьевич"), QStringLiteral("Бобер Марк Григорьевич")}));
}

// «Iгар» и «Ігар» — одно имя, в том числе когда полное имя составлено из частей
void TestCatalog::translateNameMatchesIdentityKey()
{
    QCOMPARE(m_catalog->translateName(QStringLiteral("Вышыванюк Ігар Міхайлавіч")),
             QStringList({QStringLiteral("Вышиванюк Игорь Михайлович")}));
    QCOMPARE(m_catalog->translateName(QStringLiteral("Вышыванюк Iгар Мiхайлавіч")),
             QStringList({QStringLiteral("Вышиванюк Игорь Михайлович")}));

    QString error;
    Person lis;
    lis.awardId = 14;
    lis.ukaseNo = 21;
    lis.ukaseDate = QDate(2020, 1, 1);
    lis.nameBy = QStringLiteral("Ліс Марк Рыгоравіч");
    lis.name = QStringLiteral("Лис Марк Григорьевич");
    QVERIFY(m_catalog->save(lis, &error));

    Person bobr;
    bobr.awardId = 14;
    bobr.ukaseNo = 22;
    bobr.ukaseDate = QDate(2020, 1, 2);
    bobr.nameBy = QStringLiteral("Бобр Iгар Рыгоравіч");
    bobr.name = QStringLiteral("Бобер Игорь Григорьевич");
    QVERIFY(m_catalog->save(bobr, &error));

    QCOMPARE(m_catalog->translateName(QStringLiteral("Ліс Ігар Рыгоравіч")),
             QStringList({QStringLiteral("Лис Игорь Григорьевич")}));
}

// более позднее написание, которое встречается чаще, побеждает; при равном счёте остаётся самый ранний указ
void TestCatalog::professionRuPrefersTheFrequentSpelling()
{
    QString error;
    Person early;
    early.awardId = 14;
    early.ukaseNo = 31;
    early.ukaseDate = QDate(2010, 1, 1);
    early.personalBy = QStringLiteral("слесар");
    early.personal = QStringLiteral("слесарь");
    QVERIFY(m_catalog->save(early, &error));

    Person later = early;
    later.id = 0;
    later.ukaseNo = 32;
    later.ukaseDate = QDate(2012, 1, 1);
    later.personal = QStringLiteral("токарь");
    QVERIFY(m_catalog->save(later, &error));

    Person laterAgain = later;
    laterAgain.id = 0;
    laterAgain.ukaseNo = 33;
    laterAgain.ukaseDate = QDate(2013, 1, 1);
    QVERIFY(m_catalog->save(laterAgain, &error));
    QCOMPARE(m_catalog->professionRu(QStringLiteral("слесар")), QStringLiteral("токарь"));

    Person tieEarly = early;
    tieEarly.id = 0;
    tieEarly.ukaseNo = 34;
    tieEarly.ukaseDate = QDate(2010, 6, 1);
    tieEarly.personalBy = QStringLiteral("цясляр");
    tieEarly.personal = QStringLiteral("столяр");
    QVERIFY(m_catalog->save(tieEarly, &error));

    Person tieOther = tieEarly;
    tieOther.id = 0;
    tieOther.ukaseNo = 35;
    tieOther.ukaseDate = QDate(2011, 1, 1);
    tieOther.personal = QStringLiteral("плотник");
    QVERIFY(m_catalog->save(tieOther, &error));

    Person tieEarlyAgain = tieEarly;
    tieEarlyAgain.id = 0;
    tieEarlyAgain.ukaseNo = 36;
    tieEarlyAgain.ukaseDate = QDate(2012, 1, 1);
    QVERIFY(m_catalog->save(tieEarlyAgain, &error));

    Person tieOtherAgain = tieOther;
    tieOtherAgain.id = 0;
    tieOtherAgain.ukaseNo = 37;
    tieOtherAgain.ukaseDate = QDate(2013, 1, 1);
    QVERIFY(m_catalog->save(tieOtherAgain, &error));
    QCOMPARE(m_catalog->professionRu(QStringLiteral("цясляр")), QStringLiteral("столяр"));
}

// побеждает написание, которое встречается чаще, даже если другое по имени идёт первым
void TestCatalog::rankRuPrefersTheFrequentSpelling()
{
    QString error;
    Person firstByName;
    firstByName.awardId = 14;
    firstByName.ukaseNo = 41;
    firstByName.ukaseDate = QDate(2015, 1, 1);
    firstByName.name = QStringLiteral("Абрамов Абрам");
    firstByName.rankBy = QStringLiteral("палкоўнік");
    firstByName.rank = QStringLiteral("полковник");
    QVERIFY(m_catalog->save(firstByName, &error));

    Person frequent = firstByName;
    frequent.id = 0;
    frequent.ukaseNo = 42;
    frequent.ukaseDate = QDate(2016, 1, 1);
    frequent.name = QStringLiteral("Борисов Борис");
    frequent.rank = QStringLiteral("полкаўнік");
    QVERIFY(m_catalog->save(frequent, &error));

    Person frequentAgain = frequent;
    frequentAgain.id = 0;
    frequentAgain.ukaseNo = 43;
    frequentAgain.ukaseDate = QDate(2017, 1, 1);
    frequentAgain.name = QStringLiteral("Яковлев Яков");
    QVERIFY(m_catalog->save(frequentAgain, &error));
    QCOMPARE(m_catalog->rankRu(QStringLiteral("палкоўнік")), QStringLiteral("полкаўнік"));

    Person laterName = firstByName;
    laterName.id = 0;
    laterName.ukaseNo = 44;
    laterName.ukaseDate = QDate(2010, 1, 1);
    laterName.name = QStringLiteral("Яковлев Яков");
    laterName.rankBy = QStringLiteral("маёр");
    laterName.rank = QStringLiteral("майор");
    QVERIFY(m_catalog->save(laterName, &error));

    Person earlierName = laterName;
    earlierName.id = 0;
    earlierName.ukaseNo = 45;
    earlierName.ukaseDate = QDate(2018, 1, 1);
    earlierName.name = QStringLiteral("Абрамов Абрам");
    earlierName.rank = QStringLiteral("маёр");
    QVERIFY(m_catalog->save(earlierName, &error));
    QCOMPARE(m_catalog->rankRu(QStringLiteral("маёр")), QStringLiteral("маёр"));
}

void TestCatalog::fixQuotesFollowsTheDecree()
{
    Person person;
    person.personalBy = QStringLiteral("установа «Цэнтр» – філіял");
    person.personal = QStringLiteral("учреждение \"Цэнтр\" - филиал");
    QVERIFY(Catalog::fixQuotes(person));
    QCOMPARE(person.personal, QStringLiteral("учреждение «Цэнтр» – филиал"));

    person.personalBy = QStringLiteral("установа «Цэнтр»");
    person.personal = QStringLiteral("учреждение « Цэнтр »");
    QVERIFY(Catalog::fixQuotes(person));
    QCOMPARE(person.personal, QStringLiteral("учреждение «Цэнтр»"));

    person.personal = QString(u"учреждение \u201CЦэнтр\u201D");
    QVERIFY(Catalog::fixQuotes(person));
    QCOMPARE(person.personal, QStringLiteral("учреждение «Цэнтр»"));
    QVERIFY(!Catalog::fixQuotes(person));
}

// одно белорусское полное имя с двумя русскими написаниями попадает в отчёт; одно написание
// и вторая запись, которая отличается только пробелами, — нет
void TestCatalog::russianNameConflicts()
{
    QCOMPARE(m_catalog->russianNameConflicts().size(), 0);

    QString error;
    Person first = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    first.name = QStringLiteral("Шульга Валерий Игоревич");
    QVERIFY(m_catalog->save(first, &error));

    Person spaced = first;
    spaced.id = 0;
    spaced.ukaseNo = 99;
    spaced.ukaseDate = QDate(2022, 2, 2);
    spaced.name = QStringLiteral("Шульга  Валерий   Игоревич");
    spaced.nameBy = QStringLiteral("Шульга  Валерый\tІгаравіч");
    QVERIFY(m_catalog->save(spaced, &error));
    QCOMPARE(m_catalog->russianNameConflicts().size(), 0);

    spaced.name = QStringLiteral("Шульга Валерій Игоревич");
    QVERIFY(m_catalog->save(spaced, &error));
    const QVector<Catalog::NameConflict> conflicts = m_catalog->russianNameConflicts();
    QCOMPARE(conflicts.size(), 1);
    QCOMPARE(conflicts.at(0).nameBy, QStringLiteral("Шульга Валерый Ігаравіч"));
    QCOMPARE(conflicts.at(0).rows, 2);
    QCOMPARE(conflicts.at(0).names.size(), 2);
    QCOMPARE(conflicts.at(0).names.at(0).name, QStringLiteral("Шульга Валерий Игоревич"));
    QCOMPARE(conflicts.at(0).names.at(0).count, 1);
    QCOMPARE(conflicts.at(0).names.at(1).name, QStringLiteral("Шульга Валерій Игоревич"));
    QCOMPARE(conflicts.at(0).names.at(1).count, 1);
}

// копия, сохранённая после удаления своей записи (например, медленный перевод), не должна
// вернуть запись в память; откатанная пачка оставляет новые номера незаданными
void TestCatalog::savingDeletedRecordFails()
{
    QString error;
    Person stale = *byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    QVERIFY(m_catalog->remove({stale.id}, &error));
    const int count = m_catalog->people().size();

    stale.rank = QStringLiteral("палкоўнік");
    QVERIFY(!m_catalog->save(stale, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(m_catalog->people().size(), count);

    Person fresh = stale;
    fresh.id = 0;
    QVector<Person> batch = {fresh, stale};
    QVERIFY(!m_catalog->saveMany(batch, &error));
    QCOMPARE(batch.at(0).id, 0);
    QCOMPARE(m_catalog->people().size(), count);
}

void TestCatalog::failedOpenKeepsCurrentDatabase()
{
    const QString notSqlite = m_dir.filePath(QStringLiteral("not-a-database.sqlite"));
    QFile file(notSqlite);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not an SQLite file");
    file.close();
    const QString emptyDb = m_dir.filePath(QStringLiteral("no-tables.sqlite"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("empty"));
        db.setDatabaseName(emptyDb);
        QVERIFY(db.open());
        QSqlQuery(db).exec(QStringLiteral("CREATE TABLE Other (ID INTEGER)"));
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("empty"));

    const int count = m_catalog->people().size();
    QSignalSpy changed(m_catalog, &Catalog::dataChanged);
    for (const QString& path : {notSqlite, emptyDb, m_dir.filePath(QStringLiteral("missing.sqlite"))}) {
        QString error;
        QVERIFY(!m_catalog->open(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(m_catalog->isOpen());
        QCOMPARE(m_catalog->path(), m_path);
        QCOMPARE(m_catalog->people().size(), count);
    }
    QCOMPARE(changed.count(), 0);
    QVERIFY(!QFile::exists(m_dir.filePath(QStringLiteral("missing.sqlite"))));
}

// первое вхождение в каждом совпавшем поле заменяется, сохраняется и нормализуется
// как правка, и поиск дублей идёт уже по новому имени
void TestCatalog::replaceTextSavesLikeAnEdit()
{
    QString error;
    QCOMPARE(m_catalog->countMatches(TextField::NameBy, false, QStringLiteral("Шульга")), 1);
    QCOMPARE(m_catalog->countMatches(TextField::NameBy, true, QStringLiteral("Шульга")), 0);
    const int count = m_catalog->replaceText(TextField::NameBy, false, QStringLiteral("Шульга"),
                                             QStringLiteral("Шульгa"), &error); // латинская «a»
    QCOMPARE(count, 1);
    const Person* renamed = byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Ігаравіч"));
    QVERIFY(renamed); // латинская «a» при сохранении нормализована
    QCOMPARE(stored(m_path, renamed->id, QStringLiteral("Name_BY")), QStringLiteral("Шульга Валерый Ігаравіч"));
    QVERIFY(m_catalog->contains(*renamed));

    QCOMPARE(m_catalog->replaceText(TextField::NameBy, true, QStringLiteral("Шульга Валерый Ігаравіч"),
                                    QStringLiteral("Шульга Валерый Пятровіч"), &error), 1);
    QVERIFY(byNameBy(*m_catalog, QStringLiteral("Шульга Валерый Пятровіч")));
    QCOMPARE(m_catalog->replaceText(TextField::NameBy, false, QStringLiteral("няма такога"), QString(), &error), 0);
}

// название организации пишется в личные данные, имя остаётся пустым
void TestCatalog::organizationFlagIsStored()
{
    const QString titleBy = QStringLiteral("Адкрытае акцыянернае таварыства «Савушкін прадукт»");
    Person org;
    org.awardId = 14;
    org.personalBy = titleBy;
    org.ukaseNo = 245;
    org.ukaseDate = QDate(2026, 7, 22);
    org.language = kLangBelarusian;
    QString error;
    QVERIFY2(m_catalog->save(org, &error), qPrintable(error));
    QCOMPARE(stored(m_path, org.id, QStringLiteral("Name")), QString());
    QCOMPARE(stored(m_path, org.id, QStringLiteral("Name_BY")), QString());
    QCOMPARE(stored(m_path, org.id, QStringLiteral("Personal_Info_BY")), titleBy);

    Person other = org;
    other.id = 0;
    other.ukaseNo = 306;
    other.personalBy = QStringLiteral("Адкрытае акцыянернае таварыства «Планар»");
    QVERIFY2(m_catalog->save(other, &error), qPrintable(error));
    Person sameDecree = other;
    sameDecree.id = 0;
    sameDecree.personalBy = QStringLiteral("Адкрытае акцыянернае таварыства «ІНТЭГРАЛ»");
    QVERIFY2(m_catalog->save(sameDecree, &error), qPrintable(error));
    QCOMPARE(m_catalog->count(other), 1);
    QCOMPARE(m_catalog->count(sameDecree), 1);

    QVERIFY2(m_catalog->open(m_path, &error), qPrintable(error));
    const Person* saved = m_catalog->person(org.id);
    QVERIFY(saved);
    QVERIFY(saved->name.isEmpty());
    QVERIFY(saved->nameBy.isEmpty());
    QCOMPARE(saved->personalBy, titleBy);
}

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
