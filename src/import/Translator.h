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

// key from KAVALER_YANDEX_API_KEY or QSettings "YandexApiKey" (without "Api-Key " prefix)
QString yandexApiKey();
void setYandexApiKey(const QString& key);

// Translates the Belarusian fields of the people in place. Several people go in one
// request (up to kCloudRequestChars characters) over one connection; a person is
// never split between requests. progress(done) is called after each request with
// the number of people processed so far; returning false stops. Returns how many
// people from the start were processed: less than people.size() after an error
// (error is set) or a stop. Fields already filled are kept unless force is set.
int translateWithCloud(QVector<Person>& people, CloudScope scope, bool force, QString* error,
                       const std::function<bool(int done)>& progress = {});
bool translateWithCloud(Person& person, CloudScope scope, bool force, QString* error);

// parts of the above, exposed for the tests
namespace cloud {

// Yandex accepts up to 10 000 characters per request
constexpr int kCloudRequestChars = 9000;

// people (by their total text length) grouped into requests: each group is a range
// [first, last) of consecutive people whose texts fit in limit together. A person
// longer than limit gets a request of its own.
QVector<QPair<int, int>> groupRequests(const QVector<int>& lengths, int limit);

int translate(QVector<Person>& people, CloudScope scope, bool force, QString* error,
              const std::function<bool(int done)>& progress, const QUrl& endpoint, const QString& apiKey);

} // namespace cloud
