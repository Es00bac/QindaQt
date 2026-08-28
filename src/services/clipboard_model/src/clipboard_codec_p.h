// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QtEndian>
#include <QtGlobal>

#include <limits>

namespace QindaQt::Services::ClipboardModel::CodecDetail {

// AGENT-CONTRACT: every clipboard wire form is little-endian, versioned by a
// one-byte version after a four-byte magic, and length-prefixed. Both codecs
// must keep decoding strictly bounds-checked: a declared length is trusted
// only when the remaining buffer can satisfy it, and any trailing byte after
// the final field is MalformedData. This is the shared hostile-input floor
// the future Wayland adapter inherits; do not add a lenient decode path.
class ByteWriter final {
public:
    void u8(quint8 value) { m_buffer.append(static_cast<char>(value)); }
    void u16(quint16 value)
    {
        char bytes[2];
        qToLittleEndian(value, bytes);
        m_buffer.append(bytes, 2);
    }
    void u32(quint32 value)
    {
        char bytes[4];
        qToLittleEndian(value, bytes);
        m_buffer.append(bytes, 4);
    }
    void u64(quint64 value)
    {
        char bytes[8];
        qToLittleEndian(value, bytes);
        m_buffer.append(bytes, 8);
    }
    void raw(const QByteArray &bytes) { m_buffer.append(bytes); }
    void lengthPrefixedUtf8(const QString &text)
    {
        const QByteArray utf8 = text.toUtf8();
        Q_ASSERT(utf8.size() <= std::numeric_limits<quint16>::max());
        u16(static_cast<quint16>(utf8.size()));
        raw(utf8);
    }

    [[nodiscard]] const QByteArray &buffer() const noexcept { return m_buffer; }

private:
    QByteArray m_buffer;
};

class ByteReader final {
public:
    explicit ByteReader(QByteArray bytes)
        : m_buffer(std::move(bytes))
    {
    }

    [[nodiscard]] bool ok() const noexcept { return m_ok; }
    [[nodiscard]] qsizetype remaining() const noexcept { return m_buffer.size() - m_offset; }
    [[nodiscard]] bool atEnd() const noexcept { return m_offset == m_buffer.size(); }

    [[nodiscard]] bool readMagic(const char (&magic)[5])
    {
        if (remaining() < 4 || m_buffer.mid(m_offset, 4) != QByteArray(magic, 4)) {
            fail();
            return false;
        }
        m_offset += 4;
        return true;
    }

    [[nodiscard]] quint8 u8()
    {
        if (remaining() < 1) {
            fail();
            return 0;
        }
        return static_cast<quint8>(m_buffer.at(m_offset++));
    }

    [[nodiscard]] quint16 u16()
    {
        if (remaining() < 2) {
            fail();
            return 0;
        }
        const quint16 value = qFromLittleEndian<quint16>(m_buffer.constData() + m_offset);
        m_offset += 2;
        return value;
    }

    [[nodiscard]] quint32 u32()
    {
        if (remaining() < 4) {
            fail();
            return 0;
        }
        const quint32 value = qFromLittleEndian<quint32>(m_buffer.constData() + m_offset);
        m_offset += 4;
        return value;
    }

    [[nodiscard]] quint64 u64()
    {
        if (remaining() < 8) {
            fail();
            return 0;
        }
        const quint64 value = qFromLittleEndian<quint64>(m_buffer.constData() + m_offset);
        m_offset += 8;
        return value;
    }

    // Reads exactly `count` bytes; rejects when the buffer cannot satisfy it.
    [[nodiscard]] QByteArray sized(qsizetype count)
    {
        if (!m_ok || count < 0 || remaining() < count) {
            fail();
            return {};
        }
        const QByteArray bytes = m_buffer.mid(m_offset, count);
        m_offset += count;
        return bytes;
    }

    void fail() { m_ok = false; }

private:
    QByteArray m_buffer;
    qsizetype m_offset = 0;
    bool m_ok = true;
};

} // namespace QindaQt::Services::ClipboardModel::CodecDetail
