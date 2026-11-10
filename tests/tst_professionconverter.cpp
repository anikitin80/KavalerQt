// CProfessionConverter: должность перед именем («водителю автобуса»,
// «старшего инспектора отдела») обратно в именительный по словарям
// должностей (тесты берут копии из tests/data).

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
    void severalFormsSurviveSaving();
    void otherCaseSuppliesTheNominative();
    void dictionaryKeysMatchLookup();
    void adjectiveChainIsNotANewWord();
    void neuterGenitiveIsNotAProfession();
    void secondJobAfterUnknownFirst();
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
    QTest::newRow("ru dat -кому adjective") << QStringLiteral("техническому директору общества") << LangRU << Dative
                                            << QStringLiteral("технический директор общества");
    QTest::newRow("ru acc -кого adjective") << QStringLiteral("коммерческого директора предприятия") << LangRU << Accusative
                                            << QStringLiteral("коммерческий директор предприятия");
    QTest::newRow("ru dat -шей adjective") << QStringLiteral("старшей медицинской сестре больницы") << LangRU << Dative
                                           << QStringLiteral("старшая медицинская сестра больницы");
    QTest::newRow("ru acc stressed -ой from dictionary") << QStringLiteral("бурового мастера управления") << LangRU << Accusative
                                                         << QStringLiteral("буровой мастер управления");
    QTest::newRow("by acc -гога adjective") << QStringLiteral("другога пілота авіяцыйнай эскадрыллі") << LangBY << Accusative
                                            << QStringLiteral("другі пілот авіяцыйнай эскадрыллі");
    QTest::newRow("by acc fleeting vowel after hyphen") << QStringLiteral("лектара-музыказнаўца аркестра") << LangBY << Accusative
                                                        << QStringLiteral("лектар-музыказнавец аркестра");
    QTest::newRow("by acc student keeps faculty") << QStringLiteral("студэнта трэцяга курса механіка-матэматычнага факультэта") << LangBY << Accusative
                                                  << QStringLiteral("студэнт трэцяга курса механіка-матэматычнага факультэта");
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
    QTest::newRow("by acc profession after і") << QStringLiteral("акцёра кіно, каскадзёра і пастаноўшчыка трукаў") << LangBY << Accusative
                                               << QStringLiteral("акцёр кіно, каскадзёра і пастаноўшчык трукаў");
    QTest::newRow("by dat былому") << QStringLiteral("былому галоўнаму юрысконсульту ўстановы") << LangBY << Dative
                                   << QStringLiteral("былы галоўны юрысконсульт ўстановы");
    QTest::newRow("by acc хатнюю") << QStringLiteral("хатнюю гаспадыню вёскі") << LangBY << Accusative
                                   << QStringLiteral("хатняя гаспадыня вёскі");
    QTest::newRow("ru dat старому") << QStringLiteral("старому слесарю цеха") << LangRU << Dative
                                    << QStringLiteral("старый слесарь цеха");
    // повторный пробел остаётся, когда прилагательное становится короче
    QTest::newRow("ru dat double space") << QStringLiteral("старому  слесарю цеха") << LangRU << Dative
                                         << QStringLiteral("старый  слесарь цеха");
    // дефис принадлежит прилагательному, должность после него назад не затягивается
    QTest::newRow("by dat hyphenated adjective") << QStringLiteral("вялікаму-новаму інспектару цэха") << LangBY << Dative
                                                 << QStringLiteral("вялікі-новы інспектар цэха");
    QTest::newRow("by dat en dash") << QStringLiteral("вялікаму\u2013інспектару цэха") << LangBY << Dative
                                    << QStringLiteral("вялікі\u2013інспектар цэха");
    // должности нет в словаре: именительный всё равно восстанавливается
    QTest::newRow("ru acc unknown historian") << QStringLiteral("историка, член союза") << LangRU << Accusative
                                              << QStringLiteral("историк, член союза");
    QTest::newRow("ru dat unknown musicologist") << QStringLiteral("музыковеду театра") << LangRU << Dative
                                                 << QStringLiteral("музыковед театра");
    QTest::newRow("ru dat scientific locative") << QStringLiteral("старший научном сотруднику института") << LangRU << Dative
                                                << QStringLiteral("старший научный сотрудник института");
    QTest::newRow("ru acc unknown hyphenated") << QStringLiteral("советника-консультант отдела") << LangRU << Accusative
                                               << QStringLiteral("советник-консультант отдела");
    QTest::newRow("by acc unknown head and adjective") << QStringLiteral("старшага дэпартамента ўстановы") << LangBY << Accusative
                                                       << QStringLiteral("старшы дэпартамент ўстановы");
    QTest::newRow("ru dat scientific dative") << QStringLiteral("ведущему научному сотруднику института") << LangRU << Dative
                                              << QStringLiteral("ведущий научный сотрудник института");
    QTest::newRow("ru acc firefighter") << QStringLiteral("старшего пожарного пожарной части") << LangRU << Accusative
                                        << QStringLiteral("старший пожарный пожарной части");
    // «па» не занимает место среди первых слов, «следчага» находится
    QTest::newRow("by acc senior investigator") << QStringLiteral("старшага па корпусу дзяжурнай службы следчага ізалятара") << LangBY << Accusative
                                                << QStringLiteral("старшы па корпусу дзяжурнай службы следчы ізалятара");
    QTest::newRow("by acc air radio operator") << QStringLiteral("старшага паветранага радыста эскадрыллі") << LangBY << Accusative
                                               << QStringLiteral("старшы паветраны радыст эскадрыллі");
    QTest::newRow("by acc state secretary") << QStringLiteral("начальніка галоўнага ўпраўлення кадраў (статс-сакратара)") << LangBY << Accusative
                                            << QStringLiteral("начальнік галоўнага ўпраўлення кадраў (статс-сакратар)");
    QTest::newRow("by acc state secretary already nominative") << QStringLiteral("начальнік ўпраўлення ідэалагічнай работы і кадравага забеспячэння (статс-сакратара)") << LangBY << Accusative
                                                               << QStringLiteral("начальнік ўпраўлення ідэалагічнай работы і кадравага забеспячэння (статс-сакратар)");
    QTest::newRow("by acc senior on duty") << QStringLiteral("старшага па корпусу дзяжурнай службы турмы") << LangBY << Accusative
                                           << QStringLiteral("старшы па корпусу дзяжурнай службы турмы");
    QTest::newRow("by acc psychologist") << QStringLiteral("начальніка службы сацыяльнай работы (псіхолага)") << LangBY << Accusative
                                         << QStringLiteral("начальнік службы сацыяльнай работы (псіхолаг)");
    // вторая должность: через «і» без запятой, после «цяпер», в далёкой скобке
    QTest::newRow("by dat second job after і") << QStringLiteral("мастацкаму кіраўніку і галоўнаму дырыжору аркестра") << LangBY << Dative
                                               << QStringLiteral("мастацкі кіраўнік і галоўны дырыжор аркестра");
    QTest::newRow("by dat second job after цяпер") << QStringLiteral("дырэктару завода, цяпер намесніку міністра") << LangBY << Dative
                                                   << QStringLiteral("дырэктар завода, цяпер намеснік міністра");
    // одно слово в далёкой скобке каталог держит как в указе
    QTest::newRow("by acc single-word bracket stays") << QStringLiteral("старшага тэхніка інжынерна-тэхнічных сродкаў аховы (начальніка)") << LangBY << Accusative
                                                      << QStringLiteral("старшы тэхнік інжынерна-тэхнічных сродкаў аховы (начальніка)");
    QTest::newRow("by dat job in a far bracket") << QStringLiteral("урачу хуткай медыцынскай дапамогі (загадчыку падстанцыі)") << LangBY << Dative
                                                 << QStringLiteral("урач хуткай медыцынскай дапамогі (загадчык падстанцыі)");
    // звание и учёная степень перед именем без запятой: каталог отделяет их запятой
    QTest::newRow("ru dat trailing professor") << QStringLiteral("декану физического факультета университета профессору") << LangRU << Dative
                                               << QStringLiteral("декан физического факультета университета, профессор");
    QTest::newRow("ru acc trailing academician") << QStringLiteral("директора института академии наук академика") << LangRU << Accusative
                                                 << QStringLiteral("директор института академии наук, академик");
    QTest::newRow("by dat trailing degree") << QStringLiteral("дырэктару інстытута доктару тэхнічных навук") << LangBY << Dative
                                            << QStringLiteral("дырэктар інстытута, доктар тэхнічных навук");
    QTest::newRow("by dat leading degree") << QStringLiteral("доктару тэхнічных навук прарэктару па вучэбнай рабоце") << LangBY << Dative
                                           << QStringLiteral("доктар тэхнічных навук, прарэктар па вучэбнай рабоце");
    QTest::newRow("by dat title after a comma stays") << QStringLiteral("загадчыку кафедры, прафесару") << LangBY << Dative
                                                      << QStringLiteral("загадчык кафедры, прафесар");
}

