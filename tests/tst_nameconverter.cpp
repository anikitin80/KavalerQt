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
    void catalogPicksTheSurname();
    void catalogSurnameVariants_data();
    void catalogSurnameVariants();
    void twoKnownSpellingsAreUnsure();
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
    // дательный «Нікіту» — Нікіта, но «Марату» — Марат
    QTest::newRow("by dat male Нікіта") << QStringLiteral("Уколаву Нікіту Міхайлавічу") << LangBY << Dative
                                        << QStringLiteral("Уколаў Нікіта Міхайлавіч");
    QTest::newRow("by dat male Марат") << QStringLiteral("Грыгорчыку Марату Казіміравічу") << LangBY << Dative
                                       << QStringLiteral("Грыгорчык Марат Казіміравіч");
    QTest::newRow("by acc male Мікіта") << QStringLiteral("Лесняка Мікіту Мікалаевіча") << LangBY << Accusative
                                        << QStringLiteral("Лесняк Мікіта Мікалаевіч");
    QTest::newRow("by dat Алі") << QStringLiteral("Шабанаву Алі Шабанавічу") << LangBY << Dative
                                << QStringLiteral("Шабанаў Алі Шабанавіч");
    QTest::newRow("ru dat Любовь") << QStringLiteral("Беловой Любови Леонидовне") << LangRU << Dative
                                   << QStringLiteral("Белова Любовь Леонидовна");
    QTest::newRow("ru dat Майя") << QStringLiteral("Айдаровой Майе Васильевне") << LangRU << Dative
                                 << QStringLiteral("Айдарова Майя Васильевна");
    QTest::newRow("by acc female -юю") << QStringLiteral("Паследнюю Тамару Іванаўну") << LangBY << Accusative
                                       << QStringLiteral("Паследняя Тамара Іванаўна");
    // фамилия-прилагательное без отчества стоит первой и даёт пол
    QTest::newRow("by acc adjective male") << QStringLiteral("Сычэўскага Яна") << LangBY << Accusative
                                           << QStringLiteral("Сычэўскі Ян");
    QTest::newRow("by acc adjective female") << QStringLiteral("Быкоўскую Алену") << LangBY << Accusative
                                             << QStringLiteral("Быкоўская Алена");
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
    bool surnameSure = true;
    QVERIFY(!converter.NormalizeName(name, LangRU, Dative, &surnameSure));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Толстый Иван Петрович"));
    QVERIFY(!surnameSure);

    // фамилию правило знает точно, неразобранное отчество её не касается
    name = L"Иванову Ивану оглы";
    QVERIFY(!converter.NormalizeName(name, LangRU, Dative, &surnameSure));
    QVERIFY(surnameSure);

    // угаданную фамилию знает каталог: проверять её не нужно
    converter.SetKnownSurnames(LangRU, {{L"толстый", 1}});
    name = L"Толстому Ивану Петровичу";
    converter.NormalizeName(name, LangRU, Dative, &surnameSure);
    QVERIFY(surnameSure);
}

