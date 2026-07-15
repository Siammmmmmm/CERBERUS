#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
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

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    std::vector<uint8_t> buffer;
    void connectToDevice();
    bool send_packet(uint8_t opcode, const std::vector<uint8_t> &payload);
    void parse_frame(std::vector<uint8_t> &buffer);
    void discard_n(std::vector<uint8_t> &buffer, size_t n);
    void dispatch(uint8_t opcode, const std::vector<uint8_t> &payload);
};
#endif // MAINWINDOW_H