// первая должность незнакома, вторая после запятой всё равно приводится
void TestProfessionConverter::secondJobAfterUnknownFirst()
{
    CProfessionConverter converter;
    TestData::useTestDictionaries(converter);
    std::wstring text = L"работніцы закрытага акцыянернага таварыства «Атлант», майстру спорту міжнароднага класа";
    converter.ConvertProfession(text, LangBY, Dative);
    QVERIFY2(QString::fromStdWString(text).endsWith(QStringLiteral("«Атлант», майстар спорту міжнароднага класа")),
             qPrintable(QString::fromStdWString(text)));
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
    // первое слово не должность: «водителю» дальше пяти слов не ищется
    std::wstring text = L"проживает в г. Минске по месту жительства водителю";
    QVERIFY(!converter.ConvertProfession(text, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("проживает в г. Минске по месту жительства водителю"));
}

// незнакомая должность остаётся как есть и предлагается в словарь
void TestProfessionConverter::unknownProfessionBecomesNewWord()
{
    CProfessionConverter converter;
    TestData::useTestDictionaries(converter);
    std::wstring text = L"сварщику цеха";
    QVERIFY(converter.ConvertProfession(text, LangRU, Dative));
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("сварщик цеха"));

    const auto& words = converter.newWords();
    QCOMPARE(int(words.size()), 1);
    QCOMPARE(QString::fromStdWString(words.front().Original), QStringLiteral("сварщику"));
    QCOMPARE(QString::fromStdWString(words.front().Word), QStringLiteral("сварщик"));
    QCOMPARE(words.front().Language, LangRU);
    QCOMPARE(words.front().Case, Dative);
}

