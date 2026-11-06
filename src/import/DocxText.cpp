#include "DocxText.h"

#include <QXmlStreamReader>

// a .docx is a zip archive; QZipReader is Qt's internal zip reader (Qt6::CorePrivate),
// hundreds of times faster than extracting with tar.exe for every file
#include <private/qzipreader_p.h>

QStringList readDocxParagraphs(const QString& path, QString* error)
{
    QZipReader zip(path);
    if (!zip.isReadable() || zip.status() != QZipReader::NoError) {
        if (error)
            *error = QStringLiteral("Не удалось прочитать документ Word.");
        return {};
    }
    const QByteArray xml = zip.fileData(QStringLiteral("word/document.xml"));
    if (xml.isEmpty()) {
        if (error)
            *error = QStringLiteral("В документе Word нет текста.");
        return {};
    }

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
