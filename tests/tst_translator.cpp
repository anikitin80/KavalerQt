// Yandex translation against a local stand-in server: people are grouped into
// requests, one connection serves every request, a failed or stopped run keeps
// the people translated before it.

#include "Translator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

namespace {

// HTTP/1.1 with keep-alive; every text comes back as "ru:" + text.
// failAt: the request with this number (from 1) gets HTTP 500.
class FakeYandex : public QObject
{
public:
    explicit FakeYandex(int failAt = 0) : m_failAt(failAt)
    {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                ++connections;
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { serve(socket); });
            }
        });
    }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/translate").arg(m_server.serverPort())); }

    int connections = 0;
    QVector<QStringList> requests;
    QByteArray authorization;

private:
    void serve(QTcpSocket* socket)
    {
        QByteArray& buffer = m_buffers[socket];
        buffer += socket->readAll();
        for (;;) {
            const int headerEnd = int(buffer.indexOf("\r\n\r\n"));
            if (headerEnd < 0)
                return;
            int length = 0;
            for (const QByteArray& line : buffer.left(headerEnd).split('\n')) {
                const QByteArray lower = line.trimmed().toLower();
                if (lower.startsWith("content-length:"))
                    length = lower.mid(15).trimmed().toInt();
                if (lower.startsWith("authorization:"))
                    authorization = line.trimmed().mid(14).trimmed();
            }
            if (buffer.size() < headerEnd + 4 + length)
                return;
            const QByteArray body = buffer.mid(headerEnd + 4, length);
            buffer.remove(0, headerEnd + 4 + length);

            QStringList texts;
            QJsonArray translations;
            for (const QJsonValue& text : QJsonDocument::fromJson(body).object().value(QStringLiteral("texts")).toArray()) {
                texts << text.toString();
                translations.append(QJsonObject{{QStringLiteral("text"), QStringLiteral("ru:") + text.toString()}});
            }
            requests << texts;
            const bool fail = int(requests.size()) == m_failAt;
            const QByteArray answer = fail
                ? QByteArray(R"({"message":"quota exceeded"})")
                : QJsonDocument(QJsonObject{{QStringLiteral("translations"), translations}}).toJson(QJsonDocument::Compact);
            socket->write((fail ? "HTTP/1.1 500 Internal Server Error\r\n" : "HTTP/1.1 200 OK\r\n")
                + QByteArray("Content-Type: application/json\r\nConnection: keep-alive\r\nContent-Length: ")
                + QByteArray::number(answer.size()) + "\r\n\r\n" + answer);
        }
    }

    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    int m_failAt = 0;
};

Person belarusian(const QString& name, const QString& personal = {}, const QString& rank = {})
{
    Person person;
    person.nameBy = name;
    person.personalBy = personal;
    person.rankBy = rank;
    return person;
}

using Groups = QVector<QPair<int, int>>;

} // namespace

class TranslatorTest : public QObject
{
    Q_OBJECT

private slots:
    void groupsFitTheLimit()
    {
        QCOMPARE(cloud::groupRequests({}, 10), Groups());
        QCOMPARE(cloud::groupRequests({3, 3, 3, 3}, 10), Groups({{0, 3}, {3, 4}}));
        QCOMPARE(cloud::groupRequests({5, 5, 5}, 10), Groups({{0, 2}, {2, 3}}));
        // a person longer than the limit is sent alone, not split
        QCOMPARE(cloud::groupRequests({2, 25, 2}, 10), Groups({{0, 1}, {1, 2}, {2, 3}}));
        // people with nothing to translate join any group
        QCOMPARE(cloud::groupRequests({0, 10, 0, 0}, 10), Groups({{0, 4}}));
    }

