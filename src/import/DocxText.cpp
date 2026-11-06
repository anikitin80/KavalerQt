#include "DocxText.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QXmlStreamReader>

QStringList readDocxParagraphs(const QString& path, QString* error)
{
    QTemporaryDir temp;
    if (!temp.isValid()) {
        if (error)
            *error = QStringLiteral("Не удалось создать временную папку.");
        return {};
    }

    QProcess tar;
    tar.start(QStringLiteral("C:/Windows/System32/tar.exe"),
        {QStringLiteral("-xf"), QDir::toNativeSeparators(path), QStringLiteral("-C"), temp.path(),
         QStringLiteral("word/document.xml")});
    if (!tar.waitForStarted(5000) || !tar.waitForFinished(120000) || tar.exitCode() != 0) {
        if (error) {
            const QString details = QString::fromLocal8Bit(tar.readAllStandardError()).trimmed();
            *error = details.isEmpty() ? QStringLiteral("Не удалось прочитать документ Word.") : details;
        }
        return {};
    }

    QFile file(QDir(temp.path()).filePath(QStringLiteral("word/document.xml")));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("В документе Word нет текста.");
        return {};
    }
    const QByteArray xml = file.readAll();

    QStringList lines;
    QXmlStreamReader reader(xml);
    QString paragraph;
    bool inText = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const QString name = reader.name().toString();
            if (name == QLatin1String("p"))
                paragraph.clear();
            else if (name == QLatin1String("t"))
                inText = true;
            else if (name == QLatin1String("tab"))
                paragraph += QLatin1Char('\t');
            else if (name == QLatin1String("br") || name == QLatin1String("cr"))
                // manual line break stays inside the paragraph as '\v', like Word's
                // Range.Text: the parser expects "Surname\vName Patronymic" in one line
                paragraph += QLatin1Char('\v');
        } else if (reader.isCharacters() && inText) {
            paragraph += reader.text();
        } else if (reader.isEndElement()) {
            const QString name = reader.name().toString();
            if (name == QLatin1String("t"))
                inText = false;
            else if (name == QLatin1String("p")) {
                while (paragraph.endsWith(QLatin1Char('\r')) || paragraph.endsWith(QChar(0x0007))
                       || paragraph.endsWith(QLatin1Char('\v')))
                    paragraph.chop(1);
                if (!paragraph.isEmpty())
                    lines.push_back(paragraph);
            }
        }
    }
    if (reader.hasError() && error)
        *error = reader.errorString();
    return lines;
}
