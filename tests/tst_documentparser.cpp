// CDocumentParser на коротких указах, написанных вручную: распознавание наград, звания,
// посмертные награды, форматы списков, несколько указов и сведения о группе.

#include "TestData.h"

#include <QDate>
#include <QtTest>

Q_DECLARE_METATYPE(LangType)
Q_DECLARE_METATYPE(NounCase)

namespace {

// открывает защищённые шаги парсера
class Parser : public CDocumentParser
{
public:
    using CDocumentParser::ExtractMilitaryRank;
    using CDocumentParser::GetAwardId;
};

QString text(const std::wstring& value)
{
    return QString::fromStdWString(value);
}

const QString kUkazRu = QStringLiteral("УКАЗ ПРЕЗИДЕНТА РЕСПУБЛИКИ БЕЛАРУСЬ");
const QString kUkazBy = QStringLiteral("УКАЗ ПРЭЗІДЭНТА РЭСПУБЛІКІ БЕЛАРУСЬ");
const QString kPresident = QStringLiteral("Президент Республики Беларусь");

} // namespace

class TestDocumentParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void awardId_data();
    void awardId();
    void longestAwardMatch();
    void awardPhraseCoversTheDecreeText();
    void personAfterAward();
    void militaryRank_data();
    void militaryRank();
    void posthumousInDescription_data();
    void posthumousInDescription();

    void decreeHeader();
    void singleAwardPersonBeforeAward();
    void singleAwardPosthumous();
    void singleAwardPosthumousAfterAward();
    void comradeTitleBeforeName();
    void splitUsesTheSameNameCut();
    void listWithLineBreaks();
    void housewifeGetsTheCityOfTheSection();
    void listEndingWithDashDoesNotCrash();
    void listStartingWithDashDoesNotCrash();
    void belarusianDecree();
    void dashInsideQuotesIsNotSecondProfession();
    void twoDecreesInOneFile();
    void groupInfoBelongsToItsDecree();
    void groupInfoIsTheOrganisationOnly();
    void groupKeepsInitialsAndPrecedesTheDegree();
    void organizationIsAwardedItself();
    void organizationInAwardList();
    void workplaceIsNotAnOrganization();
    void resetClearsEverything();

private:
    void parse(const QStringList& lines, const QString& fileName = QStringLiteral("test.docx"));
    std::unique_ptr<Parser> m_parser;
};

void TestDocumentParser::initTestCase()
{
    TestData::isolateSettings();
}

void TestDocumentParser::init()
{
    m_parser = TestData::makeParser<Parser>();
}

void TestDocumentParser::parse(const QStringList& lines, const QString& fileName)
{
    std::vector<std::wstring> wide = TestData::toWide(lines);
    m_parser->ProcessFile(fileName.toStdWString(), wide);
}

void TestDocumentParser::awardId_data()
{
    QTest::addColumn<QString>("award");
    QTest::addColumn<bool>("russian");
    QTest::addColumn<int>("expected");

    QTest::newRow("medal in quotes") << QStringLiteral("медалью «За отвагу»") << true << 14;
    QTest::newRow("medal plain quotes") << QStringLiteral("медалью \"За отвагу\"") << true << 14;
    QTest::newRow("medal without quotes") << QStringLiteral("медалью Франциска Скорины") << true << 19;
    QTest::newRow("order") << QStringLiteral("орденом Франциска Скорины") << true << 11;
    QTest::newRow("order degree I") << QStringLiteral("орденом Отечества I степени") << true << 2;
    QTest::newRow("order degree II") << QStringLiteral("орденом Отечества II степени") << true << 3;
    QTest::newRow("order degree III cyrillic") << QStringLiteral("орденом Отечества ІІІ степени") << true << 4;
    QTest::newRow("order by") << QStringLiteral("ордэнам Айчыны III ступені") << false << 4;
    QTest::newRow("order by mothers") << QStringLiteral("ордэнам Маці") << false << 13;
    QTest::newRow("medal by") << QStringLiteral("медалём «За адвагу»") << false << 14;
    QTest::newRow("honorary title") << QStringLiteral("почетное звание «Народный артист Беларуси»") << true << 29;
    QTest::newRow("honorary title long name") << QStringLiteral("почетное звание «Заслуженный врач Республики Беларусь»")
                                              << true << 43;
    QTest::newRow("unknown medal") << QStringLiteral("медалью «Неизвестная»") << true << 0;
    QTest::newRow("not an award") << QStringLiteral("грамотой Совета Министров") << true << 0;
}

void TestDocumentParser::awardId()
{
    QFETCH(QString, award);
    QFETCH(bool, russian);
    QFETCH(int, expected);
    QCOMPARE(m_parser->GetAwardId(award.toStdWString(), russian), expected);
}

// более короткое название каталога не должно победить, если оно начало более длинного
void TestDocumentParser::longestAwardMatch()
{
    Parser parser;
    std::map<long, DecreeAward> table;
    DecreeAward shortMedal;
    shortMedal.Type = DecreeAward::Medal;
    shortMedal.Name = L"Медаль \"За отличие\"";
    DecreeAward longMedal;
    longMedal.Type = DecreeAward::Medal;
    longMedal.Name = L"Медаль \"За отличие в службе\"";
    table.emplace(1, shortMedal);
    table.emplace(2, longMedal);
    parser.Initialize(table);

    const std::wstring longer = L"медалью «За отличие в службе» Иванова";
    size_t length = 0;
    QCOMPARE(parser.GetAwardId(longer, true, false, &length), 2);
    QCOMPARE(text(longer.substr(0, length)), QStringLiteral("медалью «За отличие в службе»"));

    const std::wstring shorter = L"медалью «За отличие» Иванова";
    length = 0;
    QCOMPARE(parser.GetAwardId(shorter, true, false, &length), 1);
    QCOMPARE(text(shorter.substr(0, length)), QStringLiteral("медалью «За отличие»"));
}

