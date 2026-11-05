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

QVector<QChar> collectChars(const QString& text, const QString& chars)
{
    QVector<QChar> result;
    for (const QChar c : text) {
        if (chars.contains(c))
            result.push_back(c);
    }
    return result;
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
{
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
    std::stable_sort(m_groups.begin(), m_groups.end(), [descending](const Group& a, const Group& b) {
        if (a.priority != b.priority)
            return descending ? a.priority > b.priority : a.priority < b.priority;
        return descending ? a.awardId > b.awardId : a.awardId < b.awardId;
    });
    for (Group& group : m_groups)
        std::stable_sort(group.people.begin(), group.people.end(), [this](int a, int b) { return less(a, b); });
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
    const QString number = QStringLiteral("№ %1").arg(person.ukaseNo);
    if (!person.ukaseDate.isValid())
        return number;
    return person.ukaseDate.toString(QStringLiteral("dd.MM.yyyy")) + QLatin1Char(' ') + number;
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

    const QString dashes = QStringLiteral("-–");
    const auto dashRu = collectChars(person.personal, dashes);
    const auto dashBy = collectChars(person.personalBy, dashes);
    if (dashRu != dashBy && !person.personal.contains(QStringLiteral("молочно-товар"))) {
        mark.quotes = true;
        mark.text = QStringLiteral("Тире");
    }
    if (!mark.quotes) {
        const QString quoteChars = QStringLiteral("«»\"");
        if (collectChars(person.personal, quoteChars) != collectChars(person.personalBy, quoteChars)) {
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
    const Award* awardA = m_catalog->award(a.awardId);
    const Award* awardB = m_catalog->award(b.awardId);

    int cmp = 0;
    switch (m_sortColumn) {
    case 0: {
        const int pa = awardA ? awardA->priority : 0;
        const int pb = awardB ? awardB->priority : 0;
        cmp = pa - pb;
        if (cmp == 0)
            cmp = QString::compare(displayAward(a), displayAward(b));
        break;
    }
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
    case 5: {
        const QString leftText = m_sortMarks ? m_sortMarks->value(left) : translationMark(a).text;
        const QString rightText = m_sortMarks ? m_sortMarks->value(right) : translationMark(b).text;
        cmp = QString::compare(leftText, rightText);
        break;
    }
    default:
        break;
    }
    if (cmp == 0)
        cmp = a.id - b.id;
    return m_sortOrder == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
}

bool PersonModel::matchesQuick(const Person& person, const QString& quick) const
{
    const auto has = [&](const QString& text) {
        return text.contains(quick, Qt::CaseInsensitive);
    };
    if (has(displayAward(person)) || has(displayName(person)) || has(displayPersonal(person))
        || has(displayUkase(person)))
        return true;
    if (person.posthumous && has(QStringLiteral("X")))
        return true;
    return markCanContain(quick) && has(translationMark(person).text);
}

void PersonModel::fillSortMarks(QHash<int, QString>& marks) const
{
    const auto& people = m_catalog->people();
    const auto add = [&](int index) {
        marks.insert(index, translationMark(people.at(index)).text);
    };
    if (m_grouped) {
        for (const Group& group : m_groups) {
            for (int index : group.people)
                add(index);
        }
        return;
    }
    for (int index : m_rows)
        add(index);
}

void PersonModel::sortRows()
{
    QHash<int, QString> marks;
    if (m_sortColumn == 5) {
        fillSortMarks(marks);
        m_sortMarks = &marks;
    }
    if (m_grouped)
        sortGroups();
    else
        std::stable_sort(m_rows.begin(), m_rows.end(), [this](int a, int b) { return less(a, b); });
    m_sortMarks = nullptr;
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
    layoutAboutToBeChanged();
    sortRows();
    layoutChanged();
}

void PersonModel::apply(int awardId, int year, const QString& name, const QString& quick,
                        bool needTranslationOnly, bool checkQuotes, bool groupByAward)
{
    m_checkQuotes = checkQuotes;
    m_grouped = groupByAward && awardId == -1;

    beginResetModel();
    m_rows.clear();
    m_groups.clear();
    m_groupOfPerson.clear();
    QHash<int, int> groupByAwardId;
    const auto& people = m_catalog->people();
    m_rows.reserve(people.size());
    for (int i = 0; i < people.size(); ++i) {
        const Person& person = people.at(i);
        if (awardId != -1 && person.awardId != awardId)
            continue;
        if (year != -1 && (!person.ukaseDate.isValid() || person.ukaseDate.year() != year))
            continue;
        if (needTranslationOnly && !person.needTranslation())
            continue;
        if (!name.isEmpty() && !person.name.contains(name) && !person.nameBy.contains(name))
            continue;
        if (!quick.isEmpty() && !matchesQuick(person, quick))
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

QStringList PersonModel::suggestions(int personId) const
{
    return m_suggestions.value(personId);
}

void PersonModel::setSuggestions(int personId, const QStringList& values)
{
    m_suggestions.insert(personId, values);
    const auto& people = m_catalog->people();
    if (!m_grouped) {
        for (int row = 0; row < m_rows.size(); ++row) {
            if (people.at(m_rows.at(row)).id == personId) {
                emit dataChanged(index(row, 1), index(row, 1));
                return;
            }
        }
        return;
    }
    for (int group = 0; group < m_groups.size(); ++group) {
        const QModelIndex parent = index(group, 0);
        for (int row = 0; row < m_groups.at(group).people.size(); ++row) {
            if (people.at(m_groups.at(group).people.at(row)).id == personId) {
                const QModelIndex cell = index(row, 1, parent);
                emit dataChanged(cell, cell);
                return;
            }
        }
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
    return m_byMonth ? m_catalog->stats().size() : m_awardIds.size();
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
        const StatMonth& row = m_catalog->stats().at(index.row());
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
    m_awardIds.clear();
    m_yearCount.clear();
    m_years.clear();
    if (!byMonth) {
        QSet<int> awards;
        for (int year = m_catalog->yearMin(); year <= m_catalog->yearMax(); ++year)
            m_years.push_back(year);
        for (const StatMonth& row : m_catalog->stats()) {
            awards.insert(row.awardId);
            m_yearCount.insert(qMakePair(row.awardId, row.year), row.total);
        }
        m_awardIds = awards.values();
        std::sort(m_awardIds.begin(), m_awardIds.end(), [this](int a, int b) {
            const Award* aa = m_catalog->award(a);
            const Award* bb = m_catalog->award(b);
            const int pa = aa ? aa->priority : 0;
            const int pb = bb ? bb->priority : 0;
            if (pa != pb)
                return pa < pb;
            return QString::compare(aa ? aa->name : QString(), bb ? bb->name : QString()) < 0;
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
    const DictEntry& entry = m_catalog->dictionary().at(m_rows.at(index.row()));
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
    const auto& entries = m_catalog->dictionary();
    for (int i = 0; i < entries.size(); ++i) {
        const DictEntry& entry = entries.at(i);
        if (!filter.isEmpty()
            && !entry.by.contains(filter, Qt::CaseInsensitive)
            && !entry.ru.contains(filter, Qt::CaseInsensitive)
            && !entry.alt.contains(filter, Qt::CaseInsensitive))
            continue;
        m_rows.push_back(i);
    }
    endResetModel();
}