// «оглы» — не отчество, которое знают правила; имя остаётся на проверку
void TestNameConverter::unknownPatronymicIsReported()
{
    CNameConverter converter;
    std::wstring name = L"Иванову Ивану оглы";
    QVERIFY(!converter.NormalizeName(name, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Иванов Иван оглы"));

    // отчество с ошибкой: пол даёт фамилия-прилагательное
    name = L"Струнеўскую Святлану Міколу";
    QVERIFY(!converter.NormalizeName(name, LangBY, Accusative));
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Струнеўская Святлана Міколу"));

    // китайское имя без отчества: фамилия на «-оу» не склоняется
    name = L"Сюй Цайхоу";
    converter.NormalizeName(name, LangBY, Accusative);
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Сюй Цайхоу"));
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

// косвенная фамилия даёт несколько именительных: берётся известный каталогу,
// а фамилия правила, если каталог её знает, остаётся
void TestNameConverter::catalogPicksTheSurname()
{
    const auto convert = [](CNameConverter& converter, const QString& decree, LangType lang, NounCase nounCase) {
        std::wstring name = decree.toStdWString();
        converter.NormalizeName(name, lang, nounCase);
        return QString::fromStdWString(name);
    };
    CNameConverter rulesOnly;
    QCOMPARE(convert(rulesOnly, QStringLiteral("Склюту Іосіфу Абрамавічу"), LangBY, Dative),
             QStringLiteral("Склюта Іосіф Абрамавіч"));

    CNameConverter converter;
    converter.SetKnownSurnames(LangBY, {{L"склют", 2}, {L"масол", 4}, {L"колесень", 1}, {L"воўк", 3}, {L"ёрш", 1},
                                        {L"кухта", 1}, {L"кухт", 9}});
    converter.SetKnownSurnames(LangRU, {{L"лобанок", 2}});
    QCOMPARE(convert(converter, QStringLiteral("Склюту Іосіфу Абрамавічу"), LangBY, Dative),
             QStringLiteral("Склют Іосіф Абрамавіч"));
    QCOMPARE(convert(converter, QStringLiteral("Масла Леаніда Леанідавіча"), LangBY, Accusative),
             QStringLiteral("Масол Леанід Леанідавіч"));
    QCOMPARE(convert(converter, QStringLiteral("Колесня Уладзіміра Васільевіча"), LangBY, Accusative),
             QStringLiteral("Колесень Уладзімір Васільевіч"));
    QCOMPARE(convert(converter, QStringLiteral("Валка Аляксандра Аляксандравіча"), LangBY, Accusative),
             QStringLiteral("Воўк Аляксандр Аляксандравіч"));
    QCOMPARE(convert(converter, QStringLiteral("Ярша Алега Віктаравіча"), LangBY, Accusative),
             QStringLiteral("Ёрш Алег Віктаравіч"));
    QCOMPARE(convert(converter, QStringLiteral("Лобанку Александру Ивановичу"), LangRU, Dative),
             QStringLiteral("Лобанок Александр Иванович"));
    // «Кухта» каталог знает: более частый «Кухт» её не вытесняет
    QCOMPARE(convert(converter, QStringLiteral("Кухту Івану Іванавічу"), LangBY, Dative),
             QStringLiteral("Кухта Іван Іванавіч"));
    // неизвестная каталогу фамилия без известных вариантов остаётся по правилу
    QCOMPARE(convert(converter, QStringLiteral("Зайцаву Івану Іванавічу"), LangBY, Dative),
             convert(rulesOnly, QStringLiteral("Зайцаву Івану Іванавічу"), LangBY, Dative));
}

// формы, которые дают косвенные фамилии, если каталог их знает
void TestNameConverter::catalogSurnameVariants_data()
{
    QTest::addColumn<QString>("known");
    QTest::addColumn<QString>("decree");
    QTest::addColumn<LangType>("lang");
    QTest::addColumn<NounCase>("nounCase");
    QTest::addColumn<QString>("expected");

    QTest::newRow("dative -е") << QStringLiteral("мисюля") << QStringLiteral("Мисюле Евгению Николаевичу")
                               << LangRU << Dative << QStringLiteral("Мисюля Евгений Николаевич");
    QTest::newRow("adjective -ой") << QStringLiteral("кутовой") << QStringLiteral("Кутовому Борису Александровичу")
                                   << LangRU << Dative << QStringLiteral("Кутовой Борис Александрович");
    QTest::newRow("adjective -ого") << QStringLiteral("рябой") << QStringLiteral("Рябого Вячеслава Владимировича")
                                    << LangRU << Accusative << QStringLiteral("Рябой Вячеслав Владимирович");
    QTest::newRow("-ю to -я") << QStringLiteral("семяненя") << QStringLiteral("Семяненю Івану Георгіевічу")
                              << LangBY << Dative << QStringLiteral("Семяненя Іван Георгіевіч");
    QTest::newRow("-у to -ы") << QStringLiteral("харунжы") << QStringLiteral("Харунжу Аляксандра Аляксандравіча")
                              << LangBY << Accusative << QStringLiteral("Харунжы Аляксандр Аляксандравіч");
    QTest::newRow("т to ц") << QStringLiteral("кацел") << QStringLiteral("Катла Мікалая Яўгеньевіча")
                            << LangBY << Accusative << QStringLiteral("Кацел Мікалай Яўгеньевіч");
    QTest::newRow("ў to в") << QStringLiteral("жывень") << QStringLiteral("Жыўня Івана Адольфавіча")
                            << LangBY << Accusative << QStringLiteral("Жывень Іван Адольфавіч");
    QTest::newRow("я to е") << QStringLiteral("кузнец") << QStringLiteral("Кузняца Аляксандра Аляксандравіча")
                            << LangBY << Accusative << QStringLiteral("Кузнец Аляксандр Аляксандравіч");
    QTest::newRow("female indeclinable") << QStringLiteral("сухавей") << QStringLiteral("Сухавей Святлане Іванаўне")
                                         << LangBY << Dative << QStringLiteral("Сухавей Святлана Іванаўна");
    QTest::newRow("double surname") << QStringLiteral("окала-кулак") << QStringLiteral("Окала-Кулака Юрыя Міхайлавіча")
                                    << LangBY << Accusative << QStringLiteral("Окала-Кулак Юрый Міхайлавіч");
    QTest::newRow("double female") << QStringLiteral("палякова-макей") << QStringLiteral("Паляковай-Макей Веры Аляксандраўне")
                                   << LangBY << Dative << QStringLiteral("Палякова-Макей Вера Аляксандраўна");
}

void TestNameConverter::catalogSurnameVariants()
{
    QFETCH(QString, known);
    QFETCH(QString, decree);
    QFETCH(LangType, lang);
    QFETCH(NounCase, nounCase);
    QFETCH(QString, expected);

    CNameConverter converter;
    converter.SetKnownSurnames(lang, {{known.toStdWString(), 1}});
    std::wstring name = decree.toStdWString();
    bool surnameSure = false;
    converter.NormalizeName(name, lang, nounCase, &surnameSure);
    QCOMPARE(QString::fromStdWString(name), expected);
    QVERIFY(surnameSure);
}

// каталог знает два написания — выбрать по указу нельзя, фамилию проверяет пользователь;
// написания другого пола не мешают
void TestNameConverter::twoKnownSpellingsAreUnsure()
{
    CNameConverter converter;
    converter.SetKnownSurnames(LangBY, {{L"заяц", 45}, {L"заец", 4}, {L"іванова", 9}, {L"іваноў", 9}});
    converter.SetKnownSurnames(LangBY, {{L"заяц", 40}, {L"заец", 4}, {L"іваноў", 9}}, SexMale);
    converter.SetKnownSurnames(LangBY, {{L"заяц", 5}, {L"іванова", 9}}, SexFemale);

    std::wstring name = L"Зайца Аляксандра Аляксандравіча";
    bool surnameSure = true;
    SurnameSpellings spellings;
    converter.NormalizeName(name, LangBY, Accusative, &surnameSure, &spellings);
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Заяц Аляксандр Аляксандравіч"));
    QVERIFY(!surnameSure);
    // для подсказки окна импорта: оба написания, частое первым
    QCOMPARE(spellings, SurnameSpellings({{L"Заяц", 40}, {L"Заец", 4}}));

    // «Іванова» — фамилия женщин, у мужчины «Іванову» одно известное написание
    name = L"Іванову Івану Іванавічу";
    converter.NormalizeName(name, LangBY, Dative, &surnameSure);
    QCOMPARE(QString::fromStdWString(name), QStringLiteral("Іваноў Іван Іванавіч"));
    QVERIFY(surnameSure);
}

QTEST_GUILESS_MAIN(TestNameConverter)
#include "tst_nameconverter.moc"
