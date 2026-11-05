#include "PdfText.h"

#include <Common/Exception.h>
#include <PDF/PDFDoc.h>
#include <PDF/PDFNet.h>
#include <PDF/TextExtractor.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

bool ensurePdfNet(QString* error)
{
    static bool ready = false;
    static QString failure;
    if (ready)
        return true;
    if (!failure.isEmpty()) {
        if (error)
            *error = failure;
        return false;
    }
    try {
        pdftron::PDFNet::Initialize();
        ready = true;
        return true;
    } catch (pdftron::Common::Exception& exception) {
        failure = QString::fromUtf8(exception.ToString().c_str());
        if (error)
            *error = failure;
        return false;
    }
}

struct PageLine
{
    double x = 0;
    double y = 0;
    QString text;
};

} // namespace

QStringList readPdfLines(const QString& path, QString* error)
{
    if (!ensurePdfNet(error))
        return {};

    try {
        pdftron::PDF::PDFDoc document(pdftron::UString(path.toStdWString()));
        document.InitSecurityHandler();

        QStringList lines;
        for (auto page = document.GetPageIterator(); page.HasNext(); page.Next()) {
            pdftron::PDF::TextExtractor extractor;
            extractor.Begin(page.Current());

            std::vector<PageLine> pageLines;
            for (auto line = extractor.GetFirstLine(); line.IsValid(); line = line.GetNextLine()) {
                QString text;
                double previousRight = 0;
                bool hasWord = false;
                for (auto word = line.GetFirstWord(); word.IsValid(); word = word.GetNextWord()) {
                    const auto bounds = word.GetBBox();
                    pdftron::UString value;
                    value.Assign(word.GetString(), word.GetStringLen());
                    const QString piece = QString::fromUtf16(reinterpret_cast<const char16_t*>(value.CStr()));
                    if (hasWord && bounds.GetX1() - previousRight > 0.5)
                        text += QLatin1Char(' ');
                    text += piece;
                    previousRight = bounds.GetX2();
                    hasWord = true;
                }
                const double* box = line.GetBBox();
                pageLines.push_back({box[0], box[1], text});
            }

            std::sort(pageLines.begin(), pageLines.end(), [](const PageLine& left, const PageLine& right) {
                if (left.y == right.y)
                    return left.x < right.x;
                return left.y > right.y;
            });
            for (const PageLine& line : pageLines)
                lines.push_back(line.text);
        }
        return lines;
    } catch (pdftron::Common::Exception& exception) {
        if (error)
            *error = QString::fromUtf8(exception.ToString().c_str());
        return {};
    }
}
