// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_types.h>

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QStringDecoder>

#include <limits>

namespace QindaQt::Display::CodecPrivate
{

inline constexpr char kCandidateMagic[] = {'Q', 'D', '1', 'C'};
inline constexpr char kSnapshotMagic[] = {'Q', 'D', '1', 'S'};

class Writer final
{
public:
    Writer()
        : m_buffer(&m_payload)
        , m_stream(&m_buffer)
    {
        m_buffer.open(QIODevice::WriteOnly);
        m_stream.setByteOrder(QDataStream::BigEndian);
        m_stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        m_stream.setVersion(QDataStream::Qt_6_0);
    }

    void raw(const char *bytes, const qsizetype size)
    {
        if (!m_good || size < 0 || size > std::numeric_limits<qint64>::max()
            || m_stream.writeRawData(bytes, static_cast<qint64>(size))
                != static_cast<qint64>(size)) {
            m_good = false;
        }
    }

    void u32(const quint32 value) { m_stream << value; }
    void u64(const quint64 value) { m_stream << value; }
    void i32(const qint32 value) { m_stream << value; }
    void boolean(const bool value) { m_stream << value; }
    void real(const double value) { m_stream << value; }

    void bytes(const QByteArray &value)
    {
        if (static_cast<quint64>(value.size())
            > std::numeric_limits<quint32>::max()) {
            m_good = false;
            return;
        }
        u32(static_cast<quint32>(value.size()));
        raw(value.constData(), value.size());
    }

    void text(const QString &value)
    {
        bytes(value.toUtf8());
    }

    [[nodiscard]] bool good() const
    {
        return m_good && m_stream.status() == QDataStream::Ok
            && m_payload.size() <= kMaxSerializedBytes;
    }

    [[nodiscard]] QByteArray take()
    {
        m_buffer.close();
        return std::move(m_payload);
    }

private:
    QByteArray m_payload;
    QBuffer m_buffer;
    QDataStream m_stream;
    bool m_good = true;
};

class Reader final
{
public:
    explicit Reader(const QByteArrayView payload)
        : m_payload(payload.size() <= kMaxSerializedBytes
                        ? QByteArray(payload.data(), payload.size())
                        : QByteArray{})
        , m_buffer(&m_payload)
        , m_stream(&m_buffer)
    {
        m_buffer.open(QIODevice::ReadOnly);
        m_stream.setByteOrder(QDataStream::BigEndian);
        m_stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
        m_stream.setVersion(QDataStream::Qt_6_0);
        if (payload.size() > kMaxSerializedBytes) {
            m_error = CodecError::PayloadTooLarge;
        }
    }

    bool raw(char *destination, const qsizetype size)
    {
        if (!good() || size < 0 || size > m_buffer.bytesAvailable()
            || size > std::numeric_limits<qint64>::max()) {
            fail(CodecError::Truncated);
            return false;
        }
        if (m_stream.readRawData(destination, static_cast<qint64>(size))
            != static_cast<qint64>(size)) {
            fail(CodecError::Truncated);
            return false;
        }
        return true;
    }

    bool u32(quint32 &value) { return primitive(value); }
    bool u64(quint64 &value) { return primitive(value); }
    bool i32(qint32 &value) { return primitive(value); }
    bool boolean(bool &value) { return primitive(value); }
    bool real(double &value) { return primitive(value); }

    bool bytes(QByteArray &value, const qsizetype maximum)
    {
        quint32 size = 0;
        if (!u32(size)) {
            return false;
        }
        if (static_cast<quint64>(size) > static_cast<quint64>(maximum)) {
            fail(CodecError::PayloadTooLarge);
            return false;
        }
        QByteArray decoded(static_cast<qsizetype>(size), Qt::Uninitialized);
        if (!raw(decoded.data(), decoded.size())) {
            return false;
        }
        value = std::move(decoded);
        return true;
    }

    bool text(QString &value, const qsizetype maximumUtf8Bytes)
    {
        QByteArray bytesValue;
        if (!bytes(bytesValue, maximumUtf8Bytes)) {
            return false;
        }
        QStringDecoder decoder(QStringDecoder::Utf8);
        QString decoded = decoder.decode(bytesValue);
        if (decoder.hasError() || decoded.contains(QChar::Null)) {
            fail(CodecError::InvalidValue);
            return false;
        }
        value = std::move(decoded);
        return true;
    }

    void fail(const CodecError error)
    {
        if (m_error == CodecError::None) {
            m_error = error;
        }
    }

    [[nodiscard]] bool good() const
    {
        return m_error == CodecError::None && m_stream.status() == QDataStream::Ok;
    }

    [[nodiscard]] bool finished() const
    {
        return good() && m_buffer.atEnd();
    }

    [[nodiscard]] CodecError error() const
    {
        if (m_error != CodecError::None) {
            return m_error;
        }
        return m_stream.status() == QDataStream::Ok ? CodecError::None
                                                    : CodecError::Truncated;
    }

private:
    template<typename T>
    bool primitive(T &value)
    {
        if (!good()) {
            return false;
        }
        m_stream >> value;
        if (m_stream.status() != QDataStream::Ok) {
            fail(CodecError::Truncated);
            return false;
        }
        return true;
    }

    QByteArray m_payload;
    QBuffer m_buffer;
    QDataStream m_stream;
    CodecError m_error = CodecError::None;
};

inline void writeMode(Writer &writer, const Mode &mode)
{
    writer.text(mode.id);
    writer.i32(mode.pixelSize.width());
    writer.i32(mode.pixelSize.height());
    writer.u32(mode.refreshMilliHertz);
    writer.boolean(mode.preferred);
}

inline bool readMode(Reader &reader, Mode &mode)
{
    qint32 width = 0;
    qint32 height = 0;
    return reader.text(mode.id, kMaxModeIdUtf8Bytes) && reader.i32(width)
        && reader.i32(height) && reader.u32(mode.refreshMilliHertz)
        && reader.boolean(mode.preferred)
        && ((mode.pixelSize = QSize(width, height)), true);
}

inline void writeCandidateOutput(Writer &writer, const CandidateOutput &output)
{
    writer.text(output.stableId);
    writer.boolean(output.enabled);
    writer.boolean(output.primary);
    writer.text(output.modeId);
    writer.i32(output.position.x());
    writer.i32(output.position.y());
    writer.real(output.scale);
    writer.u32(static_cast<quint32>(output.transform));
    writer.u32(output.priority);
    writer.text(output.replicationSourceStableId);
}

inline bool readCandidateOutput(Reader &reader, CandidateOutput &output)
{
    qint32 x = 0;
    qint32 y = 0;
    quint32 transform = 0;
    if (!reader.text(output.stableId, kMaxStableIdUtf8Bytes)
        || !reader.boolean(output.enabled) || !reader.boolean(output.primary)
        || !reader.text(output.modeId, kMaxModeIdUtf8Bytes) || !reader.i32(x)
        || !reader.i32(y) || !reader.real(output.scale) || !reader.u32(transform)
        || !reader.u32(output.priority)
        || !reader.text(output.replicationSourceStableId, kMaxStableIdUtf8Bytes)) {
        return false;
    }
    output.position = QPoint(x, y);
    output.transform = static_cast<Transform>(transform);
    return true;
}

inline DecodeResult readerFailure(const Reader &reader, const QString &reason)
{
    const CodecError error = reader.error() == CodecError::None ? CodecError::InvalidValue
                                                                : reader.error();
    return {.error = error, .reasonCode = reason};
}

} // namespace QindaQt::Display::CodecPrivate