void TestDocumentParser::awardPhraseCoversTheDecreeText()
{
    const auto phrase = [this](const wchar_t* line, bool russian) {
        size_t length = 0;
        const int id = m_parser->GetAwardId(line, russian, false, &length);
        return qMakePair(id, text(std::wstring(line).substr(0, length)));
    };

    QCOMPARE(phrase(L"медалью «За отвагу» Иванова", true),
             qMakePair(14, QStringLiteral("медалью «За отвагу»")));
    QCOMPARE(phrase(L"медалью Франциска Скорины Иванова", true),
             qMakePair(19, QStringLiteral("медалью Франциска Скорины")));
    QCOMPARE(phrase(L"орденом Отечества ІІІ степени Иванова", true),
             qMakePair(4, QStringLiteral("орденом Отечества ІІІ степени")));
    QCOMPARE(phrase(L"почетное звание «Заслуженный врач Республики Беларусь» Иванову", true),
             qMakePair(43, QStringLiteral("почетное звание «Заслуженный врач Республики Беларусь»")));
    QCOMPARE(phrase(L"почетное звание «Народный артист\u00A0Беларуси» Иванову", true),
             qMakePair(29, QStringLiteral("почетное звание «Народный артист\u00A0Беларуси»")));
    QCOMPARE(phrase(L"ганаровае званне «Заслужаны ўрач Рэспублікі Беларусь» Іванову", false),
             qMakePair(43, QStringLiteral("ганаровае званне «Заслужаны ўрач Рэспублікі Беларусь»")));

    size_t length = 99;
    QCOMPARE(m_parser->GetAwardId(L"медалью «Неизвестная»", true, false, &length), 0);
    QCOMPARE(length, size_t(0));
}

