// readDocxParagraphs on a real decree (tests/data/docx) and the whole path
// docx -> paragraphs -> parser.

#include "DocxText.h"
#include "TestData.h"

#include <QFile>
#include <QtTest>

class TestDocxText : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void paragraphs();
    void manualLineBreakStaysInParagraph();
    void parseDocx();
    void missingFile();
};

namespace {
const QString kDecree = QStringLiteral("docx/ru-1997-282-mothers-list.docx");
}

void TestDocxText::initTestCase()
{
    TestData::isolateSettings();
    if (!QFile::exists(QStringLiteral("C:/Windows/System32/tar.exe")))
        QSKIP("readDocxParagraphs needs tar.exe (Windows 10 and later)");
}

void TestDocxText::paragraphs()
{
    QString error;
    const QStringList lines = readDocxParagraphs(TestData::path(kDecree), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // the header has a non-breaking space after "УКАЗ"
    QVERIFY(lines.contains(QStringLiteral("УКАЗ ПРЕЗИДЕНТА РЕСПУБЛИКИ БЕЛАРУСЬ")));
    QVERIFY(lines.contains(QStringLiteral("15 мая 1997 г.№ 282")));
    QVERIFY(lines.contains(QStringLiteral("–")));
    for (const QString& line : lines)
        QVERIFY2(!line.isEmpty(), "empty paragraphs are skipped");

    // the fixture text used by tst_decrees is this output; the header paragraph of
    // the source document has CR/LF inside, which the text fixture keeps as separate lines
    QStringList asFixture;
    for (QString line : lines)
        asFixture += line.remove(QLatin1Char('\r')).split(QLatin1Char('\n'));
    QCOMPARE(asFixture, TestData::readLines(TestData::path(QStringLiteral("decrees/ru-1997-282-mothers-list.txt"))));
}

// <w:br/> between surname and name must not split the paragraph:
// the parser takes the name from the line before the dash
void TestDocxText::manualLineBreakStaysInParagraph()
{
    const QStringList lines = readDocxParagraphs(TestData::path(kDecree), nullptr);
    const int index = lines.indexOf(QStringLiteral("Бодялову\vМарию Николаевну"));
    QVERIFY(index > 0);
    QCOMPARE(lines.value(index + 1), QStringLiteral("–"));
    QVERIFY(!lines.contains(QStringLiteral("Бодялову")));
}

void TestDocxText::parseDocx()
{
    auto parser = TestData::makeParser();
    std::vector<std::wstring> lines = TestData::toWide(readDocxParagraphs(TestData::path(kDecree), nullptr));
    QVERIFY(parser->ProcessFile(L"ru-1997-282.docx", lines));
    QCOMPARE(int(parser->m_Persons.size()), 25);
    QCOMPARE(QString::fromStdWString(parser->m_Persons.front().Name), QStringLiteral("Бодялова Мария Николаевна"));
}

void TestDocxText::missingFile()
{
    QString error;
    QVERIFY(readDocxParagraphs(TestData::path(QStringLiteral("docx/no-such-file.docx")), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(TestDocxText)
#include "tst_docxtext.moc"
