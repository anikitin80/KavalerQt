#include "Models.h"

#include <QBrush>
#include <QSet>
#include <algorithm>

namespace {

constexpr quintptr kGroupFlag = quintptr(1) << (sizeof(quintptr) * 8 - 1);

}

namespace {

bool hasBelarusianLetters(const QString& text)
{
    return text.contains(QChar(0x0406)) || text.contains(QChar(0x045E)) || text.contains(QStringLiteral("шч"));
}

// the same sequence of these characters in both texts ("«»" and "«»" yes,
// "«»" and "" no), without building the sequences
template <typename IsChar>
bool sameChars(const QString& left, const QString& right, IsChar isChar)
{
    qsizetype l = 0, r = 0;
    for (;;) {
        while (l < left.size() && !isChar(left.at(l).unicode()))
            ++l;
        while (r < right.size() && !isChar(right.at(r).unicode()))
            ++r;
        if (l == left.size() || r == right.size())
            return l == left.size() && r == right.size();
        if (left.at(l) != right.at(r))
            return false;
        ++l;
        ++r;
    }
}

bool markCanContain(const QString& quick)
{
    static const QStringList marks = {
        QStringLiteral("Есть"), QStringLiteral("Нет"), QStringLiteral("Имя"),
        QStringLiteral("Ошибки"), QStringLiteral("Тире"), QStringLiteral("Кавычки")
    };
    for (const QString& mark : marks) {
        if (mark.contains(quick, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

const QStringList kMonths = {
    QStringLiteral("Январь"), QStringLiteral("Февраль"), QStringLiteral("Март"),
    QStringLiteral("Апрель"), QStringLiteral("Май"), QStringLiteral("Июнь"),
    QStringLiteral("Июль"), QStringLiteral("Август"), QStringLiteral("Сентябрь"),
    QStringLiteral("Октябрь"), QStringLiteral("Ноябрь"), QStringLiteral("Декабрь")
};

} // namespace

PersonModel::PersonModel(Catalog* catalog, QObject* parent)
    : QAbstractItemModel(parent)
    , m_catalog(catalog)
    , m_collator(QLocale(QLocale::Russian, QLocale::Belarus))
{
    m_collator.setNumericMode(true);
}

bool PersonModel::isGroup(const QModelIndex& index) const
{
    return index.isValid() && (index.internalId() & kGroupFlag) != 0;
}

int PersonModel::catalogIndex(const QModelIndex& index) const
{
    if (!index.isValid() || isGroup(index))
        return -1;
    return int(index.internalId()) - 1;
}

void PersonModel::rebuildGroupLookup()
{
    m_groupOfPerson.clear();
    for (int group = 0; group < m_groups.size(); ++group) {
        for (int catalogRow : m_groups.at(group).people)
            m_groupOfPerson.insert(catalogRow, group);
    }
}

void PersonModel::sortGroups()
{
    const bool descending = m_sortColumn == 0 && m_sortOrder == Qt::DescendingOrder;
    std::stable_sort(m_groups.begin(), m_groups.end(), [this, descending](const Group& a, const Group& b) {
        const int cmp = m_catalog->compareAwards(a.awardId, b.awardId);
        return descending ? cmp > 0 : cmp < 0;
    });
    for (Group& group : m_groups)
        sortPeople(group.people);
    rebuildGroupLookup();
}

QModelIndex PersonModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column < 0 || column >= 6 || row < 0)
        return {};
    if (!parent.isValid()) {
        if (m_grouped) {
            if (row >= m_groups.size())
                return {};
            return createIndex(row, column, kGroupFlag | quintptr(row));
        }
        if (row >= m_rows.size())
            return {};
        return createIndex(row, column, quintptr(m_rows.at(row) + 1));
    }
    if (!isGroup(parent))
        return {};
    const int group = int(parent.internalId() & ~kGroupFlag);
    if (group < 0 || group >= m_groups.size() || row >= m_groups.at(group).people.size())
        return {};
    return createIndex(row, column, quintptr(m_groups.at(group).people.at(row) + 1));
}

QModelIndex PersonModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || !m_grouped || isGroup(child))
        return {};
    const int group = m_groupOfPerson.value(catalogIndex(child), -1);
    if (group < 0)
        return {};
    return createIndex(group, 0, kGroupFlag | quintptr(group));
}

int PersonModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_grouped ? m_groups.size() : m_rows.size();
    if (!isGroup(parent))
        return 0;
    const int group = int(parent.internalId() & ~kGroupFlag);
    if (group < 0 || group >= m_groups.size())
        return 0;
    return m_groups.at(group).people.size();
}

int PersonModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 6;
}

int PersonModel::shownCount() const
{
    if (!m_grouped)
        return m_rows.size();
    int count = 0;
    for (const Group& group : m_groups)
        count += group.people.size();
    return count;
}

QString PersonModel::displayAward(const Person& person) const
{
    const Award* award = m_catalog->award(person.awardId);
    return award ? award->name : QString();
}

QString PersonModel::displayName(const Person& person) const
{
    return person.name.isEmpty() ? person.nameBy : person.name;
}

QString PersonModel::displayPersonal(const Person& person) const
{
    QString text = person.personal.isEmpty() ? person.personalBy : person.personal;
    const QString rank = !person.rank.isEmpty() ? person.rank : person.rankBy;
    if (!rank.isEmpty()) {
        if (!text.isEmpty())
            text += QStringLiteral(", ");
        text += rank;
    }
    return text;
}

QString PersonModel::displayUkase(const Person& person) const
{
    // "dd.MM.yyyy № N" by hand: QDate::toString with a format parses it every call
    const auto twoDigits = [](int value) { return QString::number(value).rightJustified(2, QLatin1Char('0')); };
    const QString number = QChar(0x2116) + QLatin1Char(' ') + QString::number(person.ukaseNo);
    if (!person.ukaseDate.isValid())
        return number;
    const QDate& date = person.ukaseDate;
    return twoDigits(date.day()) + QLatin1Char('.') + twoDigits(date.month()) + QLatin1Char('.')
        + QString::number(date.year()).rightJustified(4, QLatin1Char('0')) + QLatin1Char(' ') + number;
}

PersonModel::Mark PersonModel::translationMark(const Person& person) const
{
    Mark mark;
    if (person.language != kLangBelarusian)
        return mark;

    if (person.needTranslation()) {
        mark.need = true;
        mark.text = (person.personal.isEmpty() && !person.name.isEmpty())
            ? QStringLiteral("Имя")
            : QStringLiteral("Нет");
        return mark;
    }

    mark.text = QStringLiteral("Есть");
    if (hasBelarusianLetters(person.name) || hasBelarusianLetters(person.personal) || hasBelarusianLetters(person.rank)) {
        mark.errors = true;
        mark.text = QStringLiteral("Ошибки");
    }
    if (!m_checkQuotes)
        return mark;

    const auto isDash = [](char16_t c) { return c == u'-' || c == u'\u2013'; };
    const auto isQuote = [](char16_t c) { return c == u'\u00AB' || c == u'\u00BB' || c == u'"'; };
    if (!sameChars(person.personal, person.personalBy, isDash)
        && !person.personal.contains(QStringLiteral("молочно-товар"))) {
        mark.quotes = true;
        mark.text = QStringLiteral("Тире");
    }
    if (!mark.quotes) {
        if (!sameChars(person.personal, person.personalBy, isQuote)) {
            mark.quotes = true;
            mark.text = QStringLiteral("Кавычки");
        }
    }
    return mark;
}

QString PersonModel::columnText(const Person& person, int column) const
{
    switch (column) {
    case 0: return displayAward(person);
    case 1: return displayName(person);
    case 2: return displayPersonal(person);
    case 3: return displayUkase(person);
    case 4: return person.posthumous ? QStringLiteral("X") : QString();
    case 5: return translationMark(person).text;
    default: return {};
    }
}

