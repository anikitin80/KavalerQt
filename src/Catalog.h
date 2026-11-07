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

    // readOnly: for reports and tools; every save then fails
    bool open(const QString& path, QString* error, bool readOnly = false);
    void close();
    bool isOpen() const;
    QString path() const { return m_path; }

    const QVector<Award>& awards() const { return m_awards; }
    const QVector<Person>& people() const { return m_people; }
    const Award* award(int id) const;
    // the order of awards everywhere: priority, then name, then id (<0, 0, >0)
    int compareAwards(int left, int right) const;
    // the record with this id, nullptr if there is none; valid until the next change
    const Person* person(int id) const;

    bool save(Person& person, QString* error);
    bool saveMany(QVector<Person>& people, QString* error);
    // records of the same person: decree number and date, award and name (Russian
    // or Belarusian). count() > 1 means namesakes in one decree.
    int count(const Person& probe) const;
    bool contains(const Person& probe) const { return count(probe) > 0; }
    bool remove(const QList<int>& ids, QString* error);

    QStringList translateName(const QString& nameBy) const;
    // the same Belarusian full name stored with more than one Russian spelling.
    // Whitespace is collapsed, so only the letters differ. Empty names are skipped.
    struct NameVariant
    {
        QString name;
        int count = 0;
    };
    struct NameConflict
    {
        QString nameBy;
        int rows = 0;
        QVector<NameVariant> names; // most frequent first
    };
    QVector<NameConflict> russianNameConflicts() const;
    QString professionRu(const QString& professionBy) const;
    QString rankRu(const QString& rankBy) const;
    bool fixQuotes(Person& person);

    // Latin letters typed inside Cyrillic words ("Міxaйлаўна", "прядильщицa",
    // "Iгар"; see NormalizeCyrillicText). save()/saveMany() always store the
    // fixed text; this finds the records that still have them.
    struct LetterFix
    {
        int records = 0;
        QMap<QString, int> fields; // field caption -> records
        QStringList samples;       // "before → after"
    };
    LetterFix findLatinLetters(QVector<Person>* fixed = nullptr, int maxSamples = 8) const;
    static bool normalizeTextFields(Person& person);
    // consistent copy of the open database (VACUUM INTO)
    bool backup(const QString& filePath, QString* error) const;

    int exportCsv(const QString& filePath, bool fromDate, const QDate& from, QString* error);
    // records whose field would change (exact: the whole field equals find)
    int countMatches(TextField field, bool exact, const QString& find) const;
    int replaceText(TextField field, bool exact, const QString& find, const QString& replace, QString* error);

    // Derived data (statistics, name dictionary, profession/rank translations)
    // is rebuilt on first use after a change. Callers must not keep these
    // references across a change of the catalog: copy what you show.
    const QVector<StatMonth>& stats() const;
    const QVector<DictEntry>& dictionary() const;
    int yearMin() const { return 1990; }
    int yearMax() const;
    // year of the latest decree (0 for an empty catalog), without the statistics
    int latestYear() const;

signals:
    // emitted after the database was changed, right before the in-memory data
    // changes: people() still has the old content (views save their state here)
    void aboutToChange();
    void dataChanged();

private:
    // a person is identified by decree number, decree date, award and name; the
    // award matters: one decree may give a person an order and a title.
    // Names are compared after NormalizeCyrillicText ("Iгар" == "Ігар").
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
    mutable QHash<QString, QString> m_profRu;
    mutable QHash<QString, QDate> m_profDate;
    mutable QHash<QString, QString> m_rankRu;
};
