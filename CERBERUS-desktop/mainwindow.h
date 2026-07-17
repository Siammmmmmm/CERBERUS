#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include "cerberusprotocol.h"
#include <cstdint>
#include <vector>

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
    void on_addBtn_clicked();
    void onDataReceived();
    void handleFrame(uint8_t opcode, QByteArray payload);

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    CerberusProtocol *protocol;
    void connectToDevice();
};

#endif // MAINWINDOW_H
