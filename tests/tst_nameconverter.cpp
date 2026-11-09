// CNameConverter: полные имена из текста указа (дательный для почётных званий
// и «Героя», винительный для орденов и медалей) в именительный падеж.

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
    void unknownPatronymicIsReported();
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

    // винительный: «наградить орденом ... Бодялову Марию Николаевну»
    QTest::newRow("ru acc female") << QStringLiteral("Бодялову Марию Николаевну") << LangRU << Accusative
                                   << QStringLiteral("Бодялова Мария Николаевна");
    QTest::newRow("ru acc male -ев") << QStringLiteral("Худолеева Александра Николаевича") << LangRU << Accusative
                                     << QStringLiteral("Худолеев Александр Николаевич");
    QTest::newRow("ru acc male soft") << QStringLiteral("Пусенкова Игоря Петровича") << LangRU << Accusative
                                      << QStringLiteral("Пусенков Игорь Петрович");
    // дательный: «присвоить почетное звание ... Рускевичу Владимиру Сафроновичу»
    QTest::newRow("ru dat male") << QStringLiteral("Рускевичу Владимиру Сафроновичу") << LangRU << Dative
                                 << QStringLiteral("Рускевич Владимир Сафронович");
    QTest::newRow("ru dat male -ей") << QStringLiteral("Хомчику Алексею Сидоровичу") << LangRU << Dative
                                     << QStringLiteral("Хомчик Алексей Сидорович");
    QTest::newRow("ru dat female") << QStringLiteral("Петровой Анне Сергеевне") << LangRU << Dative
                                   << QStringLiteral("Петрова Анна Сергеевна");
    // окончание отчества решает пол; заглавные не должны сделать её мужчиной
    QTest::newRow("ru dat female upper patronymic") << QStringLiteral("Никитиной Анне ИВАНОВНЕ") << LangRU << Dative
                                                    << QStringLiteral("Никитина Анна ИВАНОВНа");
    QTest::newRow("ru dat exception Павел") << QStringLiteral("Сидорову Павлу Ивановичу") << LangRU << Dative
                                            << QStringLiteral("Сидоров Павел Иванович");
    QTest::newRow("ru dat exception Лев") << QStringLiteral("Сидорову Льву Ивановичу") << LangRU << Dative
                                          << QStringLiteral("Сидоров Лев Иванович");

    // мужские имена на «-а»: «-у» винительного не остаётся
    QTest::newRow("by acc male Кузьма") << QStringLiteral("Лесняка Кузьму Мікалаевіча") << LangBY << Accusative
                                        << QStringLiteral("Лесняк Кузьма Мікалаевіч");
    QTest::newRow("by acc male Даніла") << QStringLiteral("Апарына Данілу Генадзьевіча") << LangBY << Accusative
                                        << QStringLiteral("Апарын Даніла Генадзьевіч");
    QTest::newRow("by acc male Сава") << QStringLiteral("Скулаўца Саву Васільевіча") << LangBY << Accusative
                                      << QStringLiteral("Скулавец Сава Васільевіч");
    QTest::newRow("by acc chinese У") << QStringLiteral("У Хунбіня") << LangBY << Accusative
                                      << QStringLiteral("У Хунбінь");
    QTest::newRow("by acc japanese Мінору") << QStringLiteral("Мінору Камату") << LangBY << Accusative
                                            << QStringLiteral("Мінору Камата");
    QTest::newRow("by acc Хізры") << QStringLiteral("Асадулаева Хізру Газіевіча") << LangBY << Accusative
                                  << QStringLiteral("Асадулаеў Хізры Газіевіч");
    QTest::newRow("ru acc male Никита") << QStringLiteral("Петрова Никиту Ивановича") << LangRU << Accusative
                                        << QStringLiteral("Петров Никита Иванович");
    QTest::newRow("by acc female -ава")<< QStringLiteral("Багданаву Таццяну Аляксандраўну") << LangBY << Accusative
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
    // беглые гласные и другие окончания, проверенные по всему архиву указов (decree_report)
    QTest::newRow("by acc -шка") << QStringLiteral("Мяшка Андрэя Іванавіча") << LangBY << Accusative
                                 << QStringLiteral("Мяшок Андрэй Іванавіч");
    QTest::newRow("by acc -лька") << QStringLiteral("Каралька Сяргея Іванавіча") << LangBY << Accusative
                                  << QStringLiteral("Каралёк Сяргей Іванавіч");
    QTest::newRow("by acc -ўца") << QStringLiteral("Сакаўца Юрыя Іванавіча") << LangBY << Accusative
                                 << QStringLiteral("Сакавец Юрый Іванавіч");
    QTest::newRow("by dat -ўцу") << QStringLiteral("Паўлаўцу Юрыю Іванавічу") << LangBY << Dative
                                 << QStringLiteral("Паўлавец Юрый Іванавіч");
    QTest::newRow("by acc -рца") << QStringLiteral("Азярца Юрыя Іванавіча") << LangBY << Accusative
                                 << QStringLiteral("Азярэц Юрый Іванавіч");
    QTest::newRow("ru dat -рцу") << QStringLiteral("Перцу Ивану Петровичу") << LangRU << Dative
                                 << QStringLiteral("Перец Иван Петрович");
    // «е» стоит первым, но настоящие окончания — «це» и «же»
    QTest::newRow("ru dat longer suffix Федуце") << QStringLiteral("Федуце Ивану Петровичу") << LangRU << Dative
                                                 << QStringLiteral("Федута Иван Петрович");
    QTest::newRow("ru dat longer suffix Серёже") << QStringLiteral("Сидорову Серёже Ивановичу") << LangRU << Dative
                                                 << QStringLiteral("Сидоров Серёжа Иванович");
    QTest::newRow("by acc Заяц") << QStringLiteral("Зайца Юрыя Іванавіча") << LangBY << Accusative
                                 << QStringLiteral("Заяц Юрый Іванавіч");
    QTest::newRow("by acc Корж") << QStringLiteral("Каржа Івана Аляксеевіча") << LangBY << Accusative
                                 << QStringLiteral("Корж Іван Аляксеевіч");
    QTest::newRow("by dat Корж") << QStringLiteral("Каржу Івану Аляксеевічу") << LangBY << Dative
                                 << QStringLiteral("Корж Іван Аляксеевіч");
    QTest::newRow("by acc -гага") << QStringLiteral("Доўгага Пятра Васільевіча") << LangBY << Accusative
                                  << QStringLiteral("Доўгі Пётр Васільевіч");
    QTest::newRow("by acc -няга") << QStringLiteral("Загародняга Анатолія Мікалаевіча") << LangBY << Accusative
                                  << QStringLiteral("Загародні Анатолій Мікалаевіч");
    QTest::newRow("by acc -яя") << QStringLiteral("Біцяя Анатолія Мікалаевіча") << LangBY << Accusative
                                << QStringLiteral("Біцяй Анатолій Мікалаевіч");
    QTest::newRow("by dat -ру") << QStringLiteral("Косару Івану Пятровічу") << LangBY << Dative
                                << QStringLiteral("Косар Іван Пятровіч");
    QTest::newRow("by dat female -іной") << QStringLiteral("Кузьміной Алене Іванаўне") << LangBY << Dative
                                         << QStringLiteral("Кузьміна Алена Іванаўна");
    QTest::newRow("by dat Надзеі (Latin i)") << QStringLiteral("Петрусь Надзеi Іванаўне") << LangBY << Dative
                                             << QStringLiteral("Петрусь Надзея Іванаўна");
    QTest::newRow("by dat Алесі") << QStringLiteral("Петрусь Алесі Іванаўне") << LangBY << Dative
                                  << QStringLiteral("Петрусь Алеся Іванаўна");
    QTest::newRow("ru dat Марии") << QStringLiteral("Петровой Марии Ивановне") << LangRU << Dative
                                  << QStringLiteral("Петрова Мария Ивановна");
    QTest::newRow("by dat Роберт") << QStringLiteral("Іванову Роберту Сяргеевічу") << LangBY << Dative
                                   << QStringLiteral("Іваноў Роберт Сяргеевіч");
    QTest::newRow("by acc double surname, female") << QStringLiteral("Карпінскую-Пешка Ганну Іванаўну") << LangBY << Accusative
                                                   << QStringLiteral("Карпінская-Пешка Ганна Іванаўна");
    QTest::newRow("by acc double surname, male") << QStringLiteral("Чумака-Жуня Міхаіла Яўгенавіча") << LangBY << Accusative
                                                 << QStringLiteral("Чумак-Жунь Міхаіл Яўгенавіч");
    QTest::newRow("by acc double patronymic") << QStringLiteral("Петрайціса Юозаса Гедзімінасавіча-Альбертасавіча") << LangBY << Accusative
                                              << QStringLiteral("Петрайціс Юозас Гедзімінасавіч-Альбертасавіч");
    QTest::newRow("ru dat Воробей") << QStringLiteral("Воробей Анне Ивановне") << LangRU << Dative
                                    << QStringLiteral("Воробей Анна Ивановна");
    // латинская «i» вместо белорусской «і» по дороге исправляется
    QTest::newRow("by latin i") << QStringLiteral("Лагодзiча Юрыя Іванавiча") << LangBY << Accusative
                                << QStringLiteral("Лагодзіч Юрый Іванавіч");
    QTest::newRow("by latin capital I") << QStringLiteral("Лагодзiча Юрыя Iванавiча") << LangBY << Accusative
                                        << QStringLiteral("Лагодзіч Юрый Іванавіч");

    QTest::newRow("ru dat Окружная") << QStringLiteral("Окружной Светлане Артемовне") << LangRU << Dative
                                     << QStringLiteral("Окружная Светлана Артемовна");
    QTest::newRow("ru dat Лесечко") << QStringLiteral("Лесечку Ивану Васильевичу") << LangRU << Dative
                                    << QStringLiteral("Лесечко Иван Васильевич");
    QTest::newRow("ru dat Ханок") << QStringLiteral("Ханку Эдуарду Семеновичу") << LangRU << Dative
                                  << QStringLiteral("Ханок Эдуард Семенович");
    QTest::newRow("ru dat Гребень") << QStringLiteral("Гребню Александру Владимировичу") << LangRU << Dative
                                    << QStringLiteral("Гребень Александр Владимирович");
    QTest::newRow("ru dat Волчок") << QStringLiteral("Волчку Александру Титовичу") << LangRU << Dative
                                   << QStringLiteral("Волчок Александр Титович");
    QTest::newRow("ru acc Волчк") << QStringLiteral("Волчка Владимира Владимировича") << LangRU << Accusative
                                  << QStringLiteral("Волчк Владимир Владимирович");
    QTest::newRow("ru dat Брауде") << QStringLiteral("Брауде Дине Марковне") << LangRU << Dative
                                   << QStringLiteral("Брауде Дина Марковна");
    QTest::newRow("ru dat Заец") << QStringLiteral("Зайцу Павлу Исаковичу") << LangRU << Dative
                                 << QStringLiteral("Заец Павел Исакович");
    QTest::newRow("ru dat Заец any name") << QStringLiteral("Зайцу Николаю Емельяновичу") << LangRU << Dative
                                          << QStringLiteral("Заец Николай Емельянович");
    QTest::newRow("ru dat Ермоленко") << QStringLiteral("Ермоленко Анатолию Ивановичу") << LangRU << Dative
                                      << QStringLiteral("Ермоленко Анатолий Иванович");
    QTest::newRow("ru acc Филарет") << QStringLiteral("Филарета (Вахромеева Кирилла Варфоломеевича)") << LangRU << Accusative
                                    << QStringLiteral("Филарет (Вахромеев Кирилл Варфоломеевич)");
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
    QCOMPARE(QString::fromStdWString(name), expected);
}

