// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_limits.h>

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QStringDecoder>

#include <limits>

namespace QindaQt::Network::CodecPrivate {

class Writer final {
public:
  Writer() : m_buffer(&m_payload), m_stream(&m_buffer) {
    m_buffer.open(QIODevice::WriteOnly);
    m_stream.setByteOrder(QDataStream::BigEndian);
    m_stream.setVersion(QDataStream::Qt_6_0);
  }

  void raw(const char *bytes, const qsizetype size) {
    if (!m_good || size < 0 || size > std::numeric_limits<qint64>::max()
        || m_stream.writeRawData(bytes, static_cast<qint64>(size))
               != static_cast<qint64>(size)) {
      m_good = false;
    }
  }
  void u32(const quint32 value) { m_stream << value; }
  void u64(const quint64 value) { m_stream << value; }
  void i64(const qint64 value) { m_stream << value; }
  void u8(const quint8 value) { m_stream << value; }
  void boolean(const bool value) { u8(value ? quint8(1) : quint8(0)); }
  void text(const QString &value) {
    const QByteArray bytes = value.toUtf8();
    if (static_cast<quint64>(bytes.size())
        > std::numeric_limits<quint32>::max()) {
      m_good = false;
      return;
    }
    u32(static_cast<quint32>(bytes.size()));
    raw(bytes.constData(), bytes.size());
  }
  [[nodiscard]] bool good() const {
    return m_good && m_stream.status() == QDataStream::Ok
           && m_payload.size() <= kMaxSerializedBytes;
  }
  [[nodiscard]] QByteArray take() {
    m_buffer.close();
    return std::move(m_payload);
  }

private:
  QByteArray m_payload;
  QBuffer m_buffer;
  QDataStream m_stream;
  bool m_good = true;
};

class Reader final {
public:
  explicit Reader(const QByteArrayView payload)
      : m_payload(payload.size() <= kMaxSerializedBytes
                      ? QByteArray(payload.data(), payload.size())
                      : QByteArray{}),
        m_buffer(&m_payload), m_stream(&m_buffer) {
    m_buffer.open(QIODevice::ReadOnly);
    m_stream.setByteOrder(QDataStream::BigEndian);
    m_stream.setVersion(QDataStream::Qt_6_0);
    if (payload.size() > kMaxSerializedBytes) {
      m_error = CodecError::PayloadTooLarge;
    }
  }

  bool raw(char *destination, const qsizetype size) {
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
  bool i64(qint64 &value) { return primitive(value); }
  bool u8(quint8 &value) { return primitive(value); }
  bool boolean(bool &value) {
    quint8 encoded = 0;
    if (!u8(encoded)) {
      return false;
    }
    if (encoded > 1) {
      fail(CodecError::InvalidValue);
      return false;
    }
    value = encoded == 1;
    return true;
  }
  bool text(QString &value, const qsizetype maximumUtf8Bytes) {
    quint32 size = 0;
    if (!u32(size)) {
      return false;
    }
    if (static_cast<quint64>(size) > static_cast<quint64>(maximumUtf8Bytes)) {
      fail(CodecError::PayloadTooLarge);
      return false;
    }
    QByteArray bytes(static_cast<qsizetype>(size), Qt::Uninitialized);
    if (!raw(bytes.data(), bytes.size())) {
      return false;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString decoded = decoder.decode(bytes);
    if (decoder.hasError() || decoded.contains(QChar::Null)) {
      fail(CodecError::InvalidValue);
      return false;
    }
    value = std::move(decoded);
    return true;
  }
  void fail(const CodecError error) {
    if (m_error == CodecError::None) {
      m_error = error;
    }
  }
  [[nodiscard]] bool good() const {
    return m_error == CodecError::None && m_stream.status() == QDataStream::Ok;
  }
  [[nodiscard]] bool finished() const { return good() && m_buffer.atEnd(); }
  [[nodiscard]] CodecError error() const {
    if (m_error != CodecError::None) {
      return m_error;
    }
    return m_stream.status() == QDataStream::Ok ? CodecError::None
                                                : CodecError::Truncated;
  }

private:
  template <typename T> bool primitive(T &value) {
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

DecodeResult readerFailure(const Reader &reader, QString reasonCode);

void writeRadio(Writer &writer, const Radio &radio);
bool readRadio(Reader &reader, Radio &radio);
void writeDevice(Writer &writer, const Device &device);
bool readDevice(Reader &reader, Device &device);
void writeAccessPoint(Writer &writer, const AccessPoint &point);
bool readAccessPoint(Reader &reader, AccessPoint &point);
void writeKnownNetwork(Writer &writer, const KnownNetwork &network);
bool readKnownNetwork(Reader &reader, KnownNetwork &network);
void writeActiveConnection(Writer &writer, const ActiveConnection &connection);
bool readActiveConnection(Reader &reader, ActiveConnection &connection);
void writeScanLease(Writer &writer, const ScanLease &lease);
bool readScanLease(Reader &reader, ScanLease &lease);

template <typename T, typename ReadOne>
bool readBoundedList(Reader &reader, QList<T> &values, const qsizetype maximum,
                     ReadOne readOne) {
  quint32 count = 0;
  if (!reader.u32(count)) {
    return false;
  }
  if (static_cast<quint64>(count) > static_cast<quint64>(maximum)) {
    reader.fail(CodecError::PayloadTooLarge);
    return false;
  }
  QList<T> decoded;
  decoded.reserve(static_cast<qsizetype>(count));
  for (quint32 index = 0; index < count; ++index) {
    T value;
    if (!readOne(reader, value)) {
      return false;
    }
    decoded.push_back(std::move(value));
  }
  values = std::move(decoded);
  return true;
}

template <typename T, typename WriteOne>
void writeList(Writer &writer, const QList<T> &values, WriteOne writeOne) {
  writer.u32(static_cast<quint32>(values.size()));
  for (const T &value : values) {
    writeOne(writer, value);
  }
}

} // namespace QindaQt::Network::CodecPrivate
