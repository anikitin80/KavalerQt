// CProfessionConverter: the position before the name ("водителю автобуса",
// "старшего инспектора отдела") back to the nominative case using the
// profession dictionaries (tests use copies in tests/data).

#include "ProfessionConverter.h"
#include "TestData.h"

#include <QTemporaryDir>
#include <QtTest>

Q_DECLARE_METATYPE(LangType)
Q_DECLARE_METATYPE(NounCase)

class TestProfessionConverter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void convert_data();
    void convert();
    void onlyFirstFiveWordsAreSearched();
    void unknownProfessionBecomesNewWord();
    void newWordsAreLowerCase();
    void newWordsWorkWithoutDictionaryFile();
    void dictionaryRoundTrip();
};

void TestProfessionConverter::initTestCase()
{
    TestData::isolateSettings();
}

void TestProfessionConverter::convert_data()
{
    QTest::addColumn<QString>("decree");
    QTest::addColumn<LangType>("lang");
    QTest::addColumn<NounCase>("nounCase");
    QTest::addColumn<QString>("expected");

    QTest::newRow("ru dat") << QStringLiteral("водителю автобуса") << LangRU << Dative
                            << QStringLiteral("водитель автобуса");
    QTest::newRow("ru dat capital kept") << QStringLiteral("Слесарю цеха № 5") << LangRU << Dative
                                         << QStringLiteral("Слесарь цеха № 5");
    QTest::newRow("ru acc with adjective") << QStringLiteral("старшего инспектора отдела") << LangRU << Accusative
                                           << QStringLiteral("старший инспектор отдела");
    QTest::newRow("ru acc") << QStringLiteral("механика колхоза «Рассвет»") << LangRU << Accusative
                            << QStringLiteral("механик колхоза «Рассвет»");
    QTest::newRow("ru hyphen, second part new") << QStringLiteral("водителю-экспедитору филиала") << LangRU << Dative
                                                << QStringLiteral("водитель-экспедитор филиала");
    QTest::newRow("by dat") << QStringLiteral("вадзіцелю аўтобуса") << LangBY << Dative
                            << QStringLiteral("вадзіцель аўтобуса");
    QTest::newRow("by acc") << QStringLiteral("слесара цэха") << LangBY << Accusative
                            << QStringLiteral("слесар цэха");
    QTest::newRow("by dat non-military rank") << QStringLiteral("загадчыку кафедры, прафесару") << LangBY << Dative
                                              << QStringLiteral("загадчык кафедры, прафесар");
}

void TestProfessionConverter::convert()
{
    QFETCH(QString, decree);
    QFETCH(LangType, lang);
    QFETCH(NounCase, nounCase);
    QFETCH(QString, expected);

    CProfessionConverter converter;
    TestData::useTestDictionaries(converter);
    std::wstring text = decree.toStdWString();
    QVERIFY(converter.ConvertProfession(text, lang, nounCase));
    QCOMPARE(QString::fromStdWString(text), expected);
}

void TestProfessionConverter::onlyFirstFiveWordsAreSearched()
{
    CProfessionConverter converter;
    TestData::useTestDictionaries(converter);
    std::wstring text = L"жительнице г. Минска по месту жительства водителю";
    QVERIFY(!converter.ConvertProfession(text, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("жительнице г. Минска по месту жительства водителю"));
}

// an unknown position is left as is and offered for the dictionary
void TestProfessionConverter::unknownProfessionBecomesNewWord()
{
    CProfessionConverter converter;
    TestData::useTestDictionaries(converter);
    std::wstring text = L"сварщику цеха";
    QVERIFY(!converter.ConvertProfession(text, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("сварщику цеха"));

    const auto& words = converter.newWords();
    QCOMPARE(int(words.size()), 1);
    QCOMPARE(QString::fromStdWString(words.front().Original), QStringLiteral("сварщику"));
    QCOMPARE(QString::fromStdWString(words.front().Word), QStringLiteral("сварщик"));
    QCOMPARE(words.front().Language, LangRU);
    QCOMPARE(words.front().Case, Dative);
}

// dictionary keys are looked up lower-case, so a capitalised first word must
// be stored lower-case to be found next time
void TestProfessionConverter::newWordsAreLowerCase()
{
    CProfessionConverter converter; // no files: UpdateDictionary only fills memory
    std::wstring text = L"Сварщику цеха";
    converter.ConvertProfession(text, LangRU, Dative);
    QCOMPARE(int(converter.newWords().size()), 1);
    QCOMPARE(QString::fromStdWString(converter.newWords().front().Original), QStringLiteral("сварщику"));

    std::vector<NewDictItem> items = converter.newWords();
    converter.UpdateDictionary(items);
    std::wstring again = L"Сварщику цеха № 2";
    QVERIFY(converter.ConvertProfession(again, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(again), QStringLiteral("Сварщик цеха № 2"));
}

void TestProfessionConverter::newWordsWorkWithoutDictionaryFile()
{
    CProfessionConverter converter; // no dictionary paths at all
    std::wstring text = L"сварщику цеха";
    converter.ConvertProfession(text, LangRU, Dative);
    std::vector<NewDictItem> items = converter.newWords();
    QCOMPARE(int(items.size()), 1);

    QVERIFY(!converter.UpdateDictionary(items)); // nowhere to save
    std::wstring again = L"сварщику участка";
    QVERIFY(converter.ConvertProfession(again, LangRU, Dative)); // but usable in this session
    QCOMPARE(QString::fromStdWString(again), QStringLiteral("сварщик участка"));
}

void TestProfessionConverter::dictionaryRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const std::wstring file = dir.filePath(QStringLiteral("PositionsRU.csv")).toStdWString();

    CProfessionConverter writer;
    QVERIFY(writer.SetDictionaryPath(file, LangRU)); // a new file is fine
    std::wstring text = L"сварщику цеха";
    writer.ConvertProfession(text, LangRU, Dative);
    std::vector<NewDictItem> items = writer.newWords();
    QVERIFY(writer.UpdateDictionary(items));

    CProfessionConverter reader;
    QVERIFY(reader.SetDictionaryPath(file, LangRU));
    std::wstring again = L"сварщику участка";
    QVERIFY(reader.ConvertProfession(again, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(again), QStringLiteral("сварщик участка"));
}

QTEST_GUILESS_MAIN(TestProfessionConverter)
#include "tst_professionconverter.moc"
