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
    connectToDevice();
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::onDataReceived);
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
    if (send_packet(CMD_PING, payload)) {
        ui->deviceStatus->setText("Sent: PING");
    }
}

void MainWindow::dispatch(uint8_t opcode, const std::vector<uint8_t> &payload)
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

void MainWindow::discard_n(std::vector<uint8_t> &buffer, size_t n)
{
    if (n > buffer.size()) {
        buffer.clear();
        return;
    }

    buffer.erase(buffer.begin(), buffer.begin() + n);
}

void MainWindow::parse_frame(std::vector<uint8_t> &buffer)
{
    if (buffer.size() > MAX_BUFFER) {
        buffer.clear();
        return;
    }

    while (true) {
        auto it = std::find(buffer.begin(), buffer.end(), START_BYTE);

        // Check if element is present
        if (it == buffer.end()) {
            buffer.clear();
            break;
        } else if (buffer.at(0) != START_BYTE) {
            discard_n(buffer, std::distance(buffer.begin(), it));
        }

        if (buffer.size() < 3) {
            break;
        }

        uint8_t frame_len = buffer.at(2);
        if (buffer.size() < frame_len + 4) {
            break;
        }

        uint8_t check = crbrs_CRC8(&buffer.at(1), frame_len + 2);
        if (buffer.at(frame_len + 3) == check) {
            std::vector<uint8_t> payload;
            payload.assign(buffer.begin() + 3, buffer.begin() + 3 + frame_len);
            dispatch(buffer.at(1), payload);
            discard_n(buffer, frame_len + 4);
        } else {
            discard_n(buffer, 1);
        }
    }
}

void MainWindow::onDataReceived()
{
    QByteArray data = serial->readAll();
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.constData());
    buffer.insert(buffer.end(), ptr, ptr + data.size());

    parse_frame(buffer);
}

bool MainWindow::send_packet(uint8_t opcode, const std::vector<uint8_t> &payload)
{
    if (payload.size() > MAX_PAYLOAD) {
        return false;
    }
    uint8_t len = static_cast<uint8_t>(payload.size());
    std::vector<uint8_t> packet = {START_BYTE, opcode, len};

    if (!payload.empty()) {
        packet.reserve(packet.size() + len + 2);
        packet.insert(packet.end(), payload.begin(), payload.end());
    }

    packet.push_back(crbrs_CRC8(&packet.at(1), len + 2));
    if (serial->write(reinterpret_cast<const char *>(packet.data()), packet.size()) == -1) {
        return false;
    }
    return true;
}

MainWindow::~MainWindow()
{
    if (serial->isOpen())
        serial->close();
    delete ui;
}
