#include "mainwindow.h"
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QtWidgets>
#include "opcodes.h"
#include "protocol.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
{
    ui->setupUi(this);
    protocol = new CerberusProtocol(this);
    connectToDevice();
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::onDataReceived);
    connect(protocol, &CerberusProtocol::frameReceived, this, &MainWindow::handleFrame);
    connect(protocol, &CerberusProtocol::bytesToSend, this, [this](QByteArray b) {
        serial->write(b);
    });
    connect(protocol, &CerberusProtocol::protocolError, this, [this] {
        ui->deviceStatus->setText("Protocol error");
    });
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
    std::vector<uint8_t> payload = {};
    protocol->sendCommand(CMD_PING, payload);
    ui->deviceStatus->setText("Sent: PING");
}

void MainWindow::onDataReceived()
{
    protocol->feedBytes(serial->readAll());
}

void MainWindow::handleFrame(uint8_t opcode, QByteArray payload)
{
    switch (opcode) {
    case RES_PONG:
        ui->deviceStatus->setText("Received: PONG");
        break;

    default:
        ui->deviceStatus->setText("NO CMD RECIEVED");

        break;
    }
}

MainWindow::~MainWindow()
{
    if (serial->isOpen())
        serial->close();
    delete ui;
}
