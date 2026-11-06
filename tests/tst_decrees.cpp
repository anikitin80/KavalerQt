// End-to-end parser tests on real decrees.
//
// tests/data/decrees/<id>.txt          decree paragraphs as readDocxParagraphs returns them
// tests/data/decrees/<id>.expected.tsv persons as they are stored in the catalog database
//
// A non-empty "known_issue" column marks a person the parser currently gets
// wrong; the test then expects the mismatch (QEXPECT_FAIL) and starts to fail
// (XPASS) once the parser is fixed, so the marker can be removed.

#include "TestData.h"

#include <QDate>
#include <QDir>
#include <QHash>
#include <QtTest>

namespace {

struct ExpectedPerson
{
    QString name;
    int awardId = 0;
    QString description;
    QString rank;
    bool posthumous = false;
    QString knownIssue;
};

struct Expected
{
    int ukazNumber = 0;
    QDate ukazDate;
    bool russian = false;
    QVector<ExpectedPerson> persons;
};

Expected readExpected(const QString& id)
{
    Expected expected;
    for (const QString& line : TestData::readLines(TestData::path(QStringLiteral("decrees/%1.expected.tsv").arg(id)))) {
        const QStringList cells = line.split(QLatin1Char('\t'));
        if (line.startsWith(QLatin1String("# ukaz")) && cells.size() >= 4) {
            expected.ukazNumber = cells.at(1).toInt();
            expected.ukazDate = QDate::fromString(cells.at(2), Qt::ISODate);
            expected.russian = cells.at(3) == QLatin1String("ru");
            continue;
        }
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || cells.size() < 5)
            continue;
        ExpectedPerson person;
        person.name = cells.at(0);
        person.awardId = cells.at(1).toInt();
        person.description = cells.at(2);
        person.rank = cells.at(3);
        person.posthumous = cells.at(4) == QLatin1String("1");
        person.knownIssue = cells.value(5);
        expected.persons.push_back(person);
    }
    return expected;
}

QString describe(const QString& name, int awardId, const QString& description, const QString& rank, bool posthumous)
{
    return QStringLiteral("%1 | award %2 | %3 | rank: %4 | posthumous: %5")
        .arg(name).arg(awardId).arg(description, rank, posthumous ? QStringLiteral("yes") : QStringLiteral("no"));
}

} // namespace

class TestDecrees : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void decree_data();
    void decree();

    void person_data();
    void person();

private:
    struct Parsed
    {
        std::vector<PersonInfo> persons;
        QStringList descriptions; // GetPersonDescription(person, false)
    };
    const Parsed& parsed(const QString& id);

    QStringList m_ids;
    std::unique_ptr<CDocumentParser> m_parser;
    QHash<QString, Parsed> m_parsed;
};

void TestDecrees::initTestCase()
{
    TestData::isolateSettings();
    m_parser = TestData::makeParser();
    const QDir dir(TestData::path(QStringLiteral("decrees")));
    for (const QString& file : dir.entryList({QStringLiteral("*.expected.tsv")}, QDir::Files, QDir::Name))
        m_ids.push_back(file.left(file.size() - int(qstrlen(".expected.tsv"))));
    QVERIFY2(!m_ids.isEmpty(), "no decree fixtures found");
}

const TestDecrees::Parsed& TestDecrees::parsed(const QString& id)
{
    auto it = m_parsed.find(id);
    if (it != m_parsed.end())
        return *it;

    std::vector<std::wstring> lines = TestData::toWide(
        TestData::readLines(TestData::path(QStringLiteral("decrees/%1.txt").arg(id))));
    m_parser->ResetData();
    m_parser->ProcessFile((id + QStringLiteral(".docx")).toStdWString(), lines);

    Parsed result;
    result.persons = m_parser->m_Persons;
    for (PersonInfo& person : m_parser->m_Persons)
        result.descriptions.push_back(QString::fromStdWString(m_parser->GetPersonDescription(person, false)));
    return *m_parsed.insert(id, result);
}

void TestDecrees::decree_data()
{
    QTest::addColumn<QString>("id");
    for (const QString& id : m_ids)
        QTest::newRow(qPrintable(id)) << id;
}

// decree number, date, language and the number of persons
void TestDecrees::decree()
{
    QFETCH(QString, id);
    const Expected expected = readExpected(id);
    const Parsed& result = parsed(id);

    QCOMPARE(int(result.persons.size()), expected.persons.size());
    QVERIFY(!result.persons.empty());
    for (const PersonInfo& person : result.persons) {
        QCOMPARE(person.Ukaz.Number, expected.ukazNumber);
        QCOMPARE(person.Ukaz.Date, expected.ukazDate);
        QCOMPARE(person.Ukaz.RussianLang, expected.russian);
    }
}

void TestDecrees::person_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<QString>("knownIssue");
    for (const QString& id : m_ids) {
        const Expected expected = readExpected(id);
        for (int index = 0; index < expected.persons.size(); ++index) {
            const ExpectedPerson& person = expected.persons.at(index);
            // ASCII tag: the console may not show Cyrillic, the name is in the compared text
            QTest::addRow("%s #%d", qPrintable(id), index + 1)
                << id << index
                << describe(person.name, person.awardId, person.description, person.rank, person.posthumous)
                << person.knownIssue;
        }
    }
}

// name in nominative case, award, description and rank as in the catalog
void TestDecrees::person()
{
    QFETCH(QString, id);
    QFETCH(int, index);
    QFETCH(QString, expected);
    QFETCH(QString, knownIssue);

    const Parsed& result = parsed(id);
    if (size_t(index) >= result.persons.size())
        QSKIP("person count differs, see the decree() test");

    const PersonInfo& person = result.persons[size_t(index)];
    const QString actual = describe(QString::fromStdWString(person.Name), person.AwardId,
        result.descriptions.at(index), QString::fromStdWString(person.MilitaryRank), person.Posthumous);

    if (!knownIssue.isEmpty())
        QEXPECT_FAIL("", qPrintable(knownIssue), Abort);
    QCOMPARE(actual, expected);
}

QTEST_GUILESS_MAIN(TestDecrees)
#include "tst_decrees.moc"
