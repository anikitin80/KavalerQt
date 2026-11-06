// CNameConverter: full names from the decree text (dative for honorary titles
// and "Hero", accusative for orders and medals) to the nominative case.

#include "NameConverter.h"

#include <QtTest>

Q_DECLARE_METATYPE(LangType)
Q_DECLARE_METATYPE(NounCase)

class TestNameConverter : public QObject
{
    Q_OBJECT

private slots:
    void normalizeName_data();
    void normalizeName();
    void foreignNameWithoutPatronymic();
    void uncertainSurnameIsReported();
    void aliasAtStartDoesNotThrow();
    void isMiddleName_data();
    void isMiddleName();
    void sex();
};

void TestNameConverter::normalizeName_data()
{
    QTest::addColumn<QString>("decree");
    QTest::addColumn<LangType>("lang");
    QTest::addColumn<NounCase>("nounCase");
    QTest::addColumn<QString>("expected");

    // accusative: "наградить орденом ... Бодялову Марию Николаевну"
    QTest::newRow("ru acc female") << QStringLiteral("Бодялову Марию Николаевну") << LangRU << Accusative
                                   << QStringLiteral("Бодялова Мария Николаевна");
    QTest::newRow("ru acc male -ев") << QStringLiteral("Худолеева Александра Николаевича") << LangRU << Accusative
                                     << QStringLiteral("Худолеев Александр Николаевич");
    QTest::newRow("ru acc male soft") << QStringLiteral("Пусенкова Игоря Петровича") << LangRU << Accusative
                                      << QStringLiteral("Пусенков Игорь Петрович");
    // dative: "присвоить почетное звание ... Рускевичу Владимиру Сафроновичу"
    QTest::newRow("ru dat male") << QStringLiteral("Рускевичу Владимиру Сафроновичу") << LangRU << Dative
                                 << QStringLiteral("Рускевич Владимир Сафронович");
    QTest::newRow("ru dat male -ей") << QStringLiteral("Хомчику Алексею Сидоровичу") << LangRU << Dative
                                     << QStringLiteral("Хомчик Алексей Сидорович");
    QTest::newRow("ru dat female") << QStringLiteral("Петровой Анне Сергеевне") << LangRU << Dative
                                   << QStringLiteral("Петрова Анна Сергеевна");
    QTest::newRow("ru dat exception Павел") << QStringLiteral("Сидорову Павлу Ивановичу") << LangRU << Dative
                                            << QStringLiteral("Сидоров Павел Иванович");
    QTest::newRow("ru dat exception Лев") << QStringLiteral("Сидорову Льву Ивановичу") << LangRU << Dative
                                          << QStringLiteral("Сидоров Лев Иванович");

    QTest::newRow("by acc female -ава") << QStringLiteral("Багданаву Таццяну Аляксандраўну") << LangBY << Accusative
                                        << QStringLiteral("Багданава Таццяна Аляксандраўна");
    QTest::newRow("by acc female consonant") << QStringLiteral("Міхалевіч Ірыну Іванаўну") << LangBY << Accusative
                                             << QStringLiteral("Міхалевіч Ірына Іванаўна");
    QTest::newRow("by acc female -нка") << QStringLiteral("Сцепаненка Яўгенію Пятроўну") << LangBY << Accusative
                                        << QStringLiteral("Сцепаненка Яўгенія Пятроўна");
    QTest::newRow("by acc male -ч") << QStringLiteral("Лагодзіча Юрыя Іванавіча") << LangBY << Accusative
                                    << QStringLiteral("Лагодзіч Юрый Іванавіч");
    QTest::newRow("by acc male -кі") << QStringLiteral("Цяцюцкага Васілія Васільевіча") << LangBY << Accusative
                                     << QStringLiteral("Цяцюцкі Васілій Васільевіч");
    QTest::newRow("by acc male -ы") << QStringLiteral("Харужага Уладзіміра Сцяпанавіча") << LangBY << Accusative
                                    << QStringLiteral("Харужы Уладзімір Сцяпанавіч");
    QTest::newRow("by acc male -аў") << QStringLiteral("Старацітарава Аляксандра Мікалаевіча") << LangBY << Accusative
                                     << QStringLiteral("Старацітараў Аляксандр Мікалаевіч");
    QTest::newRow("by acc male fleeting vowel") << QStringLiteral("Азаронка Рыгора Юр’евіча") << LangBY << Accusative
                                                << QStringLiteral("Азаронак Рыгор Юр’евіч");
    QTest::newRow("by acc male -а surname") << QStringLiteral("Шульгу Валерыя Ігаравіча") << LangBY << Accusative
                                            << QStringLiteral("Шульга Валерый Ігаравіч");
    QTest::newRow("by dat male -оў, exception Пётр") << QStringLiteral("Іванову Пятру Сяргеевічу") << LangBY << Dative
                                                     << QStringLiteral("Іваноў Пётр Сяргеевіч");
    // Latin "i" typed instead of Belarusian "і" is fixed on the way
    QTest::newRow("by latin i") << QStringLiteral("Лагодзiча Юрыя Іванавiча") << LangBY << Accusative
                                << QStringLiteral("Лагодзіч Юрый Іванавіч");
    QTest::newRow("by latin capital I") << QStringLiteral("Лагодзiча Юрыя Iванавiча") << LangBY << Accusative
                                        << QStringLiteral("Лагодзіч Юрый Іванавіч");
}

