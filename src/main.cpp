#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Anatoli Nikitin"));
    QApplication::setApplicationName(QStringLiteral("Kavaler"));
    QApplication::setApplicationDisplayName(QStringLiteral("Кавалер"));

    MainWindow window;
    window.show();
    return app.exec();
}
