#include "mainwindow.h"
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QtWidgets>
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
{
    ui->setupUi(this);
    connectToDevice();
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::onDataReceived);
    connect(ui->pressButton, &QPushButton::clicked, this, &MainWindow::on_pressButton_clicked);
}

void MainWindow::connectToDevice()
{
    serial->setPortName("COM9"); // change to your port
    serial->setBaudRate(QSerialPort::Baud115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (serial->open(QIODevice::ReadWrite)) {
        ui->statusLabel->setText("Device connected");
    } else {
        ui->statusLabel->setText("Failed: " + serial->errorString());
    }
}

void MainWindow::on_pressButton_clicked()
{
    if (!serial->isOpen()) {
        ui->statusLabel->setText("Not connected");
        return;
    }
    serial->write("PING\n");
    ui->statusLabel->setText("Sent: PING");
}

void MainWindow::onDataReceived()
{
    QByteArray data = serial->readAll();
    QString response = QString::fromUtf8(data).trimmed();

    if (response == "PONG") {
        ui->statusLabel->setText("Got: PONG - device is alive");
    } else {
        ui->statusLabel->setText("Got: " + response);
    }
}

MainWindow::~MainWindow()
{
    if (serial->isOpen())
        serial->close();
    delete ui;
}
