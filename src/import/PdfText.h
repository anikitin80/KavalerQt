#pragma once

#include <QString>
#include <QStringList>

QStringList readPdfLines(const QString& path, QString* error);
