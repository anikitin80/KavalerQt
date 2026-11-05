#pragma once

#include "Catalog.h"

#include <QString>

enum class CloudScope
{
    All,
    NameOnly
};

// key from KAVALER_YANDEX_API_KEY or QSettings "YandexApiKey" (without "Api-Key " prefix)
QString yandexApiKey();
void setYandexApiKey(const QString& key);

bool translateWithCloud(Person& person, CloudScope scope, bool force, QString* error);