// человек идёт после награды, поэтому отрез — фраза в указе
void TestDocumentParser::personAfterAward()
{
    parse({kUkazRu, QStringLiteral("15 мая 2001 г. № 260"),
           QStringLiteral("За заслуги наградить медалью Франциска Скорины слесаря цеха Иванова Ивана Ивановича."),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& skorina = m_parser->m_Persons.front();
    QCOMPARE(skorina.AwardId, 19);
    QCOMPARE(text(skorina.Name), QStringLiteral("Иванов Иван Иванович"));
    QCOMPARE(text(m_parser->GetPersonDescription(skorina, false)), QStringLiteral("слесарь цеха"));

    m_parser->ResetData();
    parse({kUkazRu, QStringLiteral("15 мая 2001 г. № 261"),
           QStringLiteral("За заслуги наградить медалью «За отвагу» слесаря цеха Иванова Ивана Ивановича."),
           kPresident});
    PersonInfo& medal = m_parser->m_Persons.front();
    QCOMPARE(medal.AwardId, 14);
    QCOMPARE(text(medal.Name), QStringLiteral("Иванов Иван Иванович"));
    QCOMPARE(text(m_parser->GetPersonDescription(medal, false)), QStringLiteral("слесарь цеха"));

    m_parser->ResetData();
    parse({kUkazRu, QStringLiteral("15 мая 2001 г. № 262"),
           QStringLiteral("За заслуги присвоить почетное звание «Заслуженный врач Республики Беларусь» "
                          "слесарю цеха Иванову Ивану Ивановичу."),
           kPresident});
    PersonInfo& title = m_parser->m_Persons.front();
    QCOMPARE(title.AwardId, 43);
    QCOMPARE(text(title.Name), QStringLiteral("Иванов Иван Иванович"));
    QCOMPARE(text(m_parser->GetPersonDescription(title, false)), QStringLiteral("слесарь цеха"));

    m_parser->ResetData();
    parse({kUkazRu, QStringLiteral("15 мая 2001 г. № 263"),
           QStringLiteral("За заслуги наградить орденом Отечества ІІІ степени Иванова Ивана Ивановича."),
           kPresident});
    PersonInfo& order = m_parser->m_Persons.front();
    QCOMPARE(order.AwardId, 4);
    QCOMPARE(text(order.Name), QStringLiteral("Иванов Иван Иванович"));
    QCOMPARE(text(m_parser->GetPersonDescription(order, false)), QString());

    m_parser->ResetData();
    parse({kUkazBy, QStringLiteral("15 мая 2001 г. № 264"),
           QStringLiteral("За мужнасць узнагародзіць медалём «За адвагу» слесара цэха Лагодзіча Юрыя Іванавіча."),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    PersonInfo& belarusian = m_parser->m_Persons.front();
    QCOMPARE(belarusian.AwardId, 14);
    QCOMPARE(text(belarusian.Name), QStringLiteral("Лагодзіч Юрый Іванавіч"));
    QCOMPARE(text(m_parser->GetPersonDescription(belarusian, false)), QStringLiteral("слесар цэха"));
}

void TestDocumentParser::militaryRank_data()
{
    QTest::addColumn<QString>("description");
    QTest::addColumn<LangType>("lang");
    QTest::addColumn<NounCase>("nounCase");
    QTest::addColumn<QString>("rank");
    QTest::addColumn<QString>("rest");

    QTest::newRow("after comma") << QStringLiteral("командира роты, капитана милиции") << LangRU << Accusative
                                 << QStringLiteral("капитан милиции") << QStringLiteral("командира роты");
    QTest::newRow("without comma") << QStringLiteral("старшего оперуполномоченного майора милиции") << LangRU << Accusative
                                   << QStringLiteral("майор милиции") << QStringLiteral("старшего оперуполномоченного");
    // длинные звания содержат короткие: победить должно самое длинное
    QTest::newRow("старший прапорщик") << QStringLiteral("инспектору старшему прапорщику милиции") << LangRU << Dative
                                       << QStringLiteral("старший прапорщик милиции") << QStringLiteral("инспектору");
    QTest::newRow("генерал-майор") << QStringLiteral("начальнику управления генерал-майору") << LangRU << Dative
                                   << QStringLiteral("генерал-майор") << QStringLiteral("начальнику управления");
    QTest::newRow("генерал-лейтенант acc") << QStringLiteral("начальника управления генерал-лейтенанта") << LangRU << Accusative
                                           << QStringLiteral("генерал-лейтенант") << QStringLiteral("начальника управления");
    QTest::newRow("подполковник") << QStringLiteral("заместителя начальника подполковника милиции") << LangRU << Accusative
                                  << QStringLiteral("подполковник милиции") << QStringLiteral("заместителя начальника");
    QTest::newRow("by after comma") << QStringLiteral("намесніка начальніка, старшага прапаршчыка") << LangBY << Accusative
                                    << QStringLiteral("старшы прапаршчык") << QStringLiteral("намесніка начальніка");
    QTest::newRow("by генерал-палкоўнік") << QStringLiteral("міністру генерал-палкоўніку") << LangBY << Dative
                                          << QStringLiteral("генерал-палкоўнік") << QStringLiteral("міністру");
    QTest::newRow("by дзяржаўны саветнік") << QStringLiteral("пракурору, дзяржаўнаму саветніку юстыцыі 2 класа")
                                           << LangBY << Dative << QStringLiteral("дзяржаўны саветнік юстыцыі 2 класа")
                                           << QStringLiteral("пракурору");
    QTest::newRow("by latin i") << QStringLiteral("начальніка, капiтана мiлiцыi") << LangBY << Accusative
                                << QStringLiteral("капітан міліцыі") << QStringLiteral("начальніка");
    QTest::newRow("no rank") << QStringLiteral("водителя автобуса") << LangRU << Accusative
                             << QString() << QStringLiteral("водителя автобуса");
    QTest::newRow("by старшы мічман") << QStringLiteral("камандзіра судна, старшага мічмана") << LangBY << Accusative
                                      << QStringLiteral("старшы мічман") << QStringLiteral("камандзіра судна");
    QTest::newRow("ru генерал-полковник") << QStringLiteral("министра обороны, генерал-полковника") << LangRU << Accusative
                                          << QStringLiteral("генерал-полковник") << QStringLiteral("министра обороны");
    QTest::newRow("by rank in retirement") << QStringLiteral("прафесара кафедры, палкоўніка ўнутранай службы ў адстаўцы")
                                           << LangBY << Accusative << QStringLiteral("палкоўнік ўнутранай службы ў адстаўцы")
                                           << QStringLiteral("прафесара кафедры");
    // должности, не звания
    QTest::newRow("by саветнік is a position") << QStringLiteral("саветніка ўпраўлення кадравай работы")
                                               << LangBY << Accusative << QString() << QStringLiteral("саветніка ўпраўлення кадравай работы");
    QTest::newRow("by капітан-механік is a position") << QStringLiteral("капітана-механіка прадпрыемства водных шляхоў")
                                                      << LangBY << Accusative << QString() << QStringLiteral("капітана-механіка прадпрыемства водных шляхоў");
    QTest::newRow("by капітан каманды is a position") << QStringLiteral("капітана нацыянальнай каманды Рэспублікі Беларусь па тэнісу")
                                                      << LangBY << Accusative << QString()
                                                      << QStringLiteral("капітана нацыянальнай каманды Рэспублікі Беларусь па тэнісу");
}

void TestDocumentParser::militaryRank()
{
    QFETCH(QString, description);
    QFETCH(LangType, lang);
    QFETCH(NounCase, nounCase);
    QFETCH(QString, rank);
    QFETCH(QString, rest);

    PersonInfo person;
    person.Description = description.toStdWString();
    QCOMPARE(m_parser->ExtractMilitaryRank(person, lang, nounCase), !rank.isEmpty());
    QCOMPARE(text(person.MilitaryRank), rank);
    QCOMPARE(text(person.Description), rest);
}

void TestDocumentParser::posthumousInDescription_data()
{
    QTest::addColumn<QString>("description");
    QTest::addColumn<LangType>("lang");
    QTest::addColumn<bool>("posthumous");
    QTest::newRow("ru") << QStringLiteral("водителю (посмертно)") << LangRU << true;
    QTest::newRow("by") << QStringLiteral("вадзіцелю (пасмяротна)") << LangBY << true;
    QTest::newRow("ru marker in by decree") << QStringLiteral("вадзіцелю (посмертно)") << LangBY << false;
    QTest::newRow("none") << QStringLiteral("водителю") << LangRU << false;
}

void TestDocumentParser::posthumousInDescription()
{
    QFETCH(QString, description);
    QFETCH(LangType, lang);
    QFETCH(bool, posthumous);

    PersonInfo person;
    person.Description = description.toStdWString();
    m_parser->ExtractMilitaryRank(person, lang, Dative);
    QCOMPARE(person.Posthumous, posthumous);
    QVERIFY(!text(person.Description).contains(QLatin1Char('(')) || !posthumous);
    if (posthumous)
        QVERIFY(!text(person.Description).endsWith(QLatin1Char(' ')));
}

void TestDocumentParser::decreeHeader()
{
    parse({kUkazRu, QStringLiteral("26 апреля 1994 г. № 2995-XII"),
           QStringLiteral("За заслуги наградить скульптора Азгура Заира Исааковича медалью Франциска Скорины."),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    const UkazInfo& ukaz = m_parser->m_Persons.front().Ukaz;
    QCOMPARE(ukaz.Number, 2995);
    QCOMPARE(ukaz.Date, QDate(1994, 4, 26));
    QVERIFY(ukaz.RussianLang);
    QCOMPARE(m_parser->GetFileMonth(L"test.docx"), 4);
}

void TestDocumentParser::singleAwardPersonBeforeAward()
{
    parse({kUkazRu, QStringLiteral("28 декабря 1992 г. № 2078"),
           QStringLiteral("За достижения в деле национального возрождения и развитии белорусского искусства "
                          "наградить скульптора Азгура Заира Исааковича медалью Франциска Скорины."),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& person = m_parser->m_Persons.front();
    QCOMPARE(person.AwardId, 19);
    QCOMPARE(text(person.Name), QStringLiteral("Азгур Заир Исаакович"));
    QCOMPARE(text(m_parser->GetPersonDescription(person, false)), QStringLiteral("скульптор"));
    QVERIFY(!person.Posthumous);
}

// «тов.» перед именем, в том числе с неразрывным пробелом после точки
void TestDocumentParser::comradeTitleBeforeName()
{
    parse({kUkazRu, QStringLiteral("10 октября 1991 г. № 1154-XII"),
           QStringLiteral("За заслуги присвоить директору Республиканского радиотелецентра тов.\u00A0"
                          "Чубату Георгию Дмитриевичу почетное звание «Заслуженный связист Республики Беларусь»."),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& awarded = m_parser->m_Persons.front();
    QCOMPARE(text(awarded.Name), QStringLiteral("Чубат Георгий Дмитриевич"));
    QCOMPARE(text(m_parser->GetPersonDescription(awarded, false)),
             QStringLiteral("директор Республиканского радиотелецентра"));

    m_parser->ResetData();
    parse({kUkazRu, QStringLiteral("28 декабря 1992 г. № 2078"),
           QStringLiteral("За достижения наградить скульптора тов. Азгура Заира Исааковича медалью Франциска Скорины."),
           kPresident});
    PersonInfo& medal = m_parser->m_Persons.front();
    QCOMPARE(text(medal.Name), QStringLiteral("Азгур Заир Исаакович"));
    QCOMPARE(text(m_parser->GetPersonDescription(medal, false)), QStringLiteral("скульптор"));

    m_parser->ResetData();
    parse({kUkazRu, QStringLiteral("9 апреля 1999 г.№ 206"),
           QStringLiteral("За мужество наградить:"),
           QStringLiteral("медалью «За отвагу»"),
           QStringLiteral("тов. Азгура\vЗаира Исааковича"), QStringLiteral("–"),
           QStringLiteral("скульптора"),
           kPresident});
    QCOMPARE(text(m_parser->m_Persons.front().Name), QStringLiteral("Азгур Заир Исаакович"));
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons.front(), false)), QStringLiteral("скульптор"));
}

// тот же отрез, что у самого указа: отчество забирает фамилию,
// заглавное слово, которое не отчество, остаётся в должности
void TestDocumentParser::splitUsesTheSameNameCut()
{
    PersonInfo shared;
    shared.AwardId = 14;
    shared.Ukaz.RussianLang = true;
    shared.Name = L"Петров Петр Петрович";
    shared.Description = L"слесаря Цех Иванова Ивана и токаря Петрова Петра Петровича";
    QVERIFY(m_parser->SpltPersonInfo(shared));
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    const PersonInfo& cut = m_parser->m_Persons.front();
    QCOMPARE(text(cut.Name), QStringLiteral("Иванов Иван"));
    QCOMPARE(text(cut.Description), QStringLiteral("слесарь Цех"));
    QCOMPARE(text(shared.Name), QStringLiteral("Петров Петр Петрович"));
    QCOMPARE(text(shared.Description), QStringLiteral("токарь Петрова Петра Петровича"));

    m_parser->ResetData();
    PersonInfo withPatronymic;
    withPatronymic.AwardId = 14;
    withPatronymic.Ukaz.RussianLang = true;
    withPatronymic.Description = L"слесаря цеха тов. Иванова Ивана Ивановича и токаря Петрова Петра Петровича";
    QVERIFY(m_parser->SpltPersonInfo(withPatronymic));
    const PersonInfo& full = m_parser->m_Persons.front();
    QCOMPARE(text(full.Name), QStringLiteral("Иванов Иван Иванович"));
    QCOMPARE(text(full.Description), QStringLiteral("слесарь цеха"));
}

void TestDocumentParser::singleAwardPosthumous()
{
    parse({kUkazRu, QStringLiteral("22 октября 1996 г. № 434"),
           QStringLiteral("За мужество наградить водителя Иванова Ивана Ивановича (посмертно) медалью «За отвагу»."),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& person = m_parser->m_Persons.front();
    QCOMPARE(person.AwardId, 14);
    QCOMPARE(text(person.Name), QStringLiteral("Иванов Иван Иванович"));
    QCOMPARE(text(m_parser->GetPersonDescription(person, false)), QStringLiteral("водитель"));
    QVERIFY(person.Posthumous);
}

// человек и звание до награды, «(посмертно)» после названия награды
// (Кашкан, 28.09.2001 № 553; Гурин, 21.01.2005 № 50)
void TestDocumentParser::singleAwardPosthumousAfterAward()
{
    parse({kUkazRu, QStringLiteral("28 сентября 2001 г. № 553"),
           QStringLiteral("За мужество наградить водителя майора милиции Иванова Ивана Ивановича "
                          "медалью «За отвагу» (посмертно)."),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& person = m_parser->m_Persons.front();
    QCOMPARE(person.AwardId, 14);
    QCOMPARE(text(person.Name), QStringLiteral("Иванов Иван Иванович"));
    QCOMPARE(text(m_parser->GetPersonDescription(person, false)), QStringLiteral("водитель"));
    QCOMPARE(text(person.MilitaryRank), QStringLiteral("майор милиции"));
    QVERIFY(!text(person.Description).contains(QStringLiteral("(посмертно)")));
    QVERIFY(!text(person.Description).contains(QStringLiteral("отвагу")));
    QVERIFY(person.Posthumous);
}

// нынешняя вёрстка указа: таблица «Фамилия<br>Имя Отчество», «–», должность
void TestDocumentParser::listWithLineBreaks()
{
    parse({kUkazRu, QStringLiteral("9 апреля 1999 г.№ 206"),
           QStringLiteral("За мужество и отвагу наградить:"),
           QStringLiteral("медалью «За отвагу»"),
           QStringLiteral("Лузгина\vВладимира Степановича"), QStringLiteral("–"),
           QStringLiteral("заместителя начальника отдела майора милиции"),
           QStringLiteral("Пусенкова\vИгоря Петровича"), QStringLiteral("–"),
           QStringLiteral("водителя автобуса"),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 2);

    PersonInfo& first = m_parser->m_Persons[0];
    QCOMPARE(first.AwardId, 14);
    QCOMPARE(text(first.Name), QStringLiteral("Лузгин Владимир Степанович"));
    QCOMPARE(text(m_parser->GetPersonDescription(first, false)), QStringLiteral("заместитель начальника отдела"));
    QCOMPARE(text(first.MilitaryRank), QStringLiteral("майор милиции"));
    QCOMPARE(text(m_parser->GetPersonDescription(0)), QStringLiteral("заместитель начальника отдела, майор милиции"));

    PersonInfo& second = m_parser->m_Persons[1];
    QCOMPARE(text(second.Name), QStringLiteral("Пусенков Игорь Петрович"));
    QCOMPARE(text(m_parser->GetPersonDescription(second, false)), QStringLiteral("водитель автобуса"));
    QVERIFY(second.MilitaryRank.empty());
}

// городской раздел списка («па г. Мінску») даёт место домохозяйке,
// областной раздел имеет место в тексте указа и сбрасывает его
void TestDocumentParser::housewifeGetsTheCityOfTheSection()
{
    parse({kUkazBy, QStringLiteral("24 лютага 2023 г. № 56"),
           QStringLiteral("За нараджэнне і выхаванне пяці і больш дзяцей узнагародзіць ордэнам Маці:"),
           QStringLiteral("па г. Мінску"),
           QStringLiteral("Абрамук\vТаццяну Мікалаеўну"), QStringLiteral("–"),
           QStringLiteral("хатнюю гаспадыню"),
           QStringLiteral("Аксененка\vКацярыну Мікалаеўну"), QStringLiteral("–"),
           QStringLiteral("хатнюю гаспадыню, г. Мінск"),
           QStringLiteral("па Брэсцкай вобласці"),
           QStringLiteral("Алейнікаву\vІрыну Мікалаеўну"), QStringLiteral("–"),
           QStringLiteral("хатнюю гаспадыню"),
           QStringLiteral("па гораду Мінску"), // указ от 30 мая 2022 г. № 185
           QStringLiteral("Ганчарэнка\vЯўгенію Іванаўну"), QStringLiteral("–"),
           QStringLiteral("хатнюю гаспадыню"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 4);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[3], false)), QStringLiteral("хатняя гаспадыня, г. Мінск"));
    QCOMPARE(m_parser->m_Persons[0].AwardId, 13);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[0], false)), QStringLiteral("хатняя гаспадыня, г. Мінск"));
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[1], false)), QStringLiteral("хатняя гаспадыня, г. Мінск"));
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[2], false)), QStringLiteral("хатняя гаспадыня"));
}

void TestDocumentParser::listEndingWithDashDoesNotCrash()
{
    parse({kUkazRu, QStringLiteral("9 апреля 1999 г.№ 206"),
           QStringLiteral("наградить"), QStringLiteral("медалью «За отвагу»:"),
           QStringLiteral("Лузгина\vВладимира Степановича"), QStringLiteral("–"), QStringLiteral("водителя"),
           QStringLiteral("Пусенкова\vИгоря Петровича"), QStringLiteral("–")});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    QCOMPARE(text(m_parser->m_Persons.front().Name), QStringLiteral("Лузгин Владимир Степанович"));
}

void TestDocumentParser::listStartingWithDashDoesNotCrash()
{
    // строка награды сразу с тире: строки имени перед ней нет
    std::vector<std::wstring> lines = {L"– водителя"};
    m_parser->ProcessFile(L"test.docx", lines);
    QVERIFY(m_parser->m_Persons.empty());
}

void TestDocumentParser::belarusianDecree()
{
    parse({kUkazBy, QStringLiteral("23 лютага 2006 г. № 120"),
           QStringLiteral("За асабістую мужнасць узнагародзіць:"),
           QStringLiteral("медалём «За адвагу»"),
           QStringLiteral("Лагодзiча\vЮрыя Iванавiча"), QStringLiteral("–"),
           QStringLiteral("старшага інспектара, капітана"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& person = m_parser->m_Persons.front();
    QVERIFY(!person.Ukaz.RussianLang);
    QCOMPARE(person.Ukaz.Date, QDate(2006, 2, 23));
    QCOMPARE(person.AwardId, 14);
    QCOMPARE(text(person.Name), QStringLiteral("Лагодзіч Юрый Іванавіч"));
    QCOMPARE(text(person.MilitaryRank), QStringLiteral("капітан"));
    QCOMPARE(text(m_parser->GetPersonDescription(person, false)), QStringLiteral("старшы інспектар"));
}

// «Дзіцячы сад – сярэдняя школа» — название, тире не начинает вторую
// должность; тире после вложенных кавычек («НВА «Цэнтр» – ...) всё ещё начинает
void TestDocumentParser::dashInsideQuotesIsNotSecondProfession()
{
    parse({kUkazBy, QStringLiteral("23 лютага 2006 г. № 120"),
           QStringLiteral("За шматгадовую працу ўзнагародзіць:"),
           QStringLiteral("медалём «За адвагу»"),
           QStringLiteral("Лагодзiча\vЮрыя Iванавiча"), QStringLiteral("–"),
           QStringLiteral("слесара ўстановы «Дзіцячы сад – сярэдняя школа»"),
           QStringLiteral("Іванова\vПятра Сяргеевіча"), QStringLiteral("–"),
           QStringLiteral("слесара прадпрыемства «НВА «Цэнтр» – слесара цэнтра"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 2);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[0], false)),
             QStringLiteral("слесар ўстановы «Дзіцячы сад – сярэдняя школа»"));
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[1], false)),
             QStringLiteral("слесар прадпрыемства «НВА «Цэнтр» – слесар цэнтра"));
}

void TestDocumentParser::twoDecreesInOneFile()
{
    parse({kUkazRu, QStringLiteral("9 апреля 1999 г.№ 206"),
           QStringLiteral("наградить"), QStringLiteral("медалью «За отвагу»"),
           QStringLiteral("Лузгина\vВладимира Степановича"), QStringLiteral("–"), QStringLiteral("водителя"),
           kPresident,
           kUkazRu, QStringLiteral("15 мая 1997 г.№ 282"),
           QStringLiteral("За рождение и воспитание пятерых и более детей наградить орденом Матери:"),
           QStringLiteral("Бодялову\vМарию Николаевну"), QStringLiteral("–"), QStringLiteral("работницу совхоза"),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 2);
    QCOMPARE(m_parser->m_Persons[0].Ukaz.Number, 206);
    QCOMPARE(m_parser->m_Persons[0].AwardId, 14);
    QCOMPARE(m_parser->m_Persons[1].Ukaz.Number, 282);
    QCOMPARE(m_parser->m_Persons[1].Ukaz.Date, QDate(1997, 5, 15));
    QCOMPARE(m_parser->m_Persons[1].AwardId, 13);
    QCOMPARE(text(m_parser->m_Persons[1].Name), QStringLiteral("Бодялова Мария Николаевна"));
}

// номера указов каждый год с начала: № 10 2019 года и № 10 2020 года — разные указы
void TestDocumentParser::groupInfoBelongsToItsDecree()
{
    const auto decree = [](const QString& date, const QString& company) {
        return QStringList{kUkazRu, date + QStringLiteral(" г. № 10"),
                           QStringLiteral("наградить группу работников %1 государственными наградами:").arg(company),
                           QStringLiteral("медалью «За отвагу»"),
                           QStringLiteral("Лузгина\vВладимира Степановича"), QStringLiteral("–"), QStringLiteral("водителя"),
                           kPresident};
    };
    parse(decree(QStringLiteral("10 января 2019"), QStringLiteral("ОАО «Нафтан»")), QStringLiteral("2019.docx"));
    parse(decree(QStringLiteral("10 января 2020"), QStringLiteral("ОАО «Гродно Азот»")), QStringLiteral("2020.docx"));
    QCOMPARE(int(m_parser->m_Persons.size()), 2);
    QCOMPARE(int(m_parser->m_GroupsInfo.size()), 2);

    // сведения о группе добавляются, только когда пользователь отметил их для файла
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[0], false)), QStringLiteral("водитель"));
    m_parser->CheckGroupInfo(L"2019.docx", true);
    m_parser->CheckGroupInfo(L"2020.docx", true);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[0], false)),
             QStringLiteral("водитель работников ОАО «Нафтан»"));
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[1], false)),
             QStringLiteral("водитель работников ОАО «Гродно Азот»"));
}

