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
    timer = new QTimer(this);
    timer->setSingleShot(true);
    QString password = "";

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
    connect(protocol, &CerberusProtocol::passReceived, this, &MainWindow::onPassReceived);
    connect(protocol, &CerberusProtocol::nackReceived, this, &MainWindow::onNackReceived);
    connect(timer, &QTimer::timeout, this, &MainWindow::onTimeout);

    proxy->setSourceModel(model);
    ui->credentialList->setModel(proxy); //using a proxy to keep the og creds but hide specific ones
    ui->credentialList->setSortingEnabled(true);
    QHeaderView *header = ui->credentialList->horizontalHeader();
    header->setSectionResizeMode(CredentialModel::Col_Fav, QHeaderView::Fixed);
    header->setSectionResizeMode(CredentialModel::Col_Site, QHeaderView::Stretch);
    header->setSectionResizeMode(CredentialModel::Col_Modified, QHeaderView::ResizeToContents);
    header->resizeSection(CredentialModel::Col_Fav, 32);
    proxy->setFilterKeyColumn(CredentialModel::Col_Site);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    connect(ui->searchInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        proxy->setFilterFixedString(text);
    });
    connect(ui->credentialList->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            &MainWindow::onSelectionChanged);


    //TODO: fix filtering the selected (cosmetic)
    // connect(proxy, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this] {
    //     if (ui->credentialList->selectionModel()->currentIndex().isValid()) {
    //         qDebug() << ui->credentialList->selectionModel()
    //                         ->currentIndex()
    //                         .data(CredentialModel::SlotIdxRole)
    //                         .toInt();
    //         m_before = ui->credentialList->selectionModel()
    //                        ->currentIndex()
    //                        .data(CredentialModel::SlotIdxRole)
    //                        .toInt();
    //     } else {
    //         m_before = -1;
    //     }
    // });

    // connect(proxy, &QAbstractItemModel::rowsRemoved, this, [this] {
    //     if (m_before
    //         != ui->credentialList->selectionModel()
    //                ->currentIndex()
    //                .data(CredentialModel::SlotIdxRole)
    //                .toInt()) {
    //         qDebug() << ui->credentialList->selectionModel()
    //                         ->currentIndex()
    //                         .data(CredentialModel::SlotIdxRole)
    //                         .toInt();
    //         ui->credentialList->selectionModel()->clearCurrentIndex();
    //         ui->credentialList->selectionModel()->clearSelection();
    //     }
    // });
}

void MainWindow::passwordValue(bool reveal){
    if(reveal){
        ui->passwordBtnStack->setCurrentIndex(1);  //show copy button
        ui->detailPasswordValue->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    }else{
        ui->passwordBtnStack->setCurrentIndex(0); //show fetch button
        ui->detailPasswordValue->setText("•••••••••••••••••••");
        ui->detailPasswordValue->setTextInteractionFlags(Qt::NoTextInteraction);
    }
    ui->detailPasswordValue->setProperty("revealed", reveal);
    ui->detailPasswordValue->style()->unpolish(ui->detailPasswordValue);
    ui->detailPasswordValue->style()->polish(ui->detailPasswordValue); //updates the Ui
}

bool CustomSort::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    //helps sort each column based on the col ordered values
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
    case (CredentialModel::Col_Modified):
        return sourceModel()->data(left, CredentialModel::ModifiedRole).toDate()
               < sourceModel()->data(right, CredentialModel::ModifiedRole).toDate();
    default:
        return QSortFilterProxyModel::lessThan(left, right);
    }

    return false;
}

void MainWindow::connectToDevice()
{
    serial->setPortName("COM4"); //change to your port
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

void MainWindow::on_syncBtn_clicked()
{
    if (!serial->isOpen()) {
        ui->deviceInfo->setText("● Device NOT Connected");
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

void MainWindow::onPassReceived(QString password){
    ui->detailPasswordValue->setText(password);
    pswrd = password;
    passwordValue(true);
}

void MainWindow::onNackReceived(int reason){
    ui->deviceStatus->setText("Received: NACK");
    //put reasons in debug so shoulder surfers cant see
    switch(reason){
    case NACK_BAD_PAYLOAD:
        qDebug() << "NACK: BAD PAYLOAD";
        break;
    case NACK_BAD_SLOT_IDX:
        qDebug() << "NACK: BAD SLOT";
        break;
    case NACK_DECRYPT_FAILURE:
        qDebug() << "NACK: DECRYPT FAILURE";
        break;
    case NACK_DEVICE_DISCONNECTED:
        qDebug() << "NACK: DEVICE DISCONNECTED";
        break;
    case NACK_DEVICE_LOCKED:
        qDebug() << "NACK: DEVICE LOCKED";
        break;
    case NACK_STORAGE_ERROR:
        qDebug() << "NACK: STORAGE ERROR";
        break;
    case NACK_UNKNOWN_OPCODE:
        qDebug() << "NACK: UNKNOWN OPCODE";
        break;
    default:
        ui->deviceStatus->setText("NACK: INVALID NACK CODE");
        break;
    }

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

void MainWindow::onSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    if (!current.isValid()) {
        m_selected = -1;
        ui->detailStack->setCurrentIndex(0);
        return;
    }
    onTimeout(); //update passvalue fields & clipboard
    timer->stop();
    m_selected = current.data(CredentialModel::SlotIdxRole).toInt();
    ui->detailSiteName->setText(current.data(CredentialModel::SiteRole).toString());
    ui->detailUrl->setText(current.data(CredentialModel::UrlRole).toString());
    ui->detailEmailValue->setText(current.data(CredentialModel::EmailRole).toString());
    ui->detailNotes->setPlainText(current.data(CredentialModel::NotesRole).toString());
    ui->detailModified->setText(
        ("Last Modified: ")
        + current.data(CredentialModel::ModifiedRole).toDate().toString("MMM d, yyyy"));
    ui->detailCreated->setText(
        ("First Created: ")
        + current.data(CredentialModel::CreatedRole).toDate().toString("MMM d, yyyy"));
    ui->favBtnDetail->setChecked((current.data(CredentialModel::FlagsRole).toInt() & FLAG_FAVORITE)
                                 != 0);
    ui->detailStack->setCurrentIndex(1);
}

void MainWindow::on_fetchPasswordBtn_clicked()
{
    if(m_selected < 0){
        return;
    }
    //break slot_idx into low and high bytes for payload
    uint8_t low = m_selected & 0xFF;
    uint8_t high = (m_selected >> 8) & 0xFF;
        std::vector<uint8_t> payload = {low,high};
    protocol->sendCommand(CMD_GET_PASSWORD, payload);
    timer->start(20000);
}

void MainWindow::onTimeout(){
    if(QGuiApplication::clipboard()->text()==pswrd){ //if the password shown matches the clipboard
        pswrd.clear();
        QGuiApplication::clipboard()->clear();
    }
    passwordValue(false);
}

void MainWindow::on_copyPasswordBtn_clicked()
{   //set the current password then copies
    QGuiApplication::clipboard()->setText(pswrd);
}

void MainWindow::on_copyEmailBtn_clicked()
{
    if(m_selected < 0){
        return;
    }
    QGuiApplication::clipboard()->setText(ui->detailEmailValue->text()); //copies the email
}

MainWindow::~MainWindow()
{
    if (serial->isOpen())
        serial->close();
    if(QGuiApplication::clipboard()->text()==pswrd){
        pswrd.clear();
        QGuiApplication::clipboard()->clear();
    }
    delete ui;
}