// ключи словаря ищутся в нижнем регистре, поэтому слово с заглавной
// надо хранить строчными, чтобы найти его в следующий раз
void TestProfessionConverter::newWordsAreLowerCase()
{
    CProfessionConverter converter; // без файлов: UpdateDictionary только заполняет память
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
    CProfessionConverter converter; // путей словарей нет совсем
    std::wstring text = L"сварщику цеха";
    converter.ConvertProfession(text, LangRU, Dative);
    std::vector<NewDictItem> items = converter.newWords();
    QCOMPARE(int(items.size()), 1);

    QVERIFY(!converter.UpdateDictionary(items)); // сохранять некуда
    std::wstring again = L"сварщику участка";
    QVERIFY(converter.ConvertProfession(again, LangRU, Dative)); // но в этом сеансе уже годится
    QCOMPARE(QString::fromStdWString(again), QStringLiteral("сварщик участка"));
}

// битая строка (пустой именительный или форма) пропускается и заменой не служит
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

// несколько форм одного слова («суддзі» и «суддзям», «артыста» и «артыстаў») переживают сохранение
void TestProfessionConverter::severalFormsSurviveSaving()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("PositionsBY.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QStringLiteral("суддзя;суддзі;суддзю\nсуддзя;суддзям;\nартыст;артысту;артыста\nартыст;;артыстаў\n").toUtf8());
    file.close();

    CProfessionConverter writer;
    QVERIFY(writer.SetDictionaryPath(path.toStdWString(), LangBY));
    std::vector<NewDictItem> items{{L"слесар", L"слесару", LangBY, Dative, true}};
    QVERIFY(writer.UpdateDictionary(items)); // сохранение переписывает файл

    CProfessionConverter reader;
    QVERIFY(reader.SetDictionaryPath(path.toStdWString(), LangBY));
    const struct { const wchar_t* form; NounCase nounCase; const wchar_t* nominative; } forms[] = {
        {L"суддзі", Dative, L"суддзя"}, {L"суддзям", Dative, L"суддзя"}, {L"суддзю", Accusative, L"суддзя"},
        {L"артыста", Accusative, L"артыст"}, {L"артыстаў", Accusative, L"артыст"}, {L"слесару", Dative, L"слесар"}};
    for (const auto& form : forms) {
        std::wstring text = std::wstring(form.form) + L" цэха";
        QVERIFY2(reader.ConvertProfession(text, LangBY, form.nounCase), qPrintable(QString::fromWCharArray(form.form)));
        QCOMPARE(QString::fromStdWString(text), QString::fromWCharArray(form.nominative) + QStringLiteral(" цэха"));
    }
}

