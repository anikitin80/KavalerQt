#include "Models.h"

#include <QBrush>
#include <QFont>
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

// одна и та же последовательность этих знаков в обоих текстах ("«»" и "«»" — да,
// "«»" и "" — нет), без сборки самих последовательностей
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
    if (!person.name.isEmpty())
        return person.name;
    if (!person.nameBy.isEmpty())
        return person.nameBy;
    if (!person.personal.isEmpty() || !person.personalBy.isEmpty())
        return QStringLiteral("Организация");
    return {};
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
    // «dd.MM.yyyy № N» вручную: QDate::toString с форматом разбирает его при каждом вызове
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
    // тире и кавычки сверяются всегда: облачный перевод их путает
    // («приемно»аппаратного», "Савушкин продукт" при «Савушкін прадукт»)
    const auto isDash = [](char16_t c) { return c == u'-' || c == u'\u2013'; };
    const auto isQuote = [](char16_t c) {
        return c == u'\u00AB' || c == u'\u00BB' || c == u'"' || c == u'\u201C' || c == u'\u201D' || c == u'\u201E';
    };
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
    // перевод имени по словарю с несколькими вариантами ждёт выбора:
    // в столбце имени лучший вариант, фон строки розовый
    const QStringList variants = person.name.isEmpty() ? m_suggestions.value(person.id) : QStringList();
    if (role == Qt::DisplayRole) {
        if (index.column() == 1 && !variants.isEmpty())
            return variants.first();
        return columnText(person, index.column());
    }
    if (role == Qt::ToolTipRole && !variants.isEmpty()) {
        return QStringLiteral("Перевод не сохранён: вариантов несколько, выберите нужный\n"
                              "(контекстное меню «Варианты перевода» или «Подтвердить перевод»).\n\n%1\n\n%2")
            .arg(person.nameBy, variants.join(QLatin1Char('\n')));
    }
    if (role == Qt::FontRole && person.posthumous) {
        QFont font;
        font.setBold(true);
        return font;
    }

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
        if (!variants.isEmpty())
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
            if (index.column() == 1 && person.name.isEmpty())
                return QBrush(QColor(128, 128, 128));
            if (index.column() == 2 && person.personal.isEmpty())
                return QBrush(QColor(128, 128, 128));
        }
    }
    // посмертно: бордовый жирный шрифт, фон остаётся за званием
    if (role == Qt::ForegroundRole && person.posthumous)
        return QBrush(QColor(128, 0, 32));

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
    // displayUkase() содержит только цифры, точки, пробелы и «№»
    filter.ukase = std::all_of(quick.cbegin(), quick.cend(), [](QChar c) {
        return c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char(' ') || c == QChar(0x2116);
    });
    filter.comma = quick.contains(QLatin1Char(','));
    filter.posthumous = QStringLiteral("X").contains(quick, Qt::CaseInsensitive);
    filter.mark = markCanContain(quick);
    return filter;
}

// тот же результат, что поиск по показанным текстам столбцов, без их сборки
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
        // без запятой текст не может захватить «, » между должностью и званием
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

// текст столбца сортировки для текстовых столбцов, пусто для остальных
QString PersonModel::sortText(const Person& person) const
{
    switch (m_sortColumn) {
    case 1: return displayName(person);
    case 2: return displayPersonal(person);
    case 5: return translationMark(person).text;
    default: return {};
    }
}

// текстовые столбцы: ключ сортировки каждой строки делается один раз на сортировку,
// а не в каждом из ~1,3 миллиона сравнений; равные ключи сохраняют порядок по номеру
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
    // выделение и текущая строка — устойчивые индексы: переносим их вместе с людьми
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
                        bool needTranslationOnly, bool groupByAward)
{
    // модель должна выглядеть прежней до beginResetModel(): виды и
    // модель выделения ещё читают её, обрабатывая modelAboutToBeReset
    beginResetModel();
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
    refreshAllRows();
}

void PersonModel::clearSuggestions(int personId)
{
    if (m_suggestions.remove(personId))
        refreshAllRows();
}

