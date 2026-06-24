#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setPalette(QPalette()); //sets default pallete

    qDebug() << "Current dir:" << QDir::currentPath();
    qDebug() << "Resource exists:" << QFile::exists(":/style.qss");
    qDebug() << "All resources:" << QDir(":/").entryList();
    QFile styleFile(":/style.qss"); //sets custom pallete
    if (styleFile.open(QFile::ReadOnly)) {
        a.setStyleSheet(styleFile.readAll());
        styleFile.close();
    } else {
        qDebug() << "Style file NOT found";
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}