// цель после организации отрезается («у развіццё ...»), организация
// второй раз не добавляется, «ваеннаслужачых» тоже начинает сведения о группе
void TestDocumentParser::groupInfoIsTheOrganisationOnly()
{
    parse({kUkazBy, QStringLiteral("31 ліпеня 2001 г. № 412"),
           QStringLiteral("За плённую працу, вялікі асабісты ўклад работнікаў рэспубліканскага унітарнага прадпрыемства "
                          "«Беларускі металургічны завод» у развіццё металургіі ўзнагародзіць:"),
           QStringLiteral("медалём «За адвагу»"),
           QStringLiteral("Зотава\vВалерыя Уладзіміравіча"), QStringLiteral("–"), QStringLiteral("слесара"),
           QStringLiteral("Кайко\vАляксандра Іванавіча"), QStringLiteral("–"),
           QStringLiteral("слесара рэспубліканскага унітарнага прадпрыемства «Беларускі металургічны завод»"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")}, QStringLiteral("412.docx"));
    m_parser->CheckGroupInfo(L"412.docx", true);
    QCOMPARE(int(m_parser->m_Persons.size()), 2);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[0], false)),
             QStringLiteral("слесар рэспубліканскага унітарнага прадпрыемства «Беларускі металургічны завод»"));
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[1], false)),
             QStringLiteral("слесар рэспубліканскага унітарнага прадпрыемства «Беларускі металургічны завод»"));

    parse({kUkazBy, QStringLiteral("4 жніўня 2005 г. № 351"),
           QStringLiteral("За ўзорнае выкананне службовых абавязкаў узнагародзіць медалём «За адвагу» "
                          "ваеннаслужачых Службы бяспекі Прэзідэнта Рэспублікі Беларусь:"),
           QStringLiteral("Грыба\vСяргея Іванавіча"), QStringLiteral("–"), QStringLiteral("старшага інспектара аддзела, маёра"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")}, QStringLiteral("351.docx"));
    m_parser->CheckGroupInfo(L"351.docx", true);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons.back(), false)),
             QStringLiteral("старшы інспектар аддзела Службы бяспекі Прэзідэнта Рэспублікі Беларусь"));
}