QVariant PersonModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (isGroup(index)) {
        const int group = int(index.internalId() & ~kGroupFlag);
        if (group < 0 || group >= m_groups.size())
            return {};
        const Group& item = m_groups.at(group);
        if (role == Qt::DisplayRole && (index.column() == 0 || index.column() == 1))
            return QStringLiteral("%1 (%2)").arg(item.title).arg(item.people.size());
        return {};
    }

    const int personIndex = catalogIndex(index);
    if (personIndex < 0 || personIndex >= m_catalog->people().size())
        return {};

    const Person& person = m_catalog->people().at(personIndex);
    if (m_grouped && role == Qt::DisplayRole && index.column() == 0)
        return {};
    if (role == Qt::UserRole)
        return person.id;
    if (role == Qt::DisplayRole)
        return columnText(person, index.column());

    if (role == Qt::BackgroundRole) {
        QColor color;
        if (!person.rank.isEmpty()) {
            if (person.rank.contains(QStringLiteral("милиц")))
                color = QColor(220, 220, 255);
            else if (person.rank.contains(QStringLiteral("юстиц")))
                color = QColor(255, 255, 180);
            else
                color = QColor(220, 255, 220);
        }
        if (person.posthumous)
            color = QColor(255, 200, 200);
        if (color.isValid())
            return QBrush(color);
        return {};
    }

    if (role == Qt::ForegroundRole && person.language == kLangBelarusian) {
        const Mark mark = translationMark(person);
        if (mark.quotes)
            return QBrush(QColor(0, 0, 196));
        if (mark.errors)
            return QBrush(QColor(196, 0, 0));
        if (mark.need) {
            if (index.column() == 1 && person.name.isEmpty()) {
                const bool suggested = !m_suggestions.value(person.id).isEmpty();
                return QBrush(suggested ? QColor(0, 196, 0) : QColor(128, 128, 128));
            }
            if (index.column() == 2 && person.personal.isEmpty())
                return QBrush(QColor(128, 128, 128));
        }
    }

    if (role == Qt::TextAlignmentRole && (index.column() == 3 || index.column() == 4 || index.column() == 5))
        return int(Qt::AlignCenter);

    return {};
}

Qt::ItemFlags PersonModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};
    if (isGroup(index))
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant PersonModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return QStringLiteral("Награда");
    case 1: return QStringLiteral("ФИО");
    case 2: return QStringLiteral("Личные данные");
    case 3: return QStringLiteral("Указ");
    case 4: return QStringLiteral("Посм.");
    case 5: return QStringLiteral("Перевод");
    default: return {};
    }
}

bool PersonModel::less(int left, int right) const
{
    const Person& a = m_catalog->people().at(left);
    const Person& b = m_catalog->people().at(right);

    int cmp = 0;
    switch (m_sortColumn) {
    case 0:
        cmp = m_catalog->compareAwards(a.awardId, b.awardId);
        break;
    case 1:
        cmp = QString::compare(displayName(a), displayName(b));
        break;
    case 2:
        cmp = QString::compare(displayPersonal(a), displayPersonal(b));
        break;
    case 3:
        if (a.ukaseDate != b.ukaseDate)
            cmp = a.ukaseDate < b.ukaseDate ? -1 : 1;
        else
            cmp = a.ukaseNo - b.ukaseNo;
        break;
    case 4:
        cmp = a.posthumous - b.posthumous;
        break;
    case 5:
        cmp = m_collator.compare(translationMark(a).text, translationMark(b).text);
        break;
    default:
        break;
    }
    if (cmp == 0)
        cmp = a.id - b.id;
    return m_sortOrder == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
}

PersonModel::QuickFilter PersonModel::makeQuickFilter(const QString& quick) const
{
    QuickFilter filter;
    filter.text = quick;
    for (const Award& award : m_catalog->awards()) {
        if (award.name.contains(quick, Qt::CaseInsensitive))
            filter.awards.insert(award.id);
    }
    // displayUkase() has only digits, dots, spaces and "№"
    filter.ukase = std::all_of(quick.cbegin(), quick.cend(), [](QChar c) {
        return c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char(' ') || c == QChar(0x2116);
    });
    filter.comma = quick.contains(QLatin1Char(','));
    filter.posthumous = QStringLiteral("X").contains(quick, Qt::CaseInsensitive);
    filter.mark = markCanContain(quick);
    return filter;
}

