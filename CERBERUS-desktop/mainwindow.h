#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pressButton_clicked();
    void onDataReceived();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    void connectToDevice();
};
#endif // MAINWINDOW_H