    void severalPeopleInOneRequestOverOneConnection()
    {
        FakeYandex server;
        QVector<Person> people;
        for (int i = 0; i < 5; ++i)
            people << belarusian(QStringLiteral("Імя %1").arg(i), QStringLiteral("пасада %1").arg(i));
        people[2].name = QStringLiteral("Имя готово"); // kept: not forced
        // long enough to need a second request
        people << belarusian(QString(cloud::kCloudRequestChars - 10, QLatin1Char('a')));

        QString error;
        QVector<int> progress;
        const int done = cloud::translate(people, CloudScope::All, false, &error,
            [&progress](int count) { progress << count; return true; }, server.url(), QStringLiteral("key"));

        QCOMPARE(done, 6);
        QVERIFY(error.isEmpty());
        QCOMPARE(server.requests.size(), 2);
        QCOMPARE(server.requests.at(0).size(), 9); // 5 positions + 4 names
        QCOMPARE(server.connections, 1);
        QCOMPARE(server.authorization, QByteArray("Api-Key key"));
        QCOMPARE(progress, QVector<int>({5, 6}));
        QCOMPARE(people.at(0).name, QStringLiteral("ru:Імя 0"));
        QCOMPARE(people.at(0).personal, QStringLiteral("ru:пасада 0"));
        QCOMPARE(people.at(2).name, QStringLiteral("Имя готово"));
        QCOMPARE(people.at(2).personal, QStringLiteral("ru:пасада 2"));
    }

    void nameOnlyAndForce()
    {
        FakeYandex server;
        Person person = belarusian(QStringLiteral("Іван"), QStringLiteral("слесар"), QStringLiteral("маёр"));
        person.name = QStringLiteral("Иван");
        QVector<Person> people{person};
        QString error;
        QCOMPARE(cloud::translate(people, CloudScope::NameOnly, true, &error, {}, server.url(), QStringLiteral("k")), 1);
        QCOMPARE(server.requests, QVector<QStringList>({{QStringLiteral("Іван")}}));
        QCOMPARE(people.at(0).name, QStringLiteral("ru:Іван"));
        QVERIFY(people.at(0).personal.isEmpty());
    }

    void nothingToTranslateNeedsNoKeyAndNoRequest()
    {
        FakeYandex server;
        Person person = belarusian(QStringLiteral("Іван"));
        person.name = QStringLiteral("Иван");
        QVector<Person> people{person, Person()};
        QString error;
        QCOMPARE(cloud::translate(people, CloudScope::All, false, &error, {}, server.url(), QString()), 2);
        QVERIFY(error.isEmpty());
        QVERIFY(server.requests.isEmpty());
    }

    void missingKeyIsAnError()
    {
        QVector<Person> people{belarusian(QStringLiteral("Іван"))};
        QString error;
        QCOMPARE(cloud::translate(people, CloudScope::All, false, &error, {}, QUrl(), QString()), 0);
        QVERIFY(error.contains(QStringLiteral("API-ключ")));
        QVERIFY(people.at(0).name.isEmpty());
    }

    void failureKeepsTheEarlierRequests()
    {
        FakeYandex server(2);
        const QString longName(cloud::kCloudRequestChars - 10, QLatin1Char('a'));
        QVector<Person> people{belarusian(longName), belarusian(longName), belarusian(longName)};
        QString error;
        QCOMPARE(cloud::translate(people, CloudScope::All, false, &error, {}, server.url(), QStringLiteral("k")), 1);
        QCOMPARE(error, QStringLiteral("quota exceeded"));
        QVERIFY(people.at(0).name.startsWith(QStringLiteral("ru:")));
        QVERIFY(people.at(1).name.isEmpty());
        QCOMPARE(server.requests.size(), 2);
    }

    void stopAfterTheFirstRequest()
    {
        FakeYandex server;
        const QString longName(cloud::kCloudRequestChars - 10, QLatin1Char('a'));
        QVector<Person> people{belarusian(longName), belarusian(longName)};
        QString error;
        QCOMPARE(cloud::translate(people, CloudScope::All, false, &error, [](int) { return false; }, server.url(),
                                  QStringLiteral("k")), 1);
        QVERIFY(error.isEmpty());
        QCOMPARE(server.requests.size(), 1);
        QVERIFY(people.at(1).name.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TranslatorTest)
#include "tst_translator.moc"