int PersonModel::pendingSuggestions() const
{
    int pending = 0;
    for (auto it = m_suggestions.cbegin(); it != m_suggestions.cend(); ++it) {
        const Person* person = m_catalog->person(it.key());
        if (person && person->name.isEmpty() && !it.value().isEmpty())
            ++pending;
    }
    return pending;
}

// цвет предложения — у всей строки
void PersonModel::refreshAllRows()
{
    const int last = columnCount() - 1;
    if (!m_grouped) {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0, 0), index(m_rows.size() - 1, last));
        return;
    }
    for (int group = 0; group < m_groups.size(); ++group) {
        const QModelIndex parent = index(group, 0);
        const int count = m_groups.at(group).people.size();
        if (count > 0)
            emit dataChanged(index(0, 0, parent), index(count - 1, last, parent));
    }
}

namespace {
// внутренний номер у строки группы и у строки таблицы без месяцев; у строки внутри группы — номер группы
constexpr quintptr kStatsTopId = ~quintptr(0);
}

StatsModel::StatsModel(Catalog* catalog, QObject* parent)
    : QAbstractItemModel(parent)
    , m_catalog(catalog)
{
}

QModelIndex StatsModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= columnCount())
        return {};
    if (!parent.isValid()) {
        const int rows = m_byMonth ? m_groups.size() : m_awardIds.size();
        return row < rows ? createIndex(row, column, kStatsTopId) : QModelIndex();
    }
    if (!m_byMonth || parent.internalId() != kStatsTopId || parent.row() >= m_groups.size()
        || row >= m_groups.at(parent.row()).rows.size())
        return {};
    return createIndex(row, column, quintptr(parent.row()));
}

QModelIndex StatsModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || child.internalId() == kStatsTopId)
        return {};
    return createIndex(int(child.internalId()), 0, kStatsTopId);
}

int StatsModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_byMonth ? m_groups.size() : m_awardIds.size();
    if (m_byMonth && parent.internalId() == kStatsTopId && parent.column() == 0 && parent.row() < m_groups.size())
        return m_groups.at(parent.row()).rows.size();
    return 0;
}

int StatsModel::columnCount(const QModelIndex&) const
{
    return m_byMonth ? 14 : 2 + m_years.size();
}

QVariant StatsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (m_byMonth) {
        const bool top = index.internalId() == kStatsTopId;
        if (role == Qt::FontRole && top) {
            QFont font;
            font.setBold(true);
            return font;
        }
        if (role == Qt::TextAlignmentRole && index.column() > 0)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (role != Qt::DisplayRole)
            return {};
        const Group& group = m_groups.at(top ? index.row() : int(index.internalId()));
        const Line& line = top ? group.line : group.rows.at(index.row());
        if (index.column() == 0)
            return line.label;
        if (index.column() == 1)
            return line.total;
        const int count = line.month[index.column() - 2];
        return count ? QVariant(count) : QVariant();
    }

    if (role != Qt::DisplayRole)
        return {};
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
        if (section == 0)
            return m_grouping == ByAward ? QStringLiteral("Награда / год") : QStringLiteral("Год / награда");
        if (section == 1)
            return QStringLiteral("Всего");
        if (section >= 2 && section < 14)
            return kMonths.at(section - 2);
        return {};
    }
    if (section == 0)
        return QStringLiteral("Награда");
    if (section == 1)
        return QStringLiteral("Всего");
    return QString::number(m_years.at(section - 2));
}

