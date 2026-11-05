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

QStringList requestTranslation(const QStringList& texts, QString* error)
{
    const QString apiKey = yandexApiKey();
    if (apiKey.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не задан API-ключ Яндекс.Переводчика.\n"
                                    "Укажите его в меню «Правка → API-ключ Яндекса...» "
                                    "или в переменной окружения %1.").arg(QLatin1String(kApiKeyEnv));
        return {};
    }

    QJsonArray source;
    for (const QString& text : texts)
        source.append(text);

    QJsonObject body;
    body.insert(QStringLiteral("texts"), source);
    body.insert(QStringLiteral("targetLanguageCode"), QStringLiteral("ru"));
    body.insert(QStringLiteral("sourceLanguageCode"), QStringLiteral("be"));

    QNetworkRequest request(QUrl(QStringLiteral("https://translate.api.cloud.yandex.net/translate/v2/translate")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Api-Key " + apiKey.toUtf8());

    QNetworkAccessManager network;
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
    if (result.isEmpty() && error)
        *error = QStringLiteral("Яндекс не вернул перевод.");
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

bool translateWithCloud(Person& person, CloudScope scope, bool force, QString* error)
{
    QStringList texts;
    const bool translateName = !person.nameBy.isEmpty() && (person.name.isEmpty() || force);
    const bool translatePersonal = scope != CloudScope::NameOnly && !person.personalBy.isEmpty()
        && (person.personal.isEmpty() || force);
    const bool translateRank = scope != CloudScope::NameOnly && !person.rankBy.isEmpty()
        && (person.rank.isEmpty() || force);

    if (translateName)
        texts.push_back(person.nameBy);
    if (translatePersonal)
        texts.push_back(person.personalBy);
    if (translateRank)
        texts.push_back(person.rankBy);
    if (texts.isEmpty())
        return true;

    const QStringList translated = requestTranslation(texts, error);
    if (translated.size() < texts.size())
        return false;

    int index = 0;
    if (translateName && !translated.at(index).isEmpty())
        person.name = translated.at(index++);
    else if (translateName)
        ++index;

    if (translatePersonal && index < translated.size() && !translated.at(index).isEmpty()) {
        QString text = translated.at(index++);
        text.replace(QStringLiteral("Д. "), QStringLiteral("д. "));
        text.replace(QStringLiteral("пас. "), QStringLiteral("пос. "));
        text.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        person.personal = text;
    } else if (translatePersonal) {
        ++index;
    }

    if (translateRank && index < translated.size() && !translated.at(index).isEmpty())
        person.rank = translated.at(index);
    return true;
}