// иностранцы: «Джону Смиту» (сначала имя), порядок сохраняется
void TestNameConverter::foreignNameWithoutPatronymic()
{
    CNameConverter converter;
    std::wstring name = L"Джону Смиту";
    QVERIFY(converter.NormalizeName(name, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Джон Смит"));
}

// «-ому» может быть «-ый» или «-ой»: результат — догадка, и так и сообщается
void TestNameConverter::uncertainSurnameIsReported()
{
    CNameConverter converter;
    std::wstring name = L"Толстому Ивану Петровичу";
    QVERIFY(!converter.NormalizeName(name, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Толстый Иван Петрович"));
}

// «оглы» — не отчество, которое знают правила; имя остаётся на проверку
void TestNameConverter::unknownPatronymicIsReported()
{
    CNameConverter converter;
    std::wstring name = L"Иванову Ивану оглы";
    QVERIFY(!converter.NormalizeName(name, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Иванов Иван оглы"));
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
    std::wstring femaleDatUpper = L"НИКОЛАЕВНЕ";
    std::wstring femaleUpper = L"НИКОЛАЕВНУ";
    std::wstring male = L"Николаевича";
    QCOMPARE(converter.GetSex(female, Accusative), SexFemale);
    QCOMPARE(converter.GetSex(femaleDat, Dative), SexFemale);
    QCOMPARE(converter.GetSex(femaleDatUpper, Dative), SexFemale);
    QCOMPARE(converter.GetSex(femaleUpper, Accusative), SexFemale);
    QCOMPARE(converter.GetSex(male, Accusative), SexMale);
}

QTEST_GUILESS_MAIN(TestNameConverter)
#include "tst_nameconverter.moc"
