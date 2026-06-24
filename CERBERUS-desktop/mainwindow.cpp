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
    connect(ui->addBtn, &QPushButton::clicked, this, &MainWindow::on_addBtn_clicked);
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
        ui->deviceStatus->setText("Device connected");
    } else {
        ui->deviceStatus->setText("Failed: " + serial->errorString());
    }
}

void MainWindow::on_addBtn_clicked()
{
    if (!serial->isOpen()) {
        ui->deviceStatus->setText("Not connected");
        return;
    }
    serial->write("PING\n");
    ui->deviceStatus->setText("Sent: PING");
}

void MainWindow::onDataReceived()
{
    QByteArray data = serial->readAll();
    QString response = QString::fromUtf8(data).trimmed();

    if (response == "PONG") {
        ui->deviceStatus->setText("Got: PONG - device is alive");
    } else {
        ui->deviceStatus->setText("Got: " + response);
    }
}

MainWindow::~MainWindow()
{
    if (serial->isOpen())
        serial->close();
    delete ui;
}