// «В.И.Ленина» не обрывает организацию, институт встаёт перед учёной степенью
void TestDocumentParser::groupKeepsInitialsAndPrecedesTheDegree()
{
    parse({kUkazRu, QStringLiteral("12 мая 1996 г. № 170"),
           QStringLiteral("наградить работников Минского тракторного завода имени В.И.Ленина медалью «За отвагу»:"),
           QStringLiteral("Астапенко\vВиктора Андреевича"), QStringLiteral("–"),
           QStringLiteral("начальника сталелитейного цеха"),
           kPresident}, QStringLiteral("170.docx"));
    m_parser->CheckGroupInfo(L"170.docx", true);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons.front(), false)),
             QStringLiteral("начальник сталелитейного цеха Минского тракторного завода имени В.И.Ленина"));

    parse({kUkazRu, QStringLiteral("12 мая 1996 г. № 171"),
           QStringLiteral("наградить работников Минского государственного медицинского института медалью «За отвагу»:"),
           QStringLiteral("Мельниченко\vЭдуарда Михайловича"), QStringLiteral("–"),
           QStringLiteral("заведующего кафедрой стоматологии детского возраста, профессора, доктора медицинских наук"),
           kPresident}, QStringLiteral("171.docx"));
    m_parser->CheckGroupInfo(L"171.docx", true);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons.back(), false)),
             QStringLiteral("заведующий кафедрой стоматологии детского возраста Минского государственного медицинского института, профессор, доктор медицинских наук"));
}

