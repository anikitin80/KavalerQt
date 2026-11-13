#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Anatoli Nikitin"));
    QApplication::setApplicationName(QStringLiteral("Kavaler"));
    QApplication::setApplicationDisplayName(QStringLiteral("Кавалер"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/kavaler.png")));

    MainWindow window;
    window.showMaximized();
    return app.exec();
}
