#include "mainwindow.h"
#include <QDebug>
#include <QHeaderView>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QtWidgets>
#include "opcodes.h"
#include "protocol.h"
#include "ui_mainwindow.h"

CustomSort::CustomSort(QObject *parent)
    : QSortFilterProxyModel(parent)
{}

CustomSort::~CustomSort() {}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serial(new QSerialPort(this))
{
    ui->setupUi(this);
    protocol = new CerberusProtocol(this);
    model = new CredentialModel(this);
    proxy = new CustomSort(this);

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

    proxy->setSourceModel(model);
    ui->credentialList->setModel(proxy);
    ui->credentialList->setSortingEnabled(true);
    QHeaderView *header = ui->credentialList->horizontalHeader();
    header->setSectionResizeMode(CredentialModel::Col_Fav, QHeaderView::Fixed);
    header->setSectionResizeMode(CredentialModel::Col_Site, QHeaderView::Stretch);
    header->setSectionResizeMode(CredentialModel::Col_Accessed, QHeaderView::ResizeToContents);
    header->resizeSection(CredentialModel::Col_Fav, 32);
    proxy->setFilterKeyColumn(CredentialModel::Col_Site);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    connect(ui->searchInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        proxy->setFilterFixedString(text);
    });
}

bool CustomSort::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    switch (left.column()) {
    case (CredentialModel::Col_Fav):
        return (
            ((sourceModel()->data(left, CredentialModel::FlagsRole).toInt() & FLAG_FAVORITE) != 0)
            < ((sourceModel()->data(right, CredentialModel::FlagsRole).toInt() & FLAG_FAVORITE)
               != 0));
    case (CredentialModel::Col_Site):
        return (QString::compare(sourceModel()->data(left).toString(),
                                 sourceModel()->data(right).toString(),
                                 Qt::CaseInsensitive)
                < 0);
    case (CredentialModel::Col_Accessed):
        return sourceModel()->data(left, CredentialModel::AccessedRole).toDate()
               < sourceModel()->data(right, CredentialModel::AccessedRole).toDate();
    default:
        return QSortFilterProxyModel::lessThan(left, right);
    }

    return false;
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