// указы 245/2026, 33/2023 и 361/2021: награждена сама организация.
// имя пустое, название целиком лежит в личных данных
void TestDocumentParser::organizationIsAwardedItself()
{
    parse({kUkazBy, QStringLiteral("22 ліпеня 2026 г. № 245"),
           QStringLiteral("За значны ўклад у развіццё перапрацоўчай прамысловасці, эфектыўную дзейнасць "
                          "і выраб прадукцыі высокай якасці ўзнагародзіць адкрытае\u00A0акцыянернае таварыства "
                          "«Савушкін прадукт» ордэнам Працоўнай Славы."),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& savushkin = m_parser->m_Persons.front();
    QCOMPARE(savushkin.AwardId, 24);
    QCOMPARE(savushkin.Ukaz.Number, 245);
    QCOMPARE(savushkin.Ukaz.Date, QDate(2026, 7, 22));
    QVERIFY(savushkin.Organization);
    QVERIFY(savushkin.Name.empty());
    QVERIFY(savushkin.Flags & PersonInfo::NF_NAME);
    QCOMPARE(text(m_parser->GetPersonDescription(savushkin, false)),
             QStringLiteral("Адкрытае акцыянернае таварыства «Савушкін прадукт»"));

    m_parser->ResetData();
    parse({kUkazBy, QStringLiteral("10 лютага 2023 г. № 33"),
           QStringLiteral("За ўклад у развіццё перапрацоўчай прамысловасці ўзнагародзіць адкрытае акцыянернае "
                          "таварыства «Малочны Мір» ордэнам Працоўнай Славы."),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& milk = m_parser->m_Persons.front();
    QCOMPARE(milk.AwardId, 24);
    QVERIFY(milk.Organization);
    QVERIFY(milk.Name.empty());
    QCOMPARE(text(m_parser->GetPersonDescription(milk, false)),
             QStringLiteral("Адкрытае акцыянернае таварыства «Малочны Мір»"));

    m_parser->ResetData();
    parse({kUkazBy, QStringLiteral("22 верасня 2021 г. № 361"),
           QStringLiteral("За значны ўклад у захаванне гісторыка-культурнай спадчыны ўзнагародзіць дзяржаўную ўстанову "
                          "«Мемарыяльны комплекс «Брэсцкая крэпасць-герой» ордэнам Францыска Скарыны."),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& fortress = m_parser->m_Persons.front();
    QCOMPARE(fortress.AwardId, 11);
    QVERIFY(fortress.Organization);
    QVERIFY(fortress.Name.empty());
    QCOMPARE(text(m_parser->GetPersonDescription(fortress, false)),
             QStringLiteral("Дзяржаўную ўстанову «Мемарыяльны комплекс «Брэсцкая крэпасць-герой»"));

    m_parser->ResetData();
    parse({kUkazRu, QStringLiteral("10 февраля 2023 г. № 33"),
           QStringLiteral("За вклад в развитие промышленности наградить открытое акционерное общество "
                          "«Молочный мир» орденом Трудовой Славы."),
           kPresident});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& russian = m_parser->m_Persons.front();
    QCOMPARE(russian.AwardId, 24);
    QVERIFY(russian.Organization);
    QVERIFY(russian.Name.empty());
    QCOMPARE(text(m_parser->GetPersonDescription(russian, false)),
             QStringLiteral("Открытое акционерное общество «Молочный мир»"));
}

// указы 386/2024, 298/2026 и 306/2026: организация — строка списка под наградой,
// название и район вместе в личных данных, тире внутри названия не режет строку
void TestDocumentParser::organizationInAwardList()
{
    parse({kUkazBy, QStringLiteral("1 кастрычніка 2024 г. № 386"),
           QStringLiteral("За значны ўклад у развіццё аграпрамысловага комплексу:"),
           QStringLiteral("1. Узнагародзіць:"),
           QStringLiteral("ордэнам Працоўнай Славы"),
           QStringLiteral("адкрытае акцыянернае таварыства «Парахонскае», Пінскі раён"),
           QStringLiteral("ордэнам Пашаны"),
           QStringLiteral("Іванова\vІвана Іванавіча"), QStringLiteral("–"),
           QStringLiteral("вадзіцеля аўтамабіля адкрытага акцыянернага таварыства «Калінінскі», Добрушскі раён"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 2);
    PersonInfo& parokhonskoe = m_parser->m_Persons[0];
    QVERIFY(parokhonskoe.Organization);
    QVERIFY(parokhonskoe.Name.empty());
    QCOMPARE(parokhonskoe.AwardId, 24);
    QCOMPARE(text(m_parser->GetPersonDescription(parokhonskoe, false)),
             QStringLiteral("Адкрытае акцыянернае таварыства «Парахонскае», Пінскі раён"));
    QVERIFY(!m_parser->m_Persons[1].Organization);
    QCOMPARE(m_parser->m_Persons[1].AwardId, 10);
    QCOMPARE(text(m_parser->m_Persons[1].Name), QStringLiteral("Іваноў Іван Іванавіч"));

    m_parser->ResetData();
    parse({kUkazBy, QStringLiteral("9 верасня 2026 г. № 306"),
           QStringLiteral("За працу:"),
           QStringLiteral("1. Узнагародзіць:"),
           QStringLiteral("ордэнам Працоўнай Славы"),
           QStringLiteral("адкрытае акцыянернае таварыства «Планар»;"),
           QStringLiteral("адкрытае акцыянернае таварыства «ІНТЭГРАЛ» – кіруючая кампанія холдынга «ІНТЭГРАЛ»"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 2);
    QVERIFY(m_parser->m_Persons[0].Organization);
    QVERIFY(m_parser->m_Persons[1].Organization);
    QVERIFY(m_parser->m_Persons[0].Name.empty());
    QVERIFY(m_parser->m_Persons[1].Name.empty());
    QCOMPARE(m_parser->m_Persons[0].AwardId, 24);
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[0], false)),
             QStringLiteral("Адкрытае акцыянернае таварыства «Планар»"));
    QCOMPARE(text(m_parser->GetPersonDescription(m_parser->m_Persons[1], false)),
             QStringLiteral("Адкрытае акцыянернае таварыства «ІНТЭГРАЛ» – кіруючая кампанія холдынга «ІНТЭГРАЛ»"));
}

// родительный падеж — место работы человека; список после двоеточия тоже люди
void TestDocumentParser::workplaceIsNotAnOrganization()
{
    parse({kUkazBy, QStringLiteral("10 лютага 2023 г. № 33"),
           QStringLiteral("За працу ўзнагародзіць слесара адкрытага акцыянернага таварыства «Малочны Мір» "
                          "Іванова Івана Іванавіча медалём «За адвагу»."),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    PersonInfo& worker = m_parser->m_Persons.front();
    QVERIFY(!worker.Organization);
    QCOMPARE(worker.AwardId, 14);
    QCOMPARE(text(worker.Name), QStringLiteral("Іваноў Іван Іванавіч"));
    QCOMPARE(text(m_parser->GetPersonDescription(worker, false)),
             QStringLiteral("слесар адкрытага акцыянернага таварыства «Малочны Мір»"));

    m_parser->ResetData();
    parse({kUkazBy, QStringLiteral("1 студзеня 2020 г. № 1"),
           QStringLiteral("За працу ўзнагародзіць калектыў адкрытага акцыянернага таварыства «Малочны Мір» "
                          "ордэнам Працоўнай Славы:"),
           QStringLiteral("Іванова\vІвана Іванавіча"), QStringLiteral("–"), QStringLiteral("слесара"),
           QStringLiteral("Прэзідэнт Рэспублікі Беларусь")});
    QCOMPARE(int(m_parser->m_Persons.size()), 1);
    QVERIFY(!m_parser->m_Persons.front().Organization);
    QCOMPARE(text(m_parser->m_Persons.front().Name), QStringLiteral("Іваноў Іван Іванавіч"));
}

void TestDocumentParser::resetClearsEverything()
{
    parse({kUkazRu, QStringLiteral("10 января 2019 г. № 10"),
           QStringLiteral("наградить группу работников ОАО «Нафтан» государственными наградами:"),
           QStringLiteral("медалью «За отвагу»"),
           QStringLiteral("Лузгина\vВладимира Степановича"), QStringLiteral("–"), QStringLiteral("водителя")});
    QVERIFY(!m_parser->m_Persons.empty());
    QVERIFY(!m_parser->m_GroupsInfo.empty());
    QVERIFY(!m_parser->m_AwardsStat.empty());

    m_parser->ResetData();
    QVERIFY(m_parser->m_Persons.empty());
    QVERIFY(m_parser->m_GroupsInfo.empty());
    QVERIFY(m_parser->m_AwardsStat.empty());
    QCOMPARE(m_parser->m_UkazInfo.Number, 0);
}

QTEST_GUILESS_MAIN(TestDocumentParser)
#include "tst_documentparser.moc"
