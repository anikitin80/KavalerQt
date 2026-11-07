#pragma once

#include "Catalog.h"

#include <QString>
#include <QUrl>
#include <QVector>

#include <functional>

enum class CloudScope
{
    All,
    NameOnly
};

// ключ из KAVALER_YANDEX_API_KEY или QSettings «YandexApiKey» (без приставки «Api-Key »)
QString yandexApiKey();
void setYandexApiKey(const QString& key);

// Переводит белорусские поля людей на месте. Несколько человек идут в одном
// запросе (до kCloudRequestChars символов) по одному соединению; человек
// между запросами не делится. progress(done) вызывается после каждого запроса
// с числом уже обработанных людей; false останавливает перед следующим.
// canceled проверяется, пока запрос в пути; true обрывает этот ответ.
// Возвращает, сколько людей с начала обработано: меньше people.size()
// после ошибки (error заполнен) или остановки. Уже заполненные поля остаются, если не задан force.
int translateWithCloud(QVector<Person>& people, CloudScope scope, bool force, QString* error,
                       const std::function<bool(int done)>& progress = {},
                       const std::function<bool()>& canceled = {});
bool translateWithCloud(Person& person, CloudScope scope, bool force, QString* error);

// части описанного выше, открытые для тестов
namespace cloud {

// Яндекс принимает до 10 000 символов на запрос
constexpr int kCloudRequestChars = 9000;

// сколько символов текст занимает в теле JSON: каждая «"» и «\» экранируется обратной косой
int requestLength(const QString& text);

// люди (по длине их текста в JSON), собранные в запросы: каждая группа — диапазон
// [first, last) подряд идущих людей, чьи тексты вместе влезают в limit. Человек
// длиннее limit получает свой запрос. lengths — суммы requestLength,
// поэтому запас в 1 000 символов под 10 000 Яндекса держится и для кавычек.
QVector<QPair<int, int>> groupRequests(const QVector<int>& lengths, int limit);

int translate(QVector<Person>& people, CloudScope scope, bool force, QString* error,
              const std::function<bool(int done)>& progress, const QUrl& endpoint, const QString& apiKey,
              const std::function<bool()>& canceled = {});

} // namespace cloud
