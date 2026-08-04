#include "mainwindow.h"
#include <QDebug>
#include <QHeaderView>
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
    model = new CredentialModel(this);
    connectToDevice();
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::onDataReceived);
    connect(protocol, &CerberusProtocol::metadataReceived, this, &MainWindow::onMetadataReceived);
    connect(protocol, &CerberusProtocol::metadataComplete, this, &MainWindow::onMetadataComplete);
    connect(protocol, &CerberusProtocol::frameReceived, this, &MainWindow::handleFrame);
    connect(protocol, &CerberusProtocol::bytesToSend, this, [this](QByteArray b) {
        serial->write(b);
    });
    connect(protocol, &CerberusProtocol::protocolError, this, [this] {
        ui->deviceStatus->setText("Protocol error");
    });
    ui->credentialList->setModel(model);
    QHeaderView *header = ui->credentialList->horizontalHeader();
    header->QHeaderView::setSectionResizeMode(CredentialModel::Col_Fav, QHeaderView::Fixed);
    header->QHeaderView::setSectionResizeMode(CredentialModel::Col_Site, QHeaderView::Stretch);
    header->QHeaderView::setSectionResizeMode(CredentialModel::Col_Accessed,
                                              QHeaderView::ResizeToContents);
    header->resizeSection(CredentialModel::Col_Fav, 32);
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
    model->clear();
    protocol->sendCommand(CMD_UNLOCK, payload);
    ui->deviceStatus->setText("Sent: PING");
}

void MainWindow::onDataReceived()
{
    protocol->feedBytes(serial->readAll());
}

void MainWindow::onMetadataReceived(credential metadata)
{
    model->append(metadata);
    ui->countLabel->setText(QString::number(model->rowCount()));
}

void MainWindow::onMetadataComplete()
{
    ui->deviceStatus->setText("METADATA: COMPLETED");
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