// the same result as searching the shown column texts, without building them
bool PersonModel::matchesQuick(const Person& person, const QuickFilter& filter) const
{
    const auto has = [&](const QString& text) {
        return text.contains(filter.text, Qt::CaseInsensitive);
    };
    if (filter.awards.contains(person.awardId) || has(displayName(person)))
        return true;
    if (filter.comma) {
        if (has(displayPersonal(person)))
            return true;
    } else {
        // without a comma the text cannot span the ", " between position and rank
        if (has(person.personal.isEmpty() ? person.personalBy : person.personal)
            || has(!person.rank.isEmpty() ? person.rank : person.rankBy))
            return true;
    }
    if (filter.ukase && has(displayUkase(person)))
        return true;
    if (person.posthumous && filter.posthumous)
        return true;
    return filter.mark && has(translationMark(person).text);
}

// text of the sort column for the text columns, empty for the others
QString PersonModel::sortText(const Person& person) const
{
    switch (m_sortColumn) {
    case 1: return displayName(person);
    case 2: return displayPersonal(person);
    case 5: return translationMark(person).text;
    default: return {};
    }
}

// text columns: the collation key of every row is made once per sort, not in
// each of the ~1.3 million comparisons; equal keys keep the order by id
void PersonModel::sortPeople(QVector<int>& rows) const
{
    if (m_sortColumn != 1 && m_sortColumn != 2 && m_sortColumn != 5) {
        std::stable_sort(rows.begin(), rows.end(), [this](int a, int b) { return less(a, b); });
        return;
    }
    struct Item
    {
        int row;
        int id;
        QCollatorSortKey key;
    };
    const auto& people = m_catalog->people();
    std::vector<Item> items;
    items.reserve(size_t(rows.size()));
    for (int row : rows)
        items.push_back({row, people.at(row).id, m_collator.sortKey(sortText(people.at(row)))});
    const bool ascending = m_sortOrder == Qt::AscendingOrder;
    std::sort(items.begin(), items.end(), [ascending](const Item& a, const Item& b) {
        int cmp = a.key.compare(b.key);
        if (cmp == 0)
            cmp = a.id - b.id;
        return ascending ? cmp < 0 : cmp > 0;
    });
    for (int i = 0; i < rows.size(); ++i)
        rows[i] = items[size_t(i)].row;
}

void PersonModel::sortRows()
{
    if (m_grouped)
        sortGroups();
    else
        sortPeople(m_rows);
}

void PersonModel::sort(int column, Qt::SortOrder order)
{
    if (column >= 0)
        m_sortColumn = column;
    m_sortOrder = order;
    if (m_grouped) {
        if (m_groups.isEmpty())
            return;
        beginResetModel();
        sortRows();
        endResetModel();
        return;
    }
    if (m_rows.isEmpty())
        return;
    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    // selection and current row are persistent indexes: move them with their people
    const QModelIndexList before = persistentIndexList();
    sortRows();
    QHash<int, int> rowOfCatalogIndex;
    rowOfCatalogIndex.reserve(m_rows.size());
    for (int row = 0; row < m_rows.size(); ++row)
        rowOfCatalogIndex.insert(m_rows.at(row), row);
    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex& index : before) {
        const int row = rowOfCatalogIndex.value(catalogIndex(index), -1);
        after.push_back(row < 0 ? QModelIndex() : createIndex(row, index.column(), index.internalId()));
    }
    changePersistentIndexList(before, after);
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

