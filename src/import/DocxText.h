#pragma once

#include <QString>
#include <QStringList>

QStringList readDocxParagraphs(const QString& path, QString* error);
