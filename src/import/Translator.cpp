#include "Translator.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

namespace {

const char kApiKeyEnv[] = "KAVALER_YANDEX_API_KEY";
const char kApiKeySetting[] = "YandexApiKey";

enum class Field
{
    Name,
    Personal,
    Rank
};

// the Belarusian texts of one person that need a translation, in request order
QVector<Field> fieldsToTranslate(const Person& person, CloudScope scope, bool force)
{
    QVector<Field> fields;
    if (!person.nameBy.isEmpty() && (person.name.isEmpty() || force))
        fields.push_back(Field::Name);
    if (scope != CloudScope::NameOnly) {
        if (!person.personalBy.isEmpty() && (person.personal.isEmpty() || force))
            fields.push_back(Field::Personal);
        if (!person.rankBy.isEmpty() && (person.rank.isEmpty() || force))
            fields.push_back(Field::Rank);
    }
    return fields;
}

const QString& sourceText(const Person& person, Field field)
{
    switch (field) {
    case Field::Name:
        return person.nameBy;
    case Field::Personal:
        return person.personalBy;
    case Field::Rank:
        break;
    }
    return person.rankBy;
}

// an empty translation keeps the stored text
void applyTranslation(Person& person, Field field, QString text)
{
    if (text.isEmpty())
        return;
    switch (field) {
    case Field::Name:
        person.name = text;
        break;
    case Field::Personal:
        text.replace(QStringLiteral("Д. "), QStringLiteral("д. "));
        text.replace(QStringLiteral("пас. "), QStringLiteral("пос. "));
        text.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        person.personal = text;
        Catalog::fixQuotes(person);
        break;
    case Field::Rank:
        person.rank = text;
        break;
    }
}

QStringList requestTranslation(QNetworkAccessManager& network, const QUrl& endpoint, const QString& apiKey,
                               const QStringList& texts, QString* error)
{
    QJsonArray source;
    for (const QString& text : texts)
        source.append(text);

    QJsonObject body;
    body.insert(QStringLiteral("texts"), source);
    body.insert(QStringLiteral("targetLanguageCode"), QStringLiteral("ru"));
    body.insert(QStringLiteral("sourceLanguageCode"), QStringLiteral("be"));

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Api-Key " + apiKey.toUtf8());
    request.setTransferTimeout(30000); // no answer in 30 s: OperationCanceledError

    QEventLoop loop;
    QNetworkReply* reply = network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray payload = reply->readAll();
    const auto failure = reply->error();
    const QString failureText = reply->errorString();
    reply->deleteLater();

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (failure != QNetworkReply::NoError) {
        const QString message = document.object().value(QStringLiteral("message")).toString();
        if (error)
            *error = message.isEmpty() ? failureText : message;
        return {};
    }

    QStringList result;
    const QJsonArray translations = document.object().value(QStringLiteral("translations")).toArray();
    for (const QJsonValue& item : translations)
        result.push_back(item.toObject().value(QStringLiteral("text")).toString());
    if (result.size() != texts.size() && error) {
        *error = result.isEmpty() ? QStringLiteral("Яндекс не вернул перевод.")
                                  : QStringLiteral("Яндекс вернул %1 переводов вместо %2.")
                                        .arg(result.size()).arg(texts.size());
    }
    return result;
}

} // namespace

QString yandexApiKey()
{
    QString key = qEnvironmentVariable(kApiKeyEnv).trimmed();
    if (key.isEmpty())
        key = QSettings().value(QLatin1String(kApiKeySetting)).toString().trimmed();
    if (key.startsWith(QLatin1String("Api-Key "), Qt::CaseInsensitive))
        key = key.mid(8).trimmed();
    return key;
}

void setYandexApiKey(const QString& key)
{
    QSettings settings;
    if (key.trimmed().isEmpty())
        settings.remove(QLatin1String(kApiKeySetting));
    else
        settings.setValue(QLatin1String(kApiKeySetting), key.trimmed());
}

QVector<QPair<int, int>> cloud::groupRequests(const QVector<int>& lengths, int limit)
{
    QVector<QPair<int, int>> groups;
    int first = 0;
    int total = 0;
    for (int i = 0; i < lengths.size(); ++i) {
        if (i > first && total + lengths.at(i) > limit) {
            groups.push_back({first, i});
            first = i;
            total = 0;
        }
        total += lengths.at(i);
    }
    if (first < lengths.size())
        groups.push_back({first, int(lengths.size())});
    return groups;
}

int cloud::translate(QVector<Person>& people, CloudScope scope, bool force, QString* error,
                     const std::function<bool(int done)>& progress, const QUrl& endpoint, const QString& apiKey)
{
    QVector<QVector<Field>> fields;
    QVector<int> lengths;
    fields.reserve(people.size());
    lengths.reserve(people.size());
    for (const Person& person : people) {
        fields.push_back(fieldsToTranslate(person, scope, force));
        int length = 0;
        for (Field field : fields.back())
            length += int(sourceText(person, field).size());
        lengths.push_back(length);
    }

    // people with nothing to translate cost nothing and need no request
    bool anything = false;
    for (int length : lengths)
        anything = anything || length > 0;
    if (!anything)
        return int(people.size());
    if (apiKey.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не задан API-ключ Яндекс.Переводчика.\n"
                                    "Укажите его в меню «Правка → API-ключ Яндекса...» "
                                    "или в переменной окружения %1.").arg(QLatin1String(kApiKeyEnv));
        return 0;
    }

    // one manager for every request: the TLS connection to Yandex is reused
    QNetworkAccessManager network;
    for (const auto& [first, last] : groupRequests(lengths, kCloudRequestChars)) {
        QStringList texts;
        for (int i = first; i < last; ++i) {
            for (Field field : fields.at(i))
                texts.push_back(sourceText(people.at(i), field));
        }
        if (!texts.isEmpty()) {
            const QStringList translated = requestTranslation(network, endpoint, apiKey, texts, error);
            if (translated.size() != texts.size())
                return first;
            int index = 0;
            for (int i = first; i < last; ++i) {
                for (Field field : fields.at(i))
                    applyTranslation(people[i], field, translated.at(index++));
            }
        }
        if (progress && !progress(last))
            return last;
    }
    return int(people.size());
}

int translateWithCloud(QVector<Person>& people, CloudScope scope, bool force, QString* error,
                       const std::function<bool(int done)>& progress)
{
    return cloud::translate(people, scope, force, error, progress,
        QUrl(QStringLiteral("https://translate.api.cloud.yandex.net/translate/v2/translate")), yandexApiKey());
}

bool translateWithCloud(Person& person, CloudScope scope, bool force, QString* error)
{
    QVector<Person> people{person};
    if (translateWithCloud(people, scope, force, error) != 1)
        return false;
    person = people.front();
    return true;
}
