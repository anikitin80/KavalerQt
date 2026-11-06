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
    void emptyDictionaryValuesAreSkipped();
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
    QTest::newRow("by acc adjective + profession") << QStringLiteral("дзяжурнага памочніка начальніка турмы № 8") << LangBY << Accusative
                                                   << QStringLiteral("дзяжурны памочнік начальніка турмы № 8");
    QTest::newRow("by acc two adjectives") << QStringLiteral("вядучага навуковага супрацоўніка інстытута") << LangBY << Accusative
                                           << QStringLiteral("вядучы навуковы супрацоўнік інстытута");
    QTest::newRow("by acc adjective + hyphenated") << QStringLiteral("вядучага інжынера-канструктара бюро") << LangBY << Accusative
                                                   << QStringLiteral("вядучы інжынер-канструктар бюро");
    QTest::newRow("by acc adjective not in the dictionary") << QStringLiteral("вядучага ветэрынарнага ўрача станцыі") << LangBY << Accusative
                                                            << QStringLiteral("вядучы ветэрынарны ўрач станцыі");
    QTest::newRow("by dat two adjectives") << QStringLiteral("галоўнаму навуковаму супрацоўніку інстытута") << LangBY << Dative
                                           << QStringLiteral("галоўны навуковы супрацоўнік інстытута");
    QTest::newRow("by acc second profession after comma") << QStringLiteral("саліста оперы, вядучага майстра сцэны тэатра") << LangBY << Accusative
                                                          << QStringLiteral("саліст оперы, вядучы майстар сцэны тэатра");
    QTest::newRow("by acc comma, adjectives first") << QStringLiteral("саветніка Прэзідыума, галоўнага навуковага супрацоўніка інстытута") << LangBY << Accusative
                                                    << QStringLiteral("саветнік Прэзідыума, галоўны навуковы супрацоўнік інстытута");
    QTest::newRow("by acc comma inside quotes kept") << QStringLiteral("слесара таварыства «Мазырсоль, слесара»") << LangBY << Accusative
                                                     << QStringLiteral("слесар таварыства «Мазырсоль, слесара»");
    QTest::newRow("by acc comma before a compound adjective") << QStringLiteral("начальніка ўчастка механічнай, хіміка-тэрмічнай апрацоўкі") << LangBY << Accusative
                                                              << QStringLiteral("начальнік ўчастка механічнай, хіміка-тэрмічнай апрацоўкі");
    QTest::newRow("by acc compound in brackets") << QStringLiteral("загадчыка (урача-тэрапеўта) бальніцы") << LangBY << Accusative
                                                 << QStringLiteral("загадчык (урач-тэрапеўт) бальніцы");
    QTest::newRow("by acc bracket, genitive kept") << QStringLiteral("урача-тэрапеўта (загадчыка паліклінікі) бальніцы") << LangBY << Accusative
                                                   << QStringLiteral("урач-тэрапеўт (загадчык паліклінікі) бальніцы");
    QTest::newRow("by acc bracket, adjective first") << QStringLiteral("намесніка начальніка (галоўнага ўрача) бальніцы") << LangBY << Accusative
                                                     << QStringLiteral("намеснік начальніка (галоўны ўрач) бальніцы");
    QTest::newRow("by acc three-part word") << QStringLiteral("урача-анестэзіёлага-рэаніматолага бальніцы") << LangBY << Accusative
                                            << QStringLiteral("урач-анестэзіёлаг-рэаніматолаг бальніцы");
    QTest::newRow("by dat bracket after two words") << QStringLiteral("артысту балета (салісту) тэатра") << LangBY << Dative
                                                    << QStringLiteral("артыст балета (саліст) тэатра");
    QTest::newRow("by acc былога") << QStringLiteral("былога генеральнага дырэктара завода") << LangBY << Accusative
                                   << QStringLiteral("былы генеральны дырэктар завода");
    QTest::newRow("ru acc acting head") << QStringLiteral("исполняющего обязанности начальника отдела") << LangRU << Accusative
                                        << QStringLiteral("исполняющий обязанности начальника отдела");
    QTest::newRow("by acc same adjective twice") << QStringLiteral("дзяжурнага дзяжурнага аддзялення") << LangBY << Accusative
                                                 << QStringLiteral("дзяжурны дзяжурнага аддзялення");
    QTest::newRow("by acc Latin i in the dictionary") << QStringLiteral("прыёмную маці аддзела адукацыі") << LangBY << Accusative
                                                      << QStringLiteral("прыёмная маці аддзела адукацыі");
    QTest::newRow("by acc profession in brackets") << QStringLiteral("артыста-вакаліста (саліста) філармоніі") << LangBY << Accusative
                                                   << QStringLiteral("артыст-вакаліст (саліст) філармоніі");
    QTest::newRow("by dat profession in brackets") << QStringLiteral("артысту-вакалісту (салісту) філармоніі") << LangBY << Dative
                                                   << QStringLiteral("артыст-вакаліст (саліст) філармоніі");
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

// a broken line (empty nominative or form) is skipped, not used as a replacement
void TestProfessionConverter::emptyDictionaryValuesAreSkipped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("PositionsRU.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(";водителю;водителя\nслесарь;;\nмеханик;механику;механика\n");
    file.close();

    CProfessionConverter converter;
    QVERIFY(converter.SetDictionaryPath(path.toStdWString(), LangRU));
    std::wstring text = L"водителя автобуса";
    converter.ConvertProfession(text, LangRU, Accusative);
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("водителя автобуса"));
    std::wstring mechanic = L"механика цеха";
    QVERIFY(converter.ConvertProfession(mechanic, LangRU, Accusative));
    QCOMPARE(QString::fromStdWString(mechanic), QStringLiteral("механик цеха"));
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
