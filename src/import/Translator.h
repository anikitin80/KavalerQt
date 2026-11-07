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
// the number of people processed so far; returning false stops before the next one.
// canceled is checked while a request is in flight; true aborts that reply.
// Returns how many people from the start were processed: less than people.size()
// after an error (error is set) or a stop. Fields already filled are kept unless force is set.
int translateWithCloud(QVector<Person>& people, CloudScope scope, bool force, QString* error,
                       const std::function<bool(int done)>& progress = {},
                       const std::function<bool()>& canceled = {});
bool translateWithCloud(Person& person, CloudScope scope, bool force, QString* error);

// parts of the above, exposed for the tests
namespace cloud {

// Yandex accepts up to 10 000 characters per request
constexpr int kCloudRequestChars = 9000;

// characters a text takes in the JSON body: each " and \ is escaped with a backslash
int requestLength(const QString& text);

// people (by their JSON text length) grouped into requests: each group is a range
// [first, last) of consecutive people whose texts fit in limit together. A person
// longer than limit gets a request of its own. lengths are requestLength totals,
// so the 1 000 character margin under Yandex's 10 000 still holds for quotes.
QVector<QPair<int, int>> groupRequests(const QVector<int>& lengths, int limit);

int translate(QVector<Person>& people, CloudScope scope, bool force, QString* error,
              const std::function<bool(int done)>& progress, const QUrl& endpoint, const QString& apiKey,
              const std::function<bool()>& canceled = {});

} // namespace cloud
