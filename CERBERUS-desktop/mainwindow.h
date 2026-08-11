#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include "cerberusprotocol.h"
#include "credentialmodel.h"
#include <cstdint>
#include <qsortfilterproxymodel.h>
#include <vector>

class CustomSort : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit CustomSort(QObject *parent = nullptr);
    ~CustomSort() override;

private:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};

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
    void on_syncBtn_clicked();
    void onDataReceived();
    void onPassReceived(QString password);
    void onMetadataReceived(credential metadata);
    void onMetadataComplete();
    void onNackReceived(int reason);
    void handleFrame(uint8_t opcode, QByteArray payload);
    void onSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void on_fetchPasswordBtn_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    CerberusProtocol *protocol;
    CredentialModel *model;
    CustomSort *proxy;
    void connectToDevice();
    void passwordValue(bool reveal);

    int m_selected = -1;
    int m_before = -1;
};

#endif // MAINWINDOW_H