void PersonModel::apply(int awardId, int year, const QString& name, const QString& quick,
                        bool needTranslationOnly, bool checkQuotes, bool groupByAward)
{
    // the model must look unchanged until beginResetModel(): views and the
    // selection model still read it while handling modelAboutToBeReset
    beginResetModel();
    m_checkQuotes = checkQuotes;
    m_grouped = groupByAward && awardId == -1;
    m_rows.clear();
    m_groups.clear();
    m_groupOfPerson.clear();
    QHash<int, int> groupByAwardId;
    const auto& people = m_catalog->people();
    const QuickFilter quickFilter = makeQuickFilter(quick);
    m_rows.reserve(people.size());
    for (int i = 0; i < people.size(); ++i) {
        const Person& person = people.at(i);
        if (awardId != -1 && person.awardId != awardId)
            continue;
        if (year != -1 && (!person.ukaseDate.isValid() || person.ukaseDate.year() != year))
            continue;
        if (needTranslationOnly && !person.needTranslation())
            continue;
        if (!name.isEmpty() && !person.name.contains(name, Qt::CaseInsensitive)
            && !person.nameBy.contains(name, Qt::CaseInsensitive))
            continue;
        if (!quick.isEmpty() && !matchesQuick(person, quickFilter))
            continue;
        if (!m_grouped) {
            m_rows.push_back(i);
            continue;
        }
        int slot = groupByAwardId.value(person.awardId, -1);
        if (slot < 0) {
            Group group;
            group.awardId = person.awardId;
            if (const Award* award = m_catalog->award(person.awardId)) {
                group.title = award->name;
                group.priority = award->priority;
            } else {
                group.title = QStringLiteral("Без награды");
            }
            slot = m_groups.size();
            m_groups.push_back(group);
            groupByAwardId.insert(person.awardId, slot);
        }
        m_groups[slot].people.push_back(i);
    }
    sortRows();
    endResetModel();
}

void PersonModel::setCheckQuotes(bool check)
{
    if (m_checkQuotes == check)
        return;
    m_checkQuotes = check;
    if (m_sortColumn == 5) {
        sort(5, m_sortOrder);
        return;
    }
    const QList<int> roles = {Qt::DisplayRole, Qt::ForegroundRole};
    if (m_grouped) {
        for (int group = 0; group < m_groups.size(); ++group) {
            const int count = m_groups.at(group).people.size();
            if (count == 0)
                continue;
            const QModelIndex parent = index(group, 0);
            emit dataChanged(index(0, 1, parent), index(count - 1, 5, parent), roles);
        }
        return;
    }
    if (m_rows.isEmpty())
        return;
    emit dataChanged(index(0, 1), index(m_rows.size() - 1, 5), roles);
}

const Person* PersonModel::personAt(const QModelIndex& index) const
{
    const int personIndex = catalogIndex(index);
    if (personIndex < 0 || personIndex >= m_catalog->people().size())
        return nullptr;
    return &m_catalog->people().at(personIndex);
}

QHash<int, QModelIndex> PersonModel::indexesOfPersons(const QSet<int>& personIds) const
{
    QHash<int, QModelIndex> result;
    if (personIds.isEmpty())
        return result;
    const auto& people = m_catalog->people();
    const auto visit = [&](int catalogRow, int row, const QModelIndex& parent) {
        if (catalogRow < 0 || catalogRow >= people.size())
            return;
        const int id = people.at(catalogRow).id;
        if (personIds.contains(id))
            result.insert(id, index(row, 0, parent));
    };
    if (!m_grouped) {
        for (int row = 0; row < m_rows.size() && result.size() < personIds.size(); ++row)
            visit(m_rows.at(row), row, QModelIndex());
        return result;
    }
    for (int group = 0; group < m_groups.size() && result.size() < personIds.size(); ++group) {
        const QModelIndex parent = index(group, 0);
        const QVector<int>& members = m_groups.at(group).people;
        for (int row = 0; row < members.size(); ++row)
            visit(members.at(row), row, parent);
    }
    return result;
}

int PersonModel::groupAwardId(const QModelIndex& index) const
{
    if (!isGroup(index))
        return 0;
    const int group = int(index.internalId() & ~kGroupFlag);
    return group >= 0 && group < m_groups.size() ? m_groups.at(group).awardId : 0;
}

QModelIndex PersonModel::groupIndex(int awardId) const
{
    for (int group = 0; group < m_groups.size(); ++group) {
        if (m_groups.at(group).awardId == awardId)
            return index(group, 0);
    }
    return {};
}

QStringList PersonModel::suggestions(int personId) const
{
    return m_suggestions.value(personId);
}

void PersonModel::setSuggestions(const QHash<int, QStringList>& suggestions)
{
    if (suggestions.isEmpty())
        return;
    m_suggestions.insert(suggestions);
    if (!m_grouped) {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0, 1), index(m_rows.size() - 1, 1));
        return;
    }
    for (int group = 0; group < m_groups.size(); ++group) {
        const QModelIndex parent = index(group, 0);
        const int count = m_groups.at(group).people.size();
        if (count > 0)
            emit dataChanged(index(0, 1, parent), index(count - 1, 1, parent));
    }
}

