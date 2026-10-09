// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QFile>
#include <QtEndian>

inline void put16(QByteArray &bytes, qsizetype offset, quint16 value)
{
    qToLittleEndian(value, reinterpret_cast<uchar *>(bytes.data() + offset));
}
inline void put32(QByteArray &bytes, qsizetype offset, quint32 value)
{
    qToLittleEndian(value, reinterpret_cast<uchar *>(bytes.data() + offset));
}
// Header-format fixture only; it is never executed or represented as an app.
inline QByteArray peFixture(quint16 machine = 0x8664)
{
    QByteArray bytes(768, '\0');
    bytes.replace(0, 2, QByteArrayLiteral("MZ"));
    put32(bytes, 60, 128);
    bytes.replace(128, 4, QByteArray("PE\0\0", 4));
    put16(bytes, 132, machine); put16(bytes, 134, 1);
    const quint16 optionalSize = machine == 0x014c ? 224 : 240;
    put16(bytes, 148, optionalSize); put16(bytes, 150, 0x0002);
    put16(bytes, 152, machine == 0x014c ? 0x010b : 0x020b);
    put32(bytes, 168, 0x1000); put16(bytes, 220, 2);
    const qsizetype section = 152 + optionalSize;
    put32(bytes, section + 8, 256); put32(bytes, section + 12, 0x1000);
    put32(bytes, section + 16, 256); put32(bytes, section + 20, 512);
    put32(bytes, section + 36, 0x60000020);
    return bytes;
}
inline bool writePe(const QString &path, const QByteArray &bytes = peFixture())
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
