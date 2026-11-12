#pragma once

#include <QDate>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

constexpr int kLangRussian = 0x19;
constexpr int kLangBelarusian = 0x23;

enum class TextField
{
    NameRu = 0,
    NameBy,
    PersonalRu,
    PersonalBy
};

struct Award
{
    int id = 0;
    int type = 0;
    int priority = 0;
    QString name;
    QString nameBy;
};

struct Person
{
    int id = 0;
    int awardId = 0;
    QString name;
    QString nameBy;
    QString personal;
    QString personalBy;
    QString awardedFor;
    QString rank;
    QString rankBy;
    int ukaseNo = 0;
    QDate ukaseDate;
    int posthumous = 0;
    int language = kLangRussian;

    bool needTranslation() const;
    QString* field(TextField which);
    const QString* field(TextField which) const;
};

struct StatMonth
{
    int awardId = 0;
    int year = 0;
    int total = 0;
    int month[12] = {};
};

struct DictEntry
{
    QString type;
    QString by;
    QString ru;
    QString alt;
};

class Catalog : public QObject
{
    Q_OBJECT
public:
    explicit Catalog(QObject* parent = nullptr);
    ~Catalog() override;

    // readOnly: для отчётов и утилит; любое сохранение тогда не удаётся
    bool open(const QString& path, QString* error, bool readOnly = false);
    void close();
    bool isOpen() const;
    QString path() const { return m_path; }

    const QVector<Award>& awards() const { return m_awards; }
    const QVector<Person>& people() const { return m_people; }
    const Award* award(int id) const;
    // порядок наград везде: приоритет, затем имя, затем номер (<0, 0, >0)
    int compareAwards(int left, int right) const;
    // запись с этим номером, nullptr если её нет; годна до следующего изменения
    const Person* person(int id) const;

    bool save(Person& person, QString* error);
    bool saveMany(QVector<Person>& people, QString* error);
    // записи одного человека: номер и дата указа, награда и имя (русское
    // или белорусское). count() > 1 — тёзки в одном указе.
    int count(const Person& probe) const;
    bool contains(const Person& probe) const { return count(probe) > 0; }
    bool remove(const QList<int>& ids, QString* error);

    // Русское написание белорусского полного имени, уже лежащего в каталоге.
    // Одна запись — это имя. Несколько — написания именно этого
    // имени, самое частое первым: части других людей не подмешиваются.
    // Неизвестное имя из трёх частей составляется, только когда у каждой части одно написание.
    // Ключ схлопывает пробелы и заменяет латинские двойники, как личность
    // («Iгар» и «Ігар» — одно имя).
    // русские написания белорусского ФИО, лучшее первым: целое имя из каталога
    // или составленное из частей. Больше одного — выбор подтверждает пользователь.
    QStringList translateName(const QString& nameBy) const;
    static constexpr int kNameVariantsPerPart = 4;
    static constexpr int kNameVariants = 10;
    // одно белорусское полное имя, сохранённое больше чем с одним русским написанием.
    // Пробелы схлопнуты, так что отличаются только буквы. Пустые имена пропускаются.
    struct NameVariant
    {
        QString name;
        int count = 0;
    };
    struct NameConflict
    {
        QString nameBy;
        int rows = 0;
        QVector<NameVariant> names; // самое частое первым
    };
    QVector<NameConflict> russianNameConflicts() const;
    // самое частое русское написание; при равенстве остаётся самый ранний указ
    QString professionRu(const QString& professionBy) const;
    // самое частое русское написание; при равенстве остаётся первое по русскому имени
    QString rankRu(const QString& rankBy) const;
    // Выравнивает тире и кавычки в русском тексте должности с белорусским.
    // Вокруг длинного тире ставятся пробелы; у « » пропадают пробелы, которые
    // переводчик ставит внутри. Прямые и фигурные кавычки считаются теми же знаками.
    static bool fixQuotes(Person& person);

    // Латинские буквы, набранные внутри кириллических слов («Міxaйлаўна», «прядильщицa»,
    // «Iгар»; см. NormalizeCyrillicText). save()/saveMany() всегда пишут
    // исправленный текст; это находит записи, где они ещё остались.
    struct LetterFix
    {
        int records = 0;
        QMap<QString, int> fields; // подпись поля → записи
        QStringList samples;       // «было → стало»
    };
    LetterFix findLatinLetters(QVector<Person>* fixed = nullptr, int maxSamples = 8) const;
    static bool normalizeTextFields(Person& person);
    // цельная копия открытой базы (VACUUM INTO)
    bool backup(const QString& filePath, QString* error) const;

    int exportCsv(const QString& filePath, bool fromDate, const QDate& from, QString* error);
    // записи, у которых поле изменится (точное: всё поле равно find)
    int countMatches(TextField field, bool exact, const QString& find) const;
    int replaceText(TextField field, bool exact, const QString& find, const QString& replace, QString* error);

    // Производные данные (статистика, словарь имён, переводы должностей и званий)
    // пересобираются при первом обращении после изменения. Эти ссылки нельзя
    // держать через изменение каталога: копируйте то, что показываете.
    const QVector<StatMonth>& stats() const;
    const QVector<DictEntry>& dictionary() const;
    int yearMin() const { return 1990; }
    int yearMax() const;
    // год последнего указа (0 у пустого каталога), без статистики
    int latestYear() const;

signals:
    // испускается после изменения базы, прямо перед сменой данных в памяти:
    // people() ещё содержит старое (виды сохраняют состояние здесь)
    void aboutToChange();
    void dataChanged();

private:
    // человека узнаём по номеру указа, дате, награде и имени;
    // награда важна: один указ может дать человеку орден и звание.
    // Имена сравниваются после NormalizeCyrillicText («Iгар» == «Ігар»).
    struct IdentityKey
    {
        int ukaseNo = 0;
        qint64 day = 0;
        int awardId = 0;
        QString name;

        bool operator==(const IdentityKey& other) const
        {
            return ukaseNo == other.ukaseNo && day == other.day && awardId == other.awardId && name == other.name;
        }
        friend size_t qHash(const IdentityKey& key, size_t seed = 0) noexcept
        {
            return qHashMulti(seed, key.ukaseNo, key.day, key.awardId, key.name);
        }
    };
    using IdentityCount = QHash<IdentityKey, int>;

    void clear();
    bool load(QString* error);
    void invalidateDerived();
    void ensureDerived() const;
    void rebuildDerived() const;
    void rebuildIdentity();
    void addIdentity(const Person& person);
    void removeIdentity(const Person& person);
    bool writePerson(Person& person, QString* error);
    void storePerson(const Person& person);
    void bindText(class QSqlQuery& query, const QString& text) const;
    bool exec(class QSqlQuery& query, QString* error) const;

    QSqlDatabase m_db;
    QString m_path;
    QVector<Award> m_awards;
    QHash<int, int> m_awardById;
    QVector<Person> m_people;
    QHash<int, int> m_personById;
    IdentityCount m_identityName;
    IdentityCount m_identityNameBy;

    mutable bool m_derivedDirty = true;
    mutable QVector<StatMonth> m_stats;
    mutable QVector<DictEntry> m_dictionary;
    mutable QHash<QString, QStringList> m_first;
    mutable QHash<QString, QStringList> m_last;
    mutable QHash<QString, QStringList> m_middle;
    mutable QHash<QString, QStringList> m_nameRu; // целое белорусское имя → русские написания
    mutable QHash<QString, QString> m_profRu;
    mutable QHash<QString, QString> m_rankRu;
};
