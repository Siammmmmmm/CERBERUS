# CERBERUS

A portable, hardware-backed password vault. An ESP32-S3 device holds the credentials, and a Qt6/C++ desktop app manages them over a custom binary UART protocol. Built as a hands-on project in embedded firmware, protocol design, and hardware security.

> **Status: work in progress.** The protocol, the two-phase data model, and the desktop UI work end to end on real hardware. The device currently serves hard-coded test data. Encrypted storage, PIN unlock, and the secure element's security role are **not built yet**. Do not store real passwords in it.

## What works today

- Framed binary protocol with CRC8 and resync-on-error, implemented independently in the firmware (C) and the desktop app (C++)
- PING/PONG, metadata sync, and single-password fetch, tested over real UART
- Desktop app: searchable and sortable credential table, detail pane, favorite markers, and copy-to-clipboard that clears after 30 seconds
- I2C communication with an ATECC608B secure element (initialization succeeds; the chip is still blank and unprovisioned)

## Architecture

```
Qt6/C++ desktop app  <-- UART, framed binary protocol -->  ESP32-S3 firmware (ESP-IDF)
                                                                  |
                                                              I2C |
                                                          ATECC608B secure element
```

- **Two-phase data model.** A metadata request returns every credential's site, URL, email, notes, dates, and flags, and never a password. A separate request fetches one password by slot index. Passwords are never part of the bulk dump.
- **Layered firmware.** `transport.c` handles framing and CRC and knows nothing about the application. Dispatch is passed in as a function-pointer callback.
- **Layered desktop code.** `CerberusProtocol` has no serial-port or UI dependencies: bytes in, Qt signals out. `MainWindow` owns the serial port and the UI. The credential list uses `QAbstractTableModel` behind a `QSortFilterProxyModel`.
- **Shared contract.** `Common/protocol.h` and `Common/opcodes.h` are included by both the firmware and the desktop app, so they agree on opcodes and size limits.

## Protocol

Frame: `[0xAA][opcode][len][payload][crc8]`

- `len` is the payload length (0 to 255).
- CRC8 uses polynomial `0x31`, init `0x00`, no reflection, no final XOR. It covers opcode, length, and payload, not the start byte. For the ASCII string `123456789` this implementation produces `0xA2`.
- The receiver scans for `0xAA`, discards garbage, and drops one byte to resync on a CRC mismatch.

| Opcode | Name | Notes |
|---|---|---|
| `0x01` | `CMD_PING` | |
| `0x02` | `CMD_UNLOCK` | Currently triggers the metadata dump (placeholder) |
| `0x04` | `CMD_GET_PASSWORD` | Payload: slot index, 2 bytes, little-endian |
| `0x81` | `RES_PONG` | |
| `0x83` | `RES_METADATA` | One frame per credential |
| `0x84` | `RES_PASSWORD` | Payload: `[len][password bytes]` |
| `0x8D` | `RES_METADATA_END` | |
| `0x8F` | `RES_NACK` | Payload: 1-byte reason |

Also defined but not yet used: `CMD_GET_METADATA` (0x03), `CMD_ADD_CRED` (0x05), `CMD_DELETE_CRED` (0x06), `CMD_LOCK` (0x07), `RES_OK` (0x8E), `EVT_LOG` (0xC0).

**Metadata payload**, in wire order:

| Field | Size |
|---|---|
| Slot index | uint16, little-endian |
| Accessed, modified, created dates | 3 x uint16, little-endian |
| Flags | uint8 (bit 0 occupied, bit 1 favorite) |
| Site, URL, email, notes | each `[len][bytes]`, no terminator |

Dates are packed as `[year:7][month:4][day:5]` with a base year of 2025. String limits are site 32, URL 96, email 64, notes 50, and password 30 bytes. The worst-case metadata payload is exactly 255 bytes.

NACK reasons: `0x00` unknown opcode, `0x01` device locked, `0x02` bad slot, `0x03` decrypt failure, `0x04` storage error, `0x05` device disconnected, `0x06` bad payload. New reasons are appended at the end so the two separately built programs never disagree on a value.

## Hardware

| Part | Connection |
|---|---|
| ESP32-S3 dev board, two USB-C ports | Native USB for flashing and logs; a UART bridge for the protocol |
| Protocol link | UART0, GPIO43 (TX) and GPIO44 (RX), 115200 8N1 |
| ATECC608B breakout (Adafruit) | I2C, SDA GPIO9, SCL GPIO8, 100 kHz, address 0x60 |
| Status LED | GPIO38 |

Logs go to the native USB port so they never mix with protocol bytes. The Kconfig symbol for the secure element reads `ATECC608A`, but the part is a 608B.

## Building

**Firmware** (ESP-IDF v6.0.2, `sdkconfig.defaults` holds the pin and I2C settings):

```
idf.py set-target esp32s3
idf.py build
idf.py -p <native-usb-port> flash monitor
```

**Desktop app** (Qt 6 with the Serial Port module, CMake):

Open the project in Qt Creator. The serial port is currently hard-coded in `MainWindow::connectToDevice()`. Change it to the port your UART bridge uses before building.

## Roadmap

**Next**
- [ ] Persistent storage on a dedicated NVS partition, with metadata and password kept as separate records so the metadata path never reads ciphertext
- [ ] Add and delete credentials (`CMD_ADD_CRED` / `CMD_DELETE_CRED`) with input forms validated against the size limits
- [ ] Lock state machine, PIN unlock, ATECC608B provisioning, and encrypted password storage
- [ ] Update access and modified dates on the device (the ESP32 has no clock, so the host would supply the date or an RTC would be added)

**Later**
- [ ] Serial port picker instead of a hard-coded port
- [ ] USB CDC transport on a single-USB board

**After v1**: USB HID auto-fill, BLE and mobile support, battery power, and passkeys (FIDO2/CTAP2).

## Security design goals (not implemented yet)

- A 4-digit PIN has only 10,000 combinations, so it should be checked through the secure element as a rate-limited gate that releases a high-entropy secret, never used to derive an encryption key by itself.
- Stored ciphertext should be useless without the physical chip.
- Locking should zeroize the working key in RAM, and power loss should return the device to the locked state.

## Known limitations

- Passwords on the device are currently plaintext test fixtures.
- The serial link is unencrypted. CRC8 detects corruption, not tampering.
- Clipboard clearing covers the Copy button only. It does not cover manual Ctrl+C, Windows clipboard history, or a crash.
- The serial port is currently hard-coded.