void TestNameConverter::normalizeName()
{
    QFETCH(QString, decree);
    QFETCH(LangType, lang);
    QFETCH(NounCase, nounCase);
    QFETCH(QString, expected);

    CNameConverter converter;
    std::wstring name = decree.toStdWString();
    QVERIFY(converter.NormalizeName(name, lang, nounCase));
    // Known: capital Latin "I" ("Iгар", "Iванавіч") stays Latin. The catalog already
    // stores ~1000 Belarusian names that way, and duplicates are found by exact
    // name, so fixing only the parser would re-import those people as new ones.
    QEXPECT_FAIL("by latin capital I", "capital Latin I is not replaced; needs catalog cleanup first", Continue);
    QCOMPARE(QString::fromStdWString(name), expected);
}

// foreigners: "Джону Смиту" (first name first), the order is kept
void TestNameConverter::foreignNameWithoutPatronymic()
{
    CNameConverter converter;
    std::wstring name = L"Джону Смиту";
    QVERIFY(converter.NormalizeName(name, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Джон Смит"));
}

// "-ому" may be "-ый" or "-ой": the result is a guess and is reported as such
void TestNameConverter::uncertainSurnameIsReported()
{
    CNameConverter converter;
    std::wstring name = L"Толстому Ивану Петровичу";
    QVERIFY(!converter.NormalizeName(name, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Толстый Иван Петрович"));
}

void TestNameConverter::aliasAtStartDoesNotThrow()
{
    CNameConverter converter;
    std::wstring name = L"(Брылю) Івану Антонавічу";
    try {
        converter.NormalizeName(name, LangBY, Dative);
    } catch (...) {
        QFAIL("NormalizeName threw on an alias at the start of the name");
    }
}

void TestNameConverter::isMiddleName_data()
{
    QTest::addColumn<QString>("word");
    QTest::addColumn<LangType>("lang");
    QTest::addColumn<NounCase>("nounCase");
    QTest::addColumn<bool>("expected");
    QTest::newRow("ru dat male") << QStringLiteral("Ивановичу") << LangRU << Dative << true;
    QTest::newRow("ru acc male") << QStringLiteral("Ивановича") << LangRU << Accusative << true;
    QTest::newRow("ru dat female") << QStringLiteral("Ивановне") << LangRU << Dative << true;
    QTest::newRow("ru acc female") << QStringLiteral("Ивановну") << LangRU << Accusative << true;
    QTest::newRow("ru upper case") << QStringLiteral("ИВАНОВИЧУ") << LangRU << Dative << true;
    QTest::newRow("by dat male") << QStringLiteral("Іванавічу") << LangBY << Dative << true;
    QTest::newRow("by acc male") << QStringLiteral("Іванавіча") << LangBY << Accusative << true;
    QTest::newRow("surname") << QStringLiteral("Иванову") << LangRU << Dative << false;
    QTest::newRow("first name") << QStringLiteral("Ивану") << LangRU << Dative << false;
}

void TestNameConverter::isMiddleName()
{
    QFETCH(QString, word);
    QFETCH(LangType, lang);
    QFETCH(NounCase, nounCase);
    QFETCH(bool, expected);
    CNameConverter converter;
    QCOMPARE(converter.IsMiddleName(word.toStdWString(), lang, nounCase), expected);
}

void TestNameConverter::sex()
{
    CNameConverter converter;
    std::wstring female = L"Николаевну";
    std::wstring femaleDat = L"Николаевне";
    std::wstring male = L"Николаевича";
    QCOMPARE(converter.GetSex(female, Accusative), SexFemale);
    QCOMPARE(converter.GetSex(femaleDat, Dative), SexFemale);
    QCOMPARE(converter.GetSex(male, Accusative), SexMale);
}

QTEST_GUILESS_MAIN(TestNameConverter)
#include "tst_nameconverter.moc"
