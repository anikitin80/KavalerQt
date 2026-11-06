// TextUtil: dates in decree headers and small string helpers used by the parser.

#include "TextUtil.h"

#include <QDate>
#include <QtTest>

class TestTextUtil : public QObject
{
    Q_OBJECT

private slots:
    void parseDate_data();
    void parseDate();
    void strTrim_data();
    void strTrim();
    void splitString();
    void findTokens();
    void isCapital();
    void toLower();
    void normalizeBelarusianI_data();
    void normalizeBelarusianI();
    void normalizeCyrillicText_data();
    void normalizeCyrillicText();
};

void TestTextUtil::parseDate_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("russian");
    QTest::addColumn<QDate>("expected");

    // the parser passes the header text before "г." ("15 мая 1997 г.№ 282")
    QTest::newRow("ru may") << QStringLiteral("15 мая 1997 ") << true << QDate(1997, 5, 15);
    QTest::newRow("ru april") << QStringLiteral("9 апреля 1999 ") << true << QDate(1999, 4, 9);
    QTest::newRow("ru december") << QStringLiteral("31 декабря 2020") << true << QDate(2020, 12, 31);
    QTest::newRow("ru spaces inside") << QStringLiteral(" 1 0  октября 1996 ") << true << QDate(1996, 10, 10);
    QTest::newRow("by january") << QStringLiteral("28 студзеня 2015 ") << false << QDate(2015, 1, 28);
    QTest::newRow("by march") << QStringLiteral("1 сакавіка 2020") << false << QDate(2020, 3, 1);
    QTest::newRow("by october") << QStringLiteral("1 кастрычніка 2024 ") << false << QDate(2024, 10, 1);
    QTest::newRow("by november") << QStringLiteral("6 лістапада 2025") << false << QDate(2025, 11, 6);
    // decrees often type the Belarusian "і" as Latin "i"
    QTest::newRow("by july latin i") << QStringLiteral("2 лiпеня 2019") << false << QDate(2019, 7, 2);
    QTest::newRow("by may same as ru")<< QStringLiteral("5 мая 2016") << false << QDate(2016, 5, 5);
    QTest::newRow("wrong language months") << QStringLiteral("28 студзеня 2015") << true << QDate();
    QTest::newRow("no such day") << QStringLiteral("30 февраля 2020") << true << QDate();
    QTest::newRow("no month") << QStringLiteral("2020") << true << QDate();
    QTest::newRow("garbage") << QStringLiteral("Президент") << true << QDate();
    QTest::newRow("empty") << QString() << true << QDate();
}

void TestTextUtil::parseDate()
{
    QFETCH(QString, text);
    QFETCH(bool, russian);
    QFETCH(QDate, expected);
    QCOMPARE(ParseDate(text.toStdWString(), russian), expected);
}

void TestTextUtil::strTrim_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");
    QTest::newRow("spaces") << QStringLiteral("  водитель  ") << QStringLiteral("водитель");
    QTest::newRow("dots and spaces") << QStringLiteral(" . водитель автобуса . ") << QStringLiteral("водитель автобуса");
    QTest::newRow("inner dots kept") << QStringLiteral("г. Минск") << QStringLiteral("г. Минск");
    QTest::newRow("only separators") << QStringLiteral(" . . ") << QString();
    QTest::newRow("empty") << QString() << QString();
}

void TestTextUtil::strTrim()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    std::wstring value = text.toStdWString();
    StrTrim(value);
    QCOMPARE(QString::fromStdWString(value), expected);
}

void TestTextUtil::splitString()
{
    const auto split = [](const wchar_t* text) {
        QStringList result;
        for (const std::wstring& part : SplitString(text, L','))
            result.push_back(QString::fromStdWString(part));
        return result;
    };
    QCOMPARE(split(L"ому,ему"), QStringList({QStringLiteral("ому"), QStringLiteral("ему")}));
    QCOMPARE(split(L"а,,б"), QStringList({QStringLiteral("а"), QString(), QStringLiteral("б")}));
    QCOMPARE(split(L"о,е,э,"), QStringList({QStringLiteral("о"), QStringLiteral("е"), QStringLiteral("э"), QString()}));
    QCOMPARE(split(L"ичу"), QStringList({QStringLiteral("ичу")}));
    QCOMPARE(split(L""), QStringList({QString()}));
}

