#pragma once

#include "Catalog.h"

#include <QAbstractItemModel>
#include <QAbstractTableModel>
#include <QCollator>
#include <QHash>
#include <QSet>
#include <QColor>

class PersonModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit PersonModel(Catalog* catalog, QObject* parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    void apply(int awardId, int year, const QString& name, const QString& quick,
               bool needTranslationOnly, bool groupByAward);

    bool isGrouped() const { return m_grouped; }
    int shownCount() const;
    const Person* personAt(const QModelIndex& index) const;
    // индексы (столбец 0) показанных людей с этими номерами, по номеру
    QHash<int, QModelIndex> indexesOfPersons(const QSet<int>& personIds) const;
    // номер награды у строки группы (групповой вид), 0 у остальных строк
    int groupAwardId(const QModelIndex& index) const;
    QModelIndex groupIndex(int awardId) const;
    QStringList suggestions(int personId) const;
    // имена на выбор, по номеру человека; столбец имени перерисовывается один раз
    void setSuggestions(const QHash<int, QStringList>& suggestions);
    void clearSuggestions(int personId);
    // люди без русского имени, чей перевод ждёт выбора варианта
    int pendingSuggestions() const;

private:
    struct Mark
    {
        QString text;
        bool need = false;
        bool errors = false;
        bool quotes = false;
    };

    QString displayName(const Person& person) const;
    QString displayPersonal(const Person& person) const;
    QString displayUkase(const Person& person) const;
    QString displayAward(const Person& person) const;
    Mark translationMark(const Person& person) const;
    QString columnText(const Person& person, int column) const;
    bool less(int left, int right) const;
    void refreshAllRows();
    // то, что не зависит от человека, решается один раз на фильтр
    struct QuickFilter
    {
        QString text;
        QSet<int> awards;        // награды, в названии которых есть текст
        bool ukase = false;      // текст может быть частью «dd.MM.yyyy № N»
        bool comma = false;      // текст может захватить «должность, звание»
        bool posthumous = false; // текст — «X»
        bool mark = false;       // текст может быть частью пометки перевода
    };
    QuickFilter makeQuickFilter(const QString& quick) const;
    bool matchesQuick(const Person& person, const QuickFilter& filter) const;
    QString sortText(const Person& person) const;
    void sortPeople(QVector<int>& rows) const;
    void sortRows();
    bool isGroup(const QModelIndex& index) const;
    int catalogIndex(const QModelIndex& index) const;
    void sortGroups();
    void rebuildGroupLookup();

    struct Group
    {
        int awardId = 0;
        int priority = 0;
        QString title;
        QVector<int> people;
    };

    Catalog* m_catalog;
    QVector<int> m_rows;
    QVector<Group> m_groups;
    QHash<int, int> m_groupOfPerson;
    int m_sortColumn = 3;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    bool m_grouped = false;
    // русская сортировка: «Ё» после «Е», белорусские «і»/«ў» рядом с «и»/«у»,
    // числа по значению («5-й» раньше «12-й»)
    QCollator m_collator;
    QHash<int, QStringList> m_suggestions;
};

class StatsModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit StatsModel(Catalog* catalog, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void reload(bool byMonth);

private:
    Catalog* m_catalog;
    bool m_byMonth = true;
    QVector<StatMonth> m_months; // своя копия: каталог пересобирает статистику лениво
    QVector<int> m_awardIds;
    QHash<QPair<int, int>, int> m_yearCount;
    QVector<int> m_years;
};

class DictionaryModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DictionaryModel(Catalog* catalog, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void reload(const QString& filter);

private:
    Catalog* m_catalog;
    QVector<DictEntry> m_rows; // своя копия, см. StatsModel
};
