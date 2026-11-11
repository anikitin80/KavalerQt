// CRankConverter: склонённое воинское звание из текста должности
// становится именительным и вырезается из строки.

#include "RankConverter.h"

#include <QtTest>

Q_DECLARE_METATYPE(LangType)
Q_DECLARE_METATYPE(NounCase)

namespace {

QString text(const std::wstring& value)
{
    return QString::fromStdWString(value);
}

} // namespace

class TestRankConverter : public QObject
{
    Q_OBJECT

private slots:
    void extract_data();
    void extract();
};

void TestRankConverter::extract_data()
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
    // звание в конце без запятой, хотя запятая в должности есть
    QTest::newRow("by trailing without comma") << QStringLiteral("памочніка начальніка аддзела, па барацьбе з карупцыяй Камітэта дзяржаўнай бяспекі маёра")
                                               << LangBY << Accusative << QStringLiteral("маёр")
                                               << QStringLiteral("памочніка начальніка аддзела, па барацьбе з карупцыяй Камітэта дзяржаўнай бяспекі");
    QTest::newRow("by trailing генерал-палкоўнік") << QStringLiteral("члена Дзяржаўнага Савета, Міністра абароны Кітайскай Народнай Рэспублікі генерал-палкоўніка")
                                                   << LangBY << Accusative << QStringLiteral("генерал-палкоўнік")
                                                   << QStringLiteral("члена Дзяржаўнага Савета, Міністра абароны Кітайскай Народнай Рэспублікі");
    // звание первым, должность после запятой
    QTest::newRow("ru leading rank") << QStringLiteral("полковнику, начальнику 927-го управления") << LangRU << Dative
                                     << QStringLiteral("полковник") << QStringLiteral("начальнику 927-го управления");
    QTest::newRow("ru leading rank with tail") << QStringLiteral("полковнику милиции, начальнику отдела") << LangRU << Dative
                                               << QStringLiteral("полковник милиции") << QStringLiteral("начальнику отдела");
    // должности, не звания
    QTest::newRow("by памочнік капітана is a position") << QStringLiteral("старшага памочніка капітана")
                                                        << LangBY << Accusative << QString() << QStringLiteral("старшага памочніка капітана");
    QTest::newRow("ru старшина группы is a position") << QStringLiteral("старшине группы, водителю") << LangRU << Dative
                                                      << QString() << QStringLiteral("старшине группы, водителю");
    QTest::newRow("by саветнік is a position") << QStringLiteral("саветніка ўпраўлення кадравай работы")
                                               << LangBY << Accusative << QString() << QStringLiteral("саветніка ўпраўлення кадравай работы");
    QTest::newRow("by капітан-механік is a position") << QStringLiteral("капітана-механіка прадпрыемства водных шляхоў")
                                                      << LangBY << Accusative << QString() << QStringLiteral("капітана-механіка прадпрыемства водных шляхоў");
    QTest::newRow("by капітан каманды is a position") << QStringLiteral("капітана нацыянальнай каманды Рэспублікі Беларусь па тэнісу")
                                                      << LangBY << Accusative << QString()
                                                      << QStringLiteral("капітана нацыянальнай каманды Рэспублікі Беларусь па тэнісу");
}

void TestRankConverter::extract()
{
    QFETCH(QString, description);
    QFETCH(LangType, lang);
    QFETCH(NounCase, nounCase);
    QFETCH(QString, rank);
    QFETCH(QString, rest);

    std::wstring source = description.toStdWString();
    std::wstring extracted;
    QCOMPARE(CRankConverter::Extract(source, extracted, lang, nounCase), !rank.isEmpty());
    QCOMPARE(text(extracted), rank);
    QCOMPARE(text(source), rest);
}

QTEST_GUILESS_MAIN(TestRankConverter)
#include "tst_rankconverter.moc"
