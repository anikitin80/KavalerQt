#pragma once

#include <QDate>
#include <QHash>
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

    bool open(const QString& path, QString* error);
    void close();
    bool isOpen() const;
    QString path() const { return m_path; }

    const QVector<Award>& awards() const { return m_awards; }
    const QVector<Person>& people() const { return m_people; }
    const Award* award(int id) const;

    bool save(Person& person, QString* error);
    bool saveMany(QVector<Person>& people, QString* error);
    bool contains(const Person& probe) const;
    bool remove(const QList<int>& ids, QString* error);

    QStringList translateName(const QString& nameBy) const;
    QString professionRu(const QString& professionBy) const;
    QString rankRu(const QString& rankBy) const;
    bool fixQuotes(Person& person);

    int exportCsv(const QString& filePath, bool fromDate, const QDate& from, QString* error);
    int replaceText(TextField field, bool exact, const QString& find, const QString& replace, QString* error);

    const QVector<StatMonth>& stats() const { return m_stats; }
    const QVector<DictEntry>& dictionary() const { return m_dictionary; }
    int yearMin() const { return 1990; }
    int yearMax() const;

signals:
    void dataChanged();

private:
    void clear();
    bool load(QString* error);
    void rebuildDerived();
    void rebuildIdentity();
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
    QSet<QString> m_identityName;
    QSet<QString> m_identityNameBy;
    QVector<StatMonth> m_stats;
    QVector<DictEntry> m_dictionary;
    QHash<QString, QStringList> m_first;
    QHash<QString, QStringList> m_last;
    QHash<QString, QStringList> m_middle;
    QHash<QString, QString> m_profRu;
    QHash<QString, QDate> m_profDate;
    QHash<QString, QString> m_rankRu;
};
