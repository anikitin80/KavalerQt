#include "Catalog.h"
#include "TextUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>
#include <algorithm>

namespace {

bool isAlphaOrDigit(QChar character)
{
    const ushort c = character.unicode();
    if (c >= u'0' && c <= u'9')
        return true;
    if (c >= u'A' && c <= u'z')
        return true;
    if (c >= u'Ё' && c <= u'і')
        return true;
    return false;
}

int findAny(const QString& text, int from, const QString& chars)
{
    for (int i = qMax(0, from); i < text.size(); ++i) {
        if (chars.contains(text.at(i)))
            return i;
    }
    return -1;
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

void addDict(QHash<QString, QStringList>& dict, const QString& by, const QString& ru)
{
    auto it = dict.find(by);
    if (it == dict.end()) {
        dict.insert(by, QStringList{ru});
        return;
    }
    if (!it->contains(ru))
        it->push_back(ru);
}

// написания части имени: самое частое первым, при равенстве — встреченное раньше
void rankDict(QHash<QString, QStringList>& dict, const QHash<QString, QHash<QString, int>>& counts)
{
    for (auto it = dict.begin(); it != dict.end(); ++it) {
        if (it->size() < 2)
            continue;
        const QHash<QString, int> seen = counts.value(it.key());
        std::stable_sort(it->begin(), it->end(), [&seen](const QString& a, const QString& b) {
            return seen.value(a) > seen.value(b);
        });
    }
}

QString joinAlt(const QStringList& values)
{
    if (values.size() <= 1)
        return QString();
    return values.mid(1).join(QStringLiteral(", "));
}

// схлопнутые пробелы и заменённые латинские двойники, те же буквы, что сравнивает личность
QString belarusianNameKey(const QString& name)
{
    return NormalizeCyrillicText(name.simplified(), true);
}

} // namespace

bool Person::needTranslation() const
{
    if (name.isEmpty() && !nameBy.isEmpty())
        return true;
    if (personal.isEmpty() && !personalBy.isEmpty())
        return true;
    if (rank.isEmpty() && !rankBy.isEmpty())
        return true;
    return false;
}

QString* Person::field(TextField which)
{
    return const_cast<QString*>(static_cast<const Person*>(this)->field(which));
}

const QString* Person::field(TextField which) const
{
    switch (which) {
    case TextField::NameRu: return &name;
    case TextField::NameBy: return &nameBy;
    case TextField::PersonalRu: return &personal;
    case TextField::PersonalBy: return &personalBy;
    }
    return nullptr;
}

Catalog::Catalog(QObject* parent)
    : QObject(parent)
{
}

Catalog::~Catalog()
{
    close();
}

bool Catalog::isOpen() const
{
    return m_db.isOpen();
}

void Catalog::close()
{
    clear();
    if (m_db.isValid()) {
        const QString name = m_db.connectionName();
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
    m_path.clear();
}

void Catalog::clear()
{
    m_awards.clear();
    m_awardById.clear();
    m_people.clear();
    m_personById.clear();
    m_identityName.clear();
    m_identityNameBy.clear();
    m_stats.clear();
    m_dictionary.clear();
    m_first.clear();
    m_last.clear();
    m_middle.clear();
    m_nameRu.clear();
    m_profRu.clear();
    m_rankRu.clear();
    m_derivedDirty = true;
    m_belarusianWords.clear();
    m_belarusianWordsDirty = true;
}

void Catalog::invalidateDerived()
{
    m_derivedDirty = true;
    m_belarusianWordsDirty = true;
}

namespace {

// слова текста в нижнем регистре, без названий в кавычках: «Мінскі трактарны завод»
// в русском переводе законно остаётся белорусским
QSet<QString> textWords(const QString& text)
{
    QSet<QString> words;
    QString word;
    int quoteDepth = 0;
    bool straightQuote = false;
    const auto flush = [&] {
        if (word.size() >= 3 && quoteDepth == 0 && !straightQuote)
            words.insert(word.toLower());
        word.clear();
    };
    for (const QChar c : text) {
        if (c.isLetter() || c == QLatin1Char('\'') || c == QChar(0x2019) || c == QChar(0x02BC)
            || (c == QLatin1Char('-') && !word.isEmpty())) {
            word += c;
            continue;
        }
        flush();
        if (c == QChar(0x00AB))
            ++quoteDepth;
        else if (c == QChar(0x00BB) && quoteDepth > 0)
            --quoteDepth;
        else if (c == QLatin1Char('"'))
            straightQuote = !straightQuote;
    }
    flush();
    return words;
}

} // namespace

QStringList Catalog::untranslatedWords(const Person& person) const
{
    if (m_belarusianWordsDirty) {
        QHash<QString, int> belarusian, russian, original;
        for (const Person& item : m_people) {
            for (const QString& word : textWords(item.personalBy) | textWords(item.rankBy))
                ++belarusian[word];
            const QSet<QString> words = textWords(item.personal) | textWords(item.rank);
            for (const QString& word : words) {
                ++russian[word];
                if (item.language == kLangRussian)
                    ++original[word];
            }
        }
        m_belarusianWords.clear();
        for (auto it = russian.cbegin(); it != russian.cend(); ++it) {
            const int count = belarusian.value(it.key());
            if (count >= kBelarusianWordMinCount && it.value() * kBelarusianWordRatio <= count
                && !original.contains(it.key()))
                m_belarusianWords.insert(it.key());
        }
        m_belarusianWordsDirty = false;
    }
    if (m_belarusianWords.isEmpty())
        return {};
    QStringList found;
    for (const QString& word : textWords(person.personal) | textWords(person.rank)) {
        if (m_belarusianWords.contains(word))
            found.push_back(word);
    }
    found.sort();
    return found;
}

void Catalog::ensureDerived() const
{
    if (m_derivedDirty)
        rebuildDerived();
}

const QVector<StatMonth>& Catalog::stats() const
{
    ensureDerived();
    return m_stats;
}

const QVector<DictEntry>& Catalog::dictionary() const
{
    ensureDerived();
    return m_dictionary;
}

const Award* Catalog::award(int id) const
{
    const auto it = m_awardById.constFind(id);
    if (it == m_awardById.cend())
        return nullptr;
    return &m_awards.at(*it);
}

int Catalog::yearMax() const
{
    return QDate::currentDate().year();
}

int Catalog::latestYear() const
{
    int latest = 0;
    for (const Person& person : m_people) {
        if (person.ukaseDate.isValid())
            latest = qMax(latest, person.ukaseDate.year());
    }
    return latest;
}

int Catalog::compareAwards(int left, int right) const
{
    if (left == right)
        return 0;
    const Award* a = award(left);
    const Award* b = award(right);
    const int priorityLeft = a ? a->priority : 0;
    const int priorityRight = b ? b->priority : 0;
    if (priorityLeft != priorityRight)
        return priorityLeft < priorityRight ? -1 : 1;
    const int byName = QString::compare(a ? a->name : QString(), b ? b->name : QString());
    if (byName != 0)
        return byName;
    return left < right ? -1 : 1;
}

const Person* Catalog::person(int id) const
{
    const auto it = m_personById.constFind(id);
    return it == m_personById.cend() ? nullptr : &m_people.at(*it);
}

bool Catalog::exec(QSqlQuery& query, QString* error) const
{
    if (query.exec())
        return true;
    if (error)
        *error = query.lastError().text();
    return false;
}

void Catalog::bindText(QSqlQuery& query, const QString& text) const
{
    if (text.isEmpty())
        query.addBindValue(QVariant());
    else
        query.addBindValue(text);
}

// открывает файл отдельным соединением и читает обе таблицы: негодный файл
// отвергается до закрытия текущей базы
static bool probeDatabase(const QString& path, QString* error)
{
    const QString connection = QStringLiteral("kavaler-probe");
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery query(db);
            ok = query.exec(QStringLiteral("SELECT ID, Type, Priority, Name, NameBY FROM Awards LIMIT 1"))
                && query.exec(QStringLiteral("SELECT ID, Award_ID, Name, Name_BY FROM Kavalers LIMIT 1"));
            if (!ok && error)
                *error = query.lastError().text();
        } else if (error) {
            *error = db.lastError().text();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

bool Catalog::open(const QString& path, QString* error, bool readOnly)
{
    if (!QFileInfo::exists(path)) {
        // SQLite молча создал бы пустой файл
        if (error)
            *error = QStringLiteral("Файл не найден: %1").arg(QDir::toNativeSeparators(path));
        return false;
    }
    if (!probeDatabase(path, error))
        return false;

    const bool wasOpen = isOpen();
    if (wasOpen)
        emit aboutToChange();
    close();
    // отсюда сбой оставляет без базы: видам говорят показать пустоту
    const auto fail = [&] {
        close();
        if (wasOpen)
            emit dataChanged();
        return false;
    };

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("kavaler"));
    m_db.setDatabaseName(path);
    if (readOnly)
        m_db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        return fail();
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    if (!load(error))
        return fail();

    m_path = path;
    emit dataChanged();
    return true;
}

bool Catalog::load(QString* error)
{
    QSqlQuery awards(m_db);
    if (!awards.exec(QStringLiteral("SELECT ID, Type, Priority, Name, NameBY FROM Awards"))) {
        if (error)
            *error = awards.lastError().text();
        return false;
    }
    while (awards.next()) {
        Award award;
        award.id = awards.value(0).toInt();
        award.type = awards.value(1).toInt();
        award.priority = awards.value(2).toInt();
        award.name = awards.value(3).toString();
        award.nameBy = awards.value(4).toString();
        m_awardById.insert(award.id, m_awards.size());
        m_awards.push_back(award);
    }

    QSqlQuery people(m_db);
    people.setForwardOnly(true); // строки читаются один раз: без кэша всех 78 тысяч
    if (!people.exec(QStringLiteral(
            "SELECT ID, Award_ID, Name, Name_BY, Personal_Info, Personal_Info_BY, "
            "Awarded_For, Rank, Rank_BY, Ukase_No, Ukase_Date, Posthumous, Ukase_Lang "
            "FROM Kavalers"))) {
        if (error)
            *error = people.lastError().text();
        return false;
    }

    m_people.reserve(80000);
    while (people.next()) {
        Person person;
        person.id = people.value(0).toInt();
        person.awardId = people.value(1).toInt();
        person.name = people.value(2).toString();
        person.nameBy = people.value(3).toString();
        person.personal = people.value(4).toString();
        person.personalBy = people.value(5).toString();
        person.awardedFor = people.value(6).toString();
        person.rank = people.value(7).toString();
        person.rankBy = people.value(8).toString();
        person.ukaseNo = people.value(9).toInt();
        person.ukaseDate = QDate::fromString(people.value(10).toString(), Qt::ISODate); // "yyyy-MM-dd"
        person.posthumous = people.value(11).toInt();
        person.language = people.value(12).toInt();
        m_people.push_back(person);
    }

    rebuildIdentity();
    invalidateDerived();
    return true;
}

void Catalog::rebuildDerived() const
{
    m_derivedDirty = false;
    m_stats.clear();
    m_dictionary.clear();
    m_first.clear();
    m_last.clear();
    m_middle.clear();
    m_nameRu.clear();
    m_profRu.clear();
    m_rankRu.clear();

    // одна белорусская должность → русское написание → как часто, и какой указ победит при равенстве
    struct ProfSpelling {
        int count = 0;
        QDate earliest;
        int order = 0;
    };
    QHash<QString, QHash<QString, ProfSpelling>> professions;

    QHash<QString, QHash<QString, int>> fullNames;
    QHash<QPair<int, int>, int> statIndex;
    for (int index = 0; index < m_people.size(); ++index) {
        const Person& person = m_people.at(index);
        const QString nameBy = belarusianNameKey(person.nameBy);
        const QString name = person.name.simplified();
        if (!nameBy.isEmpty() && !name.isEmpty())
            ++fullNames[nameBy][name];
        if (!person.ukaseDate.isValid())
            continue;
        const QPair<int, int> key(person.awardId, person.ukaseDate.year());
        auto it = statIndex.find(key);
        if (it == statIndex.end()) {
            StatMonth row;
            row.awardId = person.awardId;
            row.year = person.ukaseDate.year();
            it = statIndex.insert(key, m_stats.size());
            m_stats.push_back(row);
        }
        StatMonth& row = m_stats[*it];
        row.total++;
        const int month = person.ukaseDate.month();
        if (month >= 1 && month <= 12)
            row.month[month - 1]++;

        if (!person.personalBy.isEmpty() && !person.personal.isEmpty()) {
            ProfSpelling& slot = professions[person.personalBy][person.personal];
            if (slot.count == 0 || person.ukaseDate < slot.earliest) {
                slot.earliest = person.ukaseDate;
                slot.order = index;
            }
            ++slot.count;
        }
    }
    for (auto group = professions.cbegin(); group != professions.cend(); ++group) {
        QString bestRu;
        ProfSpelling best;
        bool chosen = false;
        for (auto spelling = group.value().cbegin(); spelling != group.value().cend(); ++spelling) {
            const ProfSpelling& item = spelling.value();
            const bool better = !chosen
                || item.count > best.count
                || (item.count == best.count && item.earliest < best.earliest)
                || (item.count == best.count && item.earliest == best.earliest && item.order < best.order);
            if (better) {
                chosen = true;
                bestRu = spelling.key();
                best = item;
            }
        }
        if (chosen)
            m_profRu.insert(group.key(), bestRu);
    }
    for (auto group = fullNames.cbegin(); group != fullNames.cend(); ++group) {
        QVector<QPair<int, QString>> ranked;
        ranked.reserve(group.value().size());
        for (auto spelling = group.value().cbegin(); spelling != group.value().cend(); ++spelling)
            ranked.push_back({spelling.value(), spelling.key()});
        std::sort(ranked.begin(), ranked.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
            if (a.first != b.first)
                return a.first > b.first;
            return a.second < b.second;
        });
        QStringList names;
        names.reserve(ranked.size());
        for (const QPair<int, QString>& item : ranked)
            names.push_back(item.second);
        m_nameRu.insert(group.key(), names);
    }

    QVector<int> byName(m_people.size());
    for (int i = 0; i < byName.size(); ++i)
        byName[i] = i;
    std::sort(byName.begin(), byName.end(), [this](int a, int b) {
        const int cmp = QString::compare(m_people.at(a).name, m_people.at(b).name);
        if (cmp == 0)
            return m_people.at(a).ukaseDate < m_people.at(b).ukaseDate;
        return cmp < 0;
    });

    // одно белорусское звание → русское написание → как часто, и где это написание впервые стоит в byName
    struct RankSpelling {
        int count = 0;
        int sortPos = 0;
    };
    QHash<QString, QHash<QString, RankSpelling>> ranks;
    QHash<QString, QHash<QString, int>> firstCounts, lastCounts, middleCounts;
    for (int order = 0; order < byName.size(); ++order) {
        const Person& person = m_people.at(byName.at(order));
        if (!person.rankBy.isEmpty() && !person.rank.isEmpty()) {
            RankSpelling& slot = ranks[person.rankBy][person.rank];
            if (slot.count == 0)
                slot.sortPos = order;
            ++slot.count;
        }

        if (person.name.isEmpty() || person.nameBy.isEmpty())
            continue;
        if (person.nameBy.count(QLatin1Char(' ')) != 2 || person.name.count(QLatin1Char(' ')) != 2)
            continue;
        const int by1 = person.nameBy.indexOf(QLatin1Char(' '));
        const int by2 = person.nameBy.indexOf(QLatin1Char(' '), by1 + 1);
        const int ru1 = person.name.indexOf(QLatin1Char(' '));
        const int ru2 = person.name.indexOf(QLatin1Char(' '), ru1 + 1);
        const auto addPart = [](QHash<QString, QStringList>& dict, QHash<QString, QHash<QString, int>>& counts,
                                 const QString& by, const QString& ru) {
            addDict(dict, by, ru);
            ++counts[by][ru];
        };
        addPart(m_first, firstCounts, belarusianNameKey(person.nameBy.mid(by1 + 1, by2 - by1 - 1)), person.name.mid(ru1 + 1, ru2 - ru1 - 1));
        addPart(m_last, lastCounts, belarusianNameKey(person.nameBy.left(by1)), person.name.left(ru1));
        addPart(m_middle, middleCounts, belarusianNameKey(person.nameBy.mid(by2 + 1)), person.name.mid(ru2 + 1));
    }
    rankDict(m_first, firstCounts);
    rankDict(m_last, lastCounts);
    rankDict(m_middle, middleCounts);
    for (auto group = ranks.cbegin(); group != ranks.cend(); ++group) {
        QString bestRu;
        RankSpelling best;
        bool chosen = false;
        for (auto spelling = group.value().cbegin(); spelling != group.value().cend(); ++spelling) {
            const RankSpelling& item = spelling.value();
            const bool better = !chosen
                || item.count > best.count
                || (item.count == best.count && item.sortPos < best.sortPos);
            if (better) {
                chosen = true;
                bestRu = spelling.key();
                best = item;
            }
        }
        if (chosen)
            m_rankRu.insert(group.key(), bestRu);
    }

    auto appendDict = [this](const QHash<QString, QStringList>& dict, const QString& type) {
        QStringList keys = dict.keys();
        keys.sort(Qt::CaseInsensitive);
        for (const QString& key : keys) {
            const QStringList values = dict.value(key);
            DictEntry entry;
            entry.type = type;
            entry.by = key;
            entry.ru = values.value(0);
            entry.alt = joinAlt(values);
            m_dictionary.push_back(entry);
        }
    };
    appendDict(m_last, QStringLiteral("Фамилия"));
    appendDict(m_first, QStringLiteral("Имя"));
    appendDict(m_middle, QStringLiteral("Отчество"));

    std::sort(m_stats.begin(), m_stats.end(), [this](const StatMonth& a, const StatMonth& b) {
        if (const int cmp = compareAwards(a.awardId, b.awardId))
            return cmp < 0;
        return a.year < b.year;
    });
}

void Catalog::rebuildIdentity()
{
    m_personById.clear();
    m_identityName.clear();
    m_identityNameBy.clear();
    m_personById.reserve(m_people.size());
    m_identityName.reserve(m_people.size());
    m_identityNameBy.reserve(m_people.size());
    for (int index = 0; index < m_people.size(); ++index) {
        const Person& person = m_people.at(index);
        m_personById.insert(person.id, index);
        addIdentity(person);
    }
}

namespace {

// имена сравниваются без латинских букв-двойников («Mixaіл» == «Міхаіл»)
QString keyName(const QString& name, bool belarusian)
{
    return NormalizeCyrillicText(name, belarusian);
}

QString identityText(const QString& name, const QString& personal)
{
    return name.isEmpty() ? personal : name;
}

} // namespace

// счётчики, не множество: у двух записей может быть один ключ, правка одной должна оставить другую
void Catalog::addIdentity(const Person& person)
{
    const qint64 day = person.ukaseDate.toJulianDay();
    const QString russian = identityText(person.name, person.personal);
    const QString belarusian = identityText(person.nameBy, person.personalBy);
    if (!russian.isEmpty())
        ++m_identityName[IdentityKey{person.ukaseNo, day, person.awardId, keyName(russian, false)}];
    if (!belarusian.isEmpty())
        ++m_identityNameBy[IdentityKey{person.ukaseNo, day, person.awardId, keyName(belarusian, true)}];
}

void Catalog::removeIdentity(const Person& person)
{
    const qint64 day = person.ukaseDate.toJulianDay();
    const auto drop = [](IdentityCount& counts, IdentityKey key) {
        auto it = counts.find(key);
        if (it != counts.end() && --it.value() <= 0)
            counts.erase(it);
    };
    const QString russian = identityText(person.name, person.personal);
    const QString belarusian = identityText(person.nameBy, person.personalBy);
    if (!russian.isEmpty())
        drop(m_identityName, IdentityKey{person.ukaseNo, day, person.awardId, keyName(russian, false)});
    if (!belarusian.isEmpty())
        drop(m_identityNameBy, IdentityKey{person.ukaseNo, day, person.awardId, keyName(belarusian, true)});
}

int Catalog::count(const Person& probe) const
{
    const qint64 day = probe.ukaseDate.toJulianDay();
    int result = 0;
    const QString russian = identityText(probe.name, probe.personal);
    const QString belarusian = identityText(probe.nameBy, probe.personalBy);
    if (!russian.isEmpty())
        result = m_identityName.value(IdentityKey{probe.ukaseNo, day, probe.awardId, keyName(russian, false)});
    if (!belarusian.isEmpty()) {
        result = std::max(result, m_identityNameBy.value(
            IdentityKey{probe.ukaseNo, day, probe.awardId, keyName(belarusian, true)}));
    }
    return result;
}

// латинские буквы-двойники в каждом текстовом поле; белорусская «і» только в
// белорусских полях («Награжден за» на языке указа)
bool Catalog::normalizeTextFields(Person& person)
{
    bool changed = false;
    const auto fix = [&changed](QString& text, bool belarusian) {
        const QString fixed = NormalizeCyrillicText(text, belarusian);
        if (fixed != text) {
            text = fixed;
            changed = true;
        }
    };
    fix(person.nameBy, true);
    fix(person.personalBy, true);
    fix(person.rankBy, true);
    fix(person.name, false);
    fix(person.personal, false);
    fix(person.rank, false);
    fix(person.awardedFor, person.language == kLangBelarusian);
    return changed;
}

Catalog::LetterFix Catalog::findLatinLetters(QVector<Person>* fixed, int maxSamples) const
{
    LetterFix result;
    for (const Person& person : m_people) {
        Person copy = person;
        if (!normalizeTextFields(copy))
            continue;
        ++result.records;
        const auto count = [&](const QString& caption, const QString& before, const QString& after) {
            if (before == after)
                return;
            ++result.fields[caption];
            if (result.samples.size() < maxSamples)
                result.samples.push_back(before + QStringLiteral("  →  ") + after);
        };
        count(QStringLiteral("ФИО (белор.)"), person.nameBy, copy.nameBy);
        count(QStringLiteral("Личные данные (белор.)"), person.personalBy, copy.personalBy);
        count(QStringLiteral("Звание (белор.)"), person.rankBy, copy.rankBy);
        count(QStringLiteral("ФИО"), person.name, copy.name);
        count(QStringLiteral("Личные данные"), person.personal, copy.personal);
        count(QStringLiteral("Звание"), person.rank, copy.rank);
        count(QStringLiteral("Награжден за"), person.awardedFor, copy.awardedFor);
        if (fixed)
            fixed->push_back(copy);
    }
    return result;
}

bool Catalog::backup(const QString& filePath, QString* error) const
{
    if (QFile::exists(filePath)) {
        if (error)
            *error = QStringLiteral("Файл уже существует: %1").arg(filePath);
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(filePath);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Catalog::writePerson(Person& person, QString* error)
{
    if (!person.personalBy.isEmpty())
        person.language = kLangBelarusian;
    normalizeTextFields(person);

    QSqlQuery query(m_db);
    const bool update = person.id > 0;
    if (update) {
        query.prepare(QStringLiteral(
            "UPDATE Kavalers SET Award_ID=?, Ukase_Date=?, Ukase_No=?, Ukase_Lang=?, Posthumous=?, "
            "Name=?, Name_BY=?, Awarded_For=?, Personal_Info=?, Personal_Info_BY=?, Rank=?, Rank_BY=? "
            "WHERE ID=?"));
    } else {
        query.prepare(QStringLiteral(
            "INSERT INTO Kavalers (Award_ID, Name, Name_BY, Personal_Info, Personal_Info_BY, "
            "Awarded_For, Rank, Rank_BY, Ukase_No, Ukase_Date, Posthumous, Ukase_Lang) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    }

    if (update) {
        query.addBindValue(person.awardId);
        if (person.ukaseDate.isValid())
            query.addBindValue(person.ukaseDate.toString(QStringLiteral("yyyy-MM-dd")));
        else
            query.addBindValue(QVariant());
        query.addBindValue(person.ukaseNo);
        query.addBindValue(person.language);
        query.addBindValue(person.posthumous);
        bindText(query, person.name);
        bindText(query, person.nameBy);
        bindText(query, person.awardedFor);
        bindText(query, person.personal);
        bindText(query, person.personalBy);
        bindText(query, person.rank);
        bindText(query, person.rankBy);
        query.addBindValue(person.id);
    } else {
        query.addBindValue(person.awardId);
        bindText(query, person.name);
        bindText(query, person.nameBy);
        bindText(query, person.personal);
        bindText(query, person.personalBy);
        bindText(query, person.awardedFor);
        bindText(query, person.rank);
        bindText(query, person.rankBy);
        query.addBindValue(person.ukaseNo);
        if (person.ukaseDate.isValid())
            query.addBindValue(person.ukaseDate.toString(QStringLiteral("yyyy-MM-dd")));
        else
            query.addBindValue(QVariant());
        query.addBindValue(person.posthumous);
        query.addBindValue(person.language);
    }

    if (!exec(query, error))
        return false;
    // запись, удалённая тем временем (или так и не сохранённая), не должна вернуться в память
    if (update && query.numRowsAffected() != 1) {
        if (error)
            *error = QStringLiteral("Запись %1 не найдена в базе: возможно, она была удалена.").arg(person.id);
        return false;
    }
    if (!update)
        person.id = query.lastInsertId().toInt();
    return true;
}

void Catalog::storePerson(const Person& person)
{
    const auto it = m_personById.constFind(person.id);
    if (it != m_personById.cend()) {
        removeIdentity(m_people.at(*it));
        m_people[*it] = person;
    } else {
        m_personById.insert(person.id, m_people.size());
        m_people.push_back(person);
    }
    addIdentity(person);
}

bool Catalog::save(Person& person, QString* error)
{
    if (!writePerson(person, error))
        return false;
    emit aboutToChange();
    storePerson(person);
    invalidateDerived();
    emit dataChanged();
    return true;
}

bool Catalog::saveMany(QVector<Person>& people, QString* error)
{
    if (people.isEmpty())
        return true;
    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    // номера новых записей назначает INSERT; откат должен их забрать
    QVector<int> originalIds;
    originalIds.reserve(people.size());
    for (const Person& person : people)
        originalIds.push_back(person.id);
    const auto rollback = [&] {
        m_db.rollback();
        for (int i = 0; i < people.size(); ++i)
            people[i].id = originalIds.at(i);
    };
    for (Person& person : people) {
        if (!writePerson(person, error)) {
            rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        rollback();
        return false;
    }
    emit aboutToChange();
    for (const Person& person : people)
        storePerson(person);
    invalidateDerived();
    emit dataChanged();
    return true;
}

bool Catalog::remove(const QList<int>& ids, QString* error)
{
    if (ids.isEmpty())
        return true;

    // один подготовленный запрос на номер в одной транзакции: список «IN (?, ...)»
    // ограничен числом параметров SQL (32766)
    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM Kavalers WHERE ID = ?"));
    for (int id : ids) {
        query.bindValue(0, id);
        if (!exec(query, error)) {
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    emit aboutToChange();
    const QSet<int> drop(ids.begin(), ids.end());
    // счётчики личности не зависят от позиций: их покидают только удалённые
    // (без 156 тысяч нормализаций имён); позиции по номеру перенумеровываются
    for (const Person& person : m_people) {
        if (drop.contains(person.id))
            removeIdentity(person);
    }
    m_people.removeIf([&drop](const Person& person) { return drop.contains(person.id); });
    m_personById.clear();
    m_personById.reserve(m_people.size());
    for (int index = 0; index < m_people.size(); ++index)
        m_personById.insert(m_people.at(index).id, index);
    invalidateDerived();
    emit dataChanged();
    return true;
}

QStringList Catalog::translateName(const QString& nameBy) const
{
    ensureDerived();
    const QString key = belarusianNameKey(nameBy);
    const auto exact = m_nameRu.constFind(key);
    if (exact != m_nameRu.cend())
        return exact.value();

    // новое полное имя составляется из частей; у части может быть несколько
    // написаний, тогда вариантов несколько: первым — из самых частых написаний
    const QStringList parts = key.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != 3)
        return {};
    const QStringList last = m_last.value(parts.at(0)).mid(0, kNameVariantsPerPart);
    const QStringList first = m_first.value(parts.at(1)).mid(0, kNameVariantsPerPart);
    const QStringList middle = m_middle.value(parts.at(2)).mid(0, kNameVariantsPerPart);
    if (first.isEmpty() || last.isEmpty() || middle.isEmpty())
        return {};
    struct Variant {
        int score;
        QString name;
    };
    QVector<Variant> variants;
    for (int l = 0; l < last.size(); ++l) {
        for (int f = 0; f < first.size(); ++f) {
            for (int m = 0; m < middle.size(); ++m)
                variants.push_back({l + f + m, last.at(l) + QLatin1Char(' ') + first.at(f) + QLatin1Char(' ') + middle.at(m)});
        }
    }
    std::stable_sort(variants.begin(), variants.end(), [](const Variant& a, const Variant& b) { return a.score < b.score; });
    QStringList names;
    for (const Variant& variant : variants.mid(0, kNameVariants))
        names.push_back(variant.name);
    return names;
}

QVector<Catalog::NameConflict> Catalog::russianNameConflicts() const
{
    QHash<QString, QHash<QString, int>> spellings;
    for (const Person& person : m_people) {
        const QString nameBy = person.nameBy.simplified();
        const QString name = person.name.simplified();
        if (nameBy.isEmpty() || name.isEmpty())
            continue;
        ++spellings[nameBy][name];
    }

    QVector<NameConflict> conflicts;
    for (auto group = spellings.cbegin(); group != spellings.cend(); ++group) {
        if (group.value().size() < 2)
            continue;
        NameConflict conflict;
        conflict.nameBy = group.key();
        for (auto spelling = group.value().cbegin(); spelling != group.value().cend(); ++spelling) {
            conflict.rows += spelling.value();
            conflict.names.push_back(NameVariant{spelling.key(), spelling.value()});
        }
        std::sort(conflict.names.begin(), conflict.names.end(), [](const NameVariant& a, const NameVariant& b) {
            if (a.count != b.count)
                return a.count > b.count;
            return a.name < b.name;
        });
        conflicts.push_back(conflict);
    }
    std::sort(conflicts.begin(), conflicts.end(), [](const NameConflict& a, const NameConflict& b) {
        if (a.rows != b.rows)
            return a.rows > b.rows;
        return a.nameBy < b.nameBy;
    });
    return conflicts;
}

QString Catalog::professionRu(const QString& professionBy) const
{
    ensureDerived();
    return m_profRu.value(professionBy);
}

QString Catalog::rankRu(const QString& rankBy) const
{
    ensureDerived();
    return m_rankRu.value(rankBy);
}

bool Catalog::fixQuotes(Person& person)
{
    if (findAny(person.personalBy, 0, QStringLiteral("-–«»\"")) < 0)
        return false;

    QString& text = person.personal;
    bool changed = false;
    const QString dashes = QStringLiteral("-–");
    // переводчик пишет ", “ ” или „ там, где в указе « »
    const QString quotesBySet = QStringLiteral("«»\"");
    const QString quotesRuSet = QStringLiteral("«»\"“”„");
    const QVector<QChar> dashBy = collectChars(person.personalBy, dashes);
    const QVector<QChar> dashRu = collectChars(text, dashes);

    if (!dashBy.isEmpty() && dashBy.size() == dashRu.size()) {
        int pos = -1;
        for (int i = 0; i < dashBy.size(); ++i) {
            pos = findAny(text, pos + 1, dashes);
            if (pos < 0)
                break;
            if (text.at(pos) != dashBy.at(i)) {
                text[pos] = dashBy.at(i);
                changed = true;
            }
            if (text.at(pos) == QChar(0x2013)) {
                if (pos > 0 && text.at(pos - 1) != QLatin1Char(' ')) {
                    text.insert(pos, QLatin1Char(' '));
                    ++pos;
                    changed = true;
                }
                if (pos + 1 < text.size() && text.at(pos + 1) != QLatin1Char(' ')) {
                    text.insert(pos + 1, QLatin1Char(' '));
                    ++pos;
                    changed = true;
                }
            }
        }
    }

    const QVector<QChar> quotesBy = collectChars(person.personalBy, quotesBySet);
    const QVector<QChar> quotesRu = collectChars(text, quotesRuSet);
    if (!quotesBy.isEmpty() && quotesBy.size() == quotesRu.size()) {
        int pos = -1;
        for (int i = 0; i < quotesBy.size(); ++i) {
            pos = findAny(text, pos + 1, quotesRuSet);
            if (pos < 0)
                break;
            if (text.at(pos) != quotesBy.at(i)) {
                text[pos] = quotesBy.at(i);
                changed = true;
            }
            if (text.at(pos) == QChar(0x00AB)) {
                if (pos + 1 < text.size() && text.at(pos + 1) == QLatin1Char(' ')) {
                    text.remove(pos + 1, 1);
                    changed = true;
                }
                if (pos > 0 && text.at(pos - 1) != QLatin1Char(' ')) {
                    text.insert(pos, QLatin1Char(' '));
                    ++pos;
                    changed = true;
                }
            }
            if (pos >= 0 && pos < text.size() && text.at(pos) == QChar(0x00BB)) {
                if (pos + 1 < text.size() && isAlphaOrDigit(text.at(pos + 1))) {
                    text.insert(pos + 1, QLatin1Char(' '));
                    ++pos;
                    changed = true;
                }
                if (pos > 0 && text.at(pos - 1) == QLatin1Char(' ')) {
                    text.remove(pos - 1, 1);
                    --pos;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

int Catalog::exportCsv(const QString& filePath, bool fromDate, const QDate& from, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return -1;
    }

    QVector<const Person*> rows;
    rows.reserve(m_people.size());
    for (const Person& person : m_people)
        rows.push_back(&person);
    std::sort(rows.begin(), rows.end(), [](const Person* a, const Person* b) { return a->id < b->id; });

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    int count = 0;
    for (const Person* person : rows) {
        if (fromDate && person->ukaseDate.isValid() && person->ukaseDate < from)
            continue;
        if (person->id <= 0) {
            if (error)
                *error = QStringLiteral("Запись еще не сохранена: %1").arg(person->personal);
            return -1;
        }
        if (person->personal.contains(QLatin1Char('"')) || person->personalBy.contains(QLatin1Char('"'))) {
            if (error)
                *error = person->personal.contains(QLatin1Char('"')) ? person->personal : person->personalBy;
            return -1;
        }

        const QString date = person->ukaseDate.isValid()
            ? person->ukaseDate.toString(QStringLiteral("yyyy.MM.dd")) + QStringLiteral(" 00:00:00")
            : QString();
        auto cell = [](QString text) {
            text.replace(QLatin1Char('\n'), QLatin1Char(' '));
            text.replace(QLatin1Char('\r'), QLatin1Char(' '));
            return text;
        };
        out << person->id << '\t' << person->awardId << '\t'
            << '"' << cell(person->name) << "\"\t"
            << '"' << cell(person->nameBy) << "\"\t"
            << '"' << cell(person->personal) << "\"\t"
            << '"' << cell(person->personalBy) << "\"\t"
            << '"' << cell(person->rank) << "\"\t"
            << '"' << cell(person->rankBy) << "\"\t"
            << date << '\t' << person->ukaseNo << '\t' << (person->posthumous ? 1 : 0) << '\t'
            << '"' << cell(person->awardedFor) << "\"\t" << person->language << '\n';
        ++count;
    }
    return count;
}

// текст поля после замены; ложь, если ничего не совпало. Точное: всё
// поле равно тексту; иначе заменяется первое вхождение
static bool replacedText(const QString& text, bool exact, const QString& find, const QString& replace, QString* result)
{
    if (exact) {
        if (text != find)
            return false;
        if (result)
            *result = replace;
        return true;
    }
    const int pos = text.indexOf(find);
    if (pos < 0)
        return false;
    if (result)
        *result = QString(text).replace(pos, find.size(), replace);
    return true;
}

int Catalog::countMatches(TextField field, bool exact, const QString& find) const
{
    if (find.isEmpty())
        return 0;
    int count = 0;
    for (const Person& person : m_people) {
        const QString* text = person.field(field);
        if (text && replacedText(*text, exact, find, QString(), nullptr))
            ++count;
    }
    return count;
}

// сохраняется как любая правка (saveMany): одна транзакция, нормализация текста, личность
int Catalog::replaceText(TextField field, bool exact, const QString& find, const QString& replace, QString* error)
{
    if (find.isEmpty())
        return 0;
    QVector<Person> changed;
    for (const Person& person : m_people) {
        const QString* text = person.field(field);
        QString updated;
        if (!text || !replacedText(*text, exact, find, replace, &updated))
            continue;
        Person copy = person;
        *copy.field(field) = updated;
        changed.push_back(copy);
    }
    if (!saveMany(changed, error))
        return -1;
    return changed.size();
}