void StatsModel::reload(bool byMonth, Grouping grouping)
{
    beginResetModel();
    m_byMonth = byMonth;
    m_grouping = grouping;
    m_groups.clear();
    m_awardIds.clear();
    m_yearCount.clear();
    m_years.clear();
    // каталог отдаёт строки по наградам (в их порядке), внутри — по годам
    const QVector<StatMonth> months = m_catalog->stats();
    const auto awardName = [this](int awardId) {
        const Award* award = m_catalog->award(awardId);
        return award ? award->name : QString::number(awardId);
    };
    const auto add = [](Line& line, const StatMonth& row) {
        line.total += row.total;
        for (int month = 0; month < 12; ++month)
            line.month[month] += row.month[month];
    };
    if (byMonth) {
        QHash<int, int> groupOf;
        QVector<StatMonth> ordered = months;
        if (grouping == ByYear) {
            std::stable_sort(ordered.begin(), ordered.end(),
                [](const StatMonth& a, const StatMonth& b) { return a.year < b.year; });
        }
        for (const StatMonth& row : ordered) {
            const int key = grouping == ByAward ? row.awardId : row.year;
            int slot = groupOf.value(key, -1);
            if (slot < 0) {
                Group group;
                group.line.label = grouping == ByAward ? awardName(row.awardId) : QString::number(row.year);
                slot = m_groups.size();
                m_groups.push_back(group);
                groupOf.insert(key, slot);
            }
            Group& group = m_groups[slot];
            add(group.line, row);
            Line line;
            line.label = grouping == ByAward ? QString::number(row.year) : awardName(row.awardId);
            add(line, row);
            group.rows.push_back(line);
        }
    } else {
        QSet<int> awards;
        for (int year = m_catalog->yearMin(); year <= m_catalog->yearMax(); ++year)
            m_years.push_back(year);
        for (const StatMonth& row : months) {
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

namespace {
// внутренний номер у строки группы; у части — номер её группы
constexpr quintptr kDictGroupId = ~quintptr(0);
}

DictionaryModel::DictionaryModel(Catalog* catalog, QObject* parent)
    : QAbstractItemModel(parent)
    , m_catalog(catalog)
{
}

bool DictionaryModel::isGroup(const QModelIndex& index)
{
    return index.isValid() && index.internalId() == kDictGroupId;
}

QModelIndex DictionaryModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= columnCount(parent))
        return {};
    if (!parent.isValid())
        return row < m_groups.size() ? createIndex(row, column, kDictGroupId) : QModelIndex();
    if (!isGroup(parent) || parent.row() >= m_groups.size() || row >= m_groups.at(parent.row()).rows.size())
        return {};
    return createIndex(row, column, quintptr(parent.row()));
}

QModelIndex DictionaryModel::parent(const QModelIndex& child) const
{
    if (!child.isValid() || isGroup(child))
        return {};
    return createIndex(int(child.internalId()), 0, kDictGroupId);
}

int DictionaryModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_groups.size();
    if (isGroup(parent) && parent.column() == 0 && parent.row() < m_groups.size())
        return m_groups.at(parent.row()).rows.size();
    return 0;
}

int DictionaryModel::columnCount(const QModelIndex&) const
{
    return 3;
}

QVariant DictionaryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    if (isGroup(index)) {
        if (index.row() >= m_groups.size() || index.column() != 0)
            return {};
        const Group& group = m_groups.at(index.row());
        if (role == Qt::DisplayRole)
            return QStringLiteral("%1 (%2)").arg(group.type).arg(group.rows.size());
        if (role == Qt::FontRole) {
            QFont font;
            font.setBold(true);
            return font;
        }
        return {};
    }
    if (role != Qt::DisplayRole)
        return {};
    const DictEntry& entry = m_groups.at(int(index.internalId())).rows.at(index.row());
    switch (index.column()) {
    case 0: return entry.by;
    case 1: return entry.ru;
    case 2: return entry.alt;
    default: return {};
    }
}

QVariant DictionaryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return QStringLiteral("Белорусский");
    case 1: return QStringLiteral("Русский");
    case 2: return QStringLiteral("Другие переводы");
    default: return {};
    }
}

void DictionaryModel::reload(const QString& filter, bool variantsOnly)
{
    beginResetModel();
    m_groups.clear();
    for (const DictEntry& entry : m_catalog->dictionary()) {
        if (variantsOnly && entry.alt.isEmpty())
            continue;
        if (!filter.isEmpty()
            && !entry.by.contains(filter, Qt::CaseInsensitive)
            && !entry.ru.contains(filter, Qt::CaseInsensitive)
            && !entry.alt.contains(filter, Qt::CaseInsensitive))
            continue;
        // словарь идёт по типам подряд: фамилии, имена, отчества
        if (m_groups.isEmpty() || m_groups.last().type != entry.type)
            m_groups.push_back(Group{entry.type, {}});
        m_groups.last().rows.push_back(entry);
    }
    endResetModel();
}