// недостающий падеж восстанавливается из именительного, уже лежащего в другом падеже
void TestProfessionConverter::otherCaseSuppliesTheNominative()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("PositionsBY.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QStringLiteral("абутнік;;абутніка\n").toUtf8());
    file.close();

    CProfessionConverter converter;
    QVERIFY(converter.SetDictionaryPath(path.toStdWString(), LangBY));

    std::wstring dative = L"абутніку цэха";
    QVERIFY(converter.ConvertProfession(dative, LangBY, Dative));
    QCOMPARE(QString::fromStdWString(dative), QStringLiteral("абутнік цэха"));
    QVERIFY(converter.newWords().empty());

    std::wstring capital = L"Абутніку цэха";
    QVERIFY(converter.ConvertProfession(capital, LangBY, Dative));
    QCOMPARE(QString::fromStdWString(capital), QStringLiteral("Абутнік цэха"));
    QVERIFY(converter.newWords().empty());

    std::wstring accusative = L"абутніка цэха";
    QVERIFY(converter.ConvertProfession(accusative, LangBY, Accusative));
    QCOMPARE(QString::fromStdWString(accusative), QStringLiteral("абутнік цэха"));
    QVERIFY(converter.newWords().empty());

    std::wstring unknown = L"зваршчыку цэха";
    QVERIFY(converter.ConvertProfession(unknown, LangBY, Dative));
    QCOMPARE(QString::fromStdWString(unknown), QStringLiteral("зваршчык цэха"));
    QCOMPARE(int(converter.newWords().size()), 1);
    QCOMPARE(QString::fromStdWString(converter.newWords().front().Original), QStringLiteral("зваршчыку"));
    QCOMPARE(QString::fromStdWString(converter.newWords().front().Word), QStringLiteral("зваршчык"));
}

// «Вадзiцель» и «вадзіцель» — одно словарное слово: нижний регистр, кириллическая «і»
void TestProfessionConverter::dictionaryKeysMatchLookup()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("PositionsBY.csv"));
    const QString canonical = QStringLiteral("вадзіцель;вадзіцелю;вадзіцеля");
    QString legacy = canonical.toUpper();
    legacy.replace(QChar(0x0406), QLatin1Char('I'));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(legacy.toUtf8());
    file.write("\n", 1);
    file.write(canonical.toUtf8());
    file.write("\n", 1);
    file.close();

    CProfessionConverter converter;
    QVERIFY(converter.SetDictionaryPath(path.toStdWString(), LangBY));
    std::wstring text = QStringLiteral("вадзіцелю цэха").toStdWString();
    QVERIFY(converter.ConvertProfession(text, LangBY, Dative));
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("вадзіцель цэха"));

    QVERIFY(converter.SaveDictionary(path.toStdWString(), LangBY));
    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QString body = QString::fromUtf8(saved.readAll());
    saved.close();
    if (body.startsWith(QChar(0xFEFF)))
        body.remove(0, 1);
    QStringList lines;
    for (const QString& line : body.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            lines << trimmed;
    }
    QCOMPARE(lines, QStringList{ canonical });

    CProfessionConverter again;
    QVERIFY(again.SetDictionaryPath(path.toStdWString(), LangBY));
    std::wstring onceMore = QStringLiteral("вадзіцелю цэха").toStdWString();
    QVERIFY(again.ConvertProfession(onceMore, LangBY, Dative));
    QCOMPARE(QString::fromStdWString(onceMore), QStringLiteral("вадзіцель цэха"));
}

// пропускается каждое начальное прилагательное, не только первое
void TestProfessionConverter::adjectiveChainIsNotANewWord()
{
    CProfessionConverter converter;
    TestData::useTestDictionaries(converter);
    std::wstring text = L"вялікаму сіняму новаму брынзалу";
    QVERIFY(converter.ConvertProfession(text, LangBY, Dative));
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("вялікі сіняму новы брынзал"));
    QCOMPARE(int(converter.newWords().size()), 1);
    QCOMPARE(QString::fromStdWString(converter.newWords().front().Original), QStringLiteral("брынзалу"));
    QCOMPARE(QString::fromStdWString(converter.newWords().front().Word), QStringLiteral("брынзал"));
}

// «падраздзялення» — не должность: родительный среднего рода остаётся
void TestProfessionConverter::neuterGenitiveIsNotAProfession()
{
    CProfessionConverter converter;
    TestData::useTestDictionaries(converter);
    std::wstring text = L"адасобленага падраздзялення";
    QVERIFY(!converter.ConvertProfession(text, LangBY, Accusative));
    QCOMPARE(QString::fromStdWString(text), QStringLiteral("адасобленага падраздзялення"));
    QCOMPARE(int(converter.newWords().size()), 0);
}

QTEST_GUILESS_MAIN(TestProfessionConverter)
#include "tst_professionconverter.moc"