void PersonModel::clearSuggestions(int personId)
{
    m_suggestions.remove(personId);
}

StatsModel::StatsModel(Catalog* catalog, QObject* parent)
    : QAbstractTableModel(parent)
    , m_catalog(catalog)
{
}

int StatsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_byMonth ? m_months.size() : m_awardIds.size();
}

int StatsModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_byMonth ? 15 : 2 + m_years.size();
}

QVariant StatsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return {};

    if (m_byMonth) {
        const StatMonth& row = m_months.at(index.row());
        if (index.column() == 0) {
            const Award* award = m_catalog->award(row.awardId);
            return award ? award->name : QString::number(row.awardId);
        }
        if (index.column() == 1)
            return row.year;
        if (index.column() == 2)
            return row.total;
        if (index.column() >= 3 && index.column() < 15)
            return row.month[index.column() - 3];
        return {};
    }

    const int awardId = m_awardIds.at(index.row());
    if (index.column() == 0) {
        const Award* award = m_catalog->award(awardId);
        return award ? award->name : QString::number(awardId);
    }
    if (index.column() == 1) {
        int total = 0;
        for (int year : m_years)
            total += m_yearCount.value(qMakePair(awardId, year));
        return total;
    }
    const int year = m_years.at(index.column() - 2);
    return m_yearCount.value(qMakePair(awardId, year));
}

QVariant StatsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    if (m_byMonth) {
        switch (section) {
        case 0: return QStringLiteral("Награда");
        case 1: return QStringLiteral("Год");
        case 2: return QStringLiteral("Всего");
        default:
            if (section >= 3 && section < 15)
                return kMonths.at(section - 3);
            return {};
        }
    }
    if (section == 0)
        return QStringLiteral("Награда");
    if (section == 1)
        return QStringLiteral("Всего");
    return QString::number(m_years.at(section - 2));
}

void StatsModel::reload(bool byMonth)
{
    beginResetModel();
    m_byMonth = byMonth;
    m_months = m_catalog->stats();
    m_awardIds.clear();
    m_yearCount.clear();
    m_years.clear();
    if (!byMonth) {
        QSet<int> awards;
        for (int year = m_catalog->yearMin(); year <= m_catalog->yearMax(); ++year)
            m_years.push_back(year);
        for (const StatMonth& row : m_months) {
            awards.insert(row.awardId);
            m_yearCount.insert(qMakePair(row.awardId, row.year), row.total);
        }
        m_awardIds = awards.values();
        std::sort(m_awardIds.begin(), m_awardIds.end(), [this](int a, int b) {
            return m_catalog->compareAwards(a, b) < 0;
        });
    }
    endResetModel();
}

DictionaryModel::DictionaryModel(Catalog* catalog, QObject* parent)
    : QAbstractTableModel(parent)
    , m_catalog(catalog)
{
}

int DictionaryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int DictionaryModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 4;
}

QVariant DictionaryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= m_rows.size())
        return {};
    const DictEntry& entry = m_rows.at(index.row());
    switch (index.column()) {
    case 0: return entry.type;
    case 1: return entry.by;
    case 2: return entry.ru;
    case 3: return entry.alt;
    default: return {};
    }
}

QVariant DictionaryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return QStringLiteral("Тип");
    case 1: return QStringLiteral("Белорусский");
    case 2: return QStringLiteral("Русский");
    case 3: return QStringLiteral("Другие переводы");
    default: return {};
    }
}

void DictionaryModel::reload(const QString& filter)
{
    beginResetModel();
    m_rows.clear();
    for (const DictEntry& entry : m_catalog->dictionary()) {
        if (!filter.isEmpty()
            && !entry.by.contains(filter, Qt::CaseInsensitive)
            && !entry.ru.contains(filter, Qt::CaseInsensitive)
            && !entry.alt.contains(filter, Qt::CaseInsensitive))
            continue;
        m_rows.push_back(entry);
    }
    endResetModel();
}