void TestTextUtil::findTokens()
{
    std::vector<std::wstring> tokens = {L"медалью", L"орденом"};
    size_t start = 0;
    size_t end = 0;
    QVERIFY(FindTokens(L"наградить орденом Почета", tokens, start, end));
    QCOMPARE(start, size_t(10));
    QCOMPARE(end, size_t(17));

    // the first token of the list wins, not the earliest position in the text
    QVERIFY(FindTokens(L"орденом и медалью", tokens, start, end));
    QCOMPARE(start, size_t(10));

    QVERIFY(!FindTokens(L"присвоить почетное звание", tokens, start, end));
}

void TestTextUtil::isCapital()
{
    for (wchar_t c : std::wstring(L"АБЯЁІI"))
        QVERIFY2(IsCapital(c), qPrintable(QString(QChar(c))));
    for (wchar_t c : std::wstring(L"абяёі1 -"))
        QVERIFY2(!IsCapital(c), qPrintable(QString(QChar(c))));
}

void TestTextUtil::toLower()
{
    QCOMPARE(QString::fromStdWString(ToLower(L"ЁЛКА Іван ЎЗБЕК Abc")), QStringLiteral("ёлка іван ўзбек abc"));
    QCOMPARE(QString::fromStdWString(ToLower(L"")), QString());
}

void TestTextUtil::normalizeBelarusianI_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    QTest::newRow("capital at word start") << QStringLiteral("Iгар") << QStringLiteral("Ігар");
    QTest::newRow("inside words") << QStringLiteral("Нiкiпарчык Раiса Мiкалаеўна")
                                  << QStringLiteral("Нікіпарчык Раіса Мікалаеўна");
    QTest::newRow("word end") << QStringLiteral("Рэспублiкi Беларусь") << QStringLiteral("Рэспублікі Беларусь");
    QTest::newRow("mixed with cyrillic") << QStringLiteral("Дзмiтрый Анатольевіч") << QStringLiteral("Дзмітрый Анатольевіч");
    QTest::newRow("in quotes") << QStringLiteral("«Мiнскi рысорны завод»") << QStringLiteral("«Мінскі рысорны завод»");
    QTest::newRow("conjunction") << QStringLiteral("кафедры лагістыкі i цэнавай палітыкі")
                                 << QStringLiteral("кафедры лагістыкі і цэнавай палітыкі");
    QTest::newRow("conjunction after comma") << QStringLiteral("аналізу, i аўдыту") << QStringLiteral("аналізу, і аўдыту");
    QTest::newRow("roman numeral") << QStringLiteral("юрыст I класа") << QStringLiteral("юрыст I класа");
    QTest::newRow("roman numerals") << QStringLiteral("ордэн Айчыны III ступені") << QStringLiteral("ордэн Айчыны III ступені");
    QTest::newRow("roman in quotes") << QStringLiteral("аддзела «I» 2-га ўпраўлення") << QStringLiteral("аддзела «I» 2-га ўпраўлення");
    QTest::newRow("latin word") << QStringLiteral("«Moeller Electrotechnika s.r.o.» прадстаўніцтва")
                                << QStringLiteral("«Moeller Electrotechnika s.r.o.» прадстаўніцтва");
    QTest::newRow("latin i between latin words") << QStringLiteral("Smith i Wesson") << QStringLiteral("Smith i Wesson");
    QTest::newRow("roman numeral XIX") << QStringLiteral("першай паловы XIX стагоддзя") << QStringLiteral("першай паловы XIX стагоддзя");
    QTest::newRow("already cyrillic") << QStringLiteral("Іваноў Ігар") << QStringLiteral("Іваноў Ігар");
    QTest::newRow("empty") << QString() << QString();
}

void TestTextUtil::normalizeBelarusianI()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);
    QCOMPARE(NormalizeBelarusianI(text), expected);
    QCOMPARE(HasLatinBelarusianI(text), text != expected);
}

