// readDocxParagraphs на настоящем указе (tests/data/docx) и весь путь
// docx → абзацы → парсер.

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
}

void TestDocxText::paragraphs()
{
    QString error;
    const QStringList lines = readDocxParagraphs(TestData::path(kDecree), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    // в шапке после «УКАЗ» стоит неразрывный пробел
    QVERIFY(lines.contains(QStringLiteral("УКАЗ ПРЕЗИДЕНТА РЕСПУБЛИКИ БЕЛАРУСЬ")));
    QVERIFY(lines.contains(QStringLiteral("15 мая 1997 г.№ 282")));
    QVERIFY(lines.contains(QStringLiteral("–")));
    for (const QString& line : lines)
        QVERIFY2(!line.isEmpty(), "empty paragraphs are skipped");

    // текст фикстуры tst_decrees — этот вывод; абзац шапки
    // исходного документа содержит CR/LF, текстовая фикстура хранит их отдельными строками
    QStringList asFixture;
    for (QString line : lines)
        asFixture += line.remove(QLatin1Char('\r')).split(QLatin1Char('\n'));
    QCOMPARE(asFixture, TestData::readLines(TestData::path(QStringLiteral("decrees/ru-1997-282-mothers-list.txt"))));
}

// <w:br/> между фамилией и именем не должен рвать абзац:
// парсер берёт имя со строки перед тире
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
