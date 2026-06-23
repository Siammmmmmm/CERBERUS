#include <QApplication>
#include "mainwindow.h"
#include <qstylehints>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    MainWindow w;
    w.show();
    return QApplication::exec();
}