void TestTextUtil::normalizeCyrillicText_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("belarusian");
    QTest::addColumn<QString>("expected");

    // real cases from the catalog
    QTest::newRow("ru ending") << QStringLiteral("прядильщицa фабрики") << false << QStringLiteral("прядильщица фабрики");
    QTest::newRow("ru surname") << QStringLiteral("Дорoxoва Анна Васильевна") << false << QStringLiteral("Дорохова Анна Васильевна");
    QTest::newRow("ru capital") << QStringLiteral("Aршaвская Ирина") << false << QStringLiteral("Аршавская Ирина");
    QTest::newRow("ru p for р") << QStringLiteral("проректоp университета") << false << QStringLiteral("проректор университета");
    QTest::newRow("ru whole word latin twins") << QStringLiteral("имени Mapкca") << false << QStringLiteral("имени Маркса");
    QTest::newRow("by name") << QStringLiteral("Шыбека Mixaіл Васільевіч") << true << QStringLiteral("Шыбека Міхаіл Васільевіч");
    QTest::newRow("by name with i") << QStringLiteral("Іcaeва Валянціна") << true << QStringLiteral("Ісаева Валянціна");
    QTest::newRow("by rank") << QStringLiteral("пaдпaлкоўнік") << true << QStringLiteral("падпалкоўнік");
    QTest::newRow("by i and c") << QStringLiteral("Kipaўcкі раён") << true << QStringLiteral("Кіраўскі раён");
    // standalone lower-case letters between Cyrillic words
    QTest::newRow("ru preposition c") << QStringLiteral("работает c 1990 года в колхозе") << false
                                      << QStringLiteral("работает с 1990 года в колхозе");
    QTest::newRow("ru conjunction a") << QStringLiteral("слесарь a также механик") << false << QStringLiteral("слесарь а также механик");
    QTest::newRow("by preposition y") << QStringLiteral("настаўнік y школе") << true << QStringLiteral("настаўнік у школе");
    // must stay
    QTest::newRow("roman latin") << QStringLiteral("першай паловы XIX стагоддзя") << true << QStringLiteral("першай паловы XIX стагоддзя");
    QTest::newRow("roman mixed script") << QStringLiteral("XXІ стагоддзя") << true << QStringLiteral("XXІ стагоддзя");
    QTest::newRow("roman ru") << QStringLiteral("участник XXIV съезда") << false << QStringLiteral("участник XXIV съезда");
    QTest::newRow("roman with hyphen") << QStringLiteral("XIX-га стагоддзя") << true << QStringLiteral("XIX-га стагоддзя");
    QTest::newRow("category letter") << QStringLiteral("водитель категории B") << false << QStringLiteral("водитель категории B");
    QTest::newRow("latin name") << QStringLiteral("ООО «Coca-Cola Beverages» в Минске") << false
                                << QStringLiteral("ООО «Coca-Cola Beverages» в Минске");
    QTest::newRow("latin words") << QStringLiteral("«Moeller Electrotechnika s.r.o.» прадстаўніцтва") << true
                                 << QStringLiteral("«Moeller Electrotechnika s.r.o.» прадстаўніцтва");
    QTest::newRow("ru keeps latin i") << QStringLiteral("водитель i механик") << false << QStringLiteral("водитель i механик");
    QTest::newRow("roman in name") << QStringLiteral("Алексий II (Ридигер Алексей Михайлович)") << false
                                   << QStringLiteral("Алексий II (Ридигер Алексей Михайлович)");
    QTest::newRow("clean") << QStringLiteral("Іваноў Ігар") << true << QStringLiteral("Іваноў Ігар");
}

void TestTextUtil::normalizeCyrillicText()
{
    QFETCH(QString, text);
    QFETCH(bool, belarusian);
    QFETCH(QString, expected);
    QCOMPARE(NormalizeCyrillicText(text, belarusian), expected);
    QCOMPARE(HasLatinLookalikes(text, belarusian), text != expected);
}

QTEST_GUILESS_MAIN(TestTextUtil)
#include "tst_textutil.moc"
