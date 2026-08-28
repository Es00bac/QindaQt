// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_codec.h>
#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>
#include <QtCore/QStringDecoder>

#include <limits>

namespace QindaQt::Power::CodecPrivate {

inline constexpr char kSnapshotMagic[] = {'Q', 'P', '1', 'S'};
inline constexpr char kOperationResultMagic[] = {'Q', 'P', '1', 'R'};

class Writer final {
public:
  Writer() : m_buffer(&m_payload), m_stream(&m_buffer) {
    m_buffer.open(QIODevice::WriteOnly);
    m_stream.setByteOrder(QDataStream::BigEndian);
    m_stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    m_stream.setVersion(QDataStream::Qt_6_0);
  }

  void raw(const char *bytes, const qsizetype size) {
    if (!m_good || size < 0 || size > std::numeric_limits<qint64>::max() ||
        m_stream.writeRawData(bytes, static_cast<qint64>(size)) !=
            static_cast<qint64>(size)) {
      m_good = false;
    }
  }
  void u32(const quint32 value) { m_stream << value; }
  void u64(const quint64 value) { m_stream << value; }
  void i64(const qint64 value) { m_stream << value; }
  void real(const double value) { m_stream << value; }
  void boolean(const bool value) { m_stream << value; }
  void text(const QString &value) {
    const QByteArray bytes = value.toUtf8();
    if (static_cast<quint64>(bytes.size()) >
        std::numeric_limits<quint32>::max()) {
      m_good = false;
      return;
    }
    u32(static_cast<quint32>(bytes.size()));
    raw(bytes.constData(), bytes.size());
  }
  [[nodiscard]] bool good() const {
    return m_good && m_stream.status() == QDataStream::Ok &&
           m_payload.size() <= kMaxSerializedBytes;
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
    m_stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    m_stream.setVersion(QDataStream::Qt_6_0);
    if (payload.size() > kMaxSerializedBytes) {
      m_error = CodecError::PayloadTooLarge;
    }
  }

  bool raw(char *destination, const qsizetype size) {
    if (!good() || size < 0 || size > m_buffer.bytesAvailable() ||
        size > std::numeric_limits<qint64>::max()) {
      fail(CodecError::Truncated);
      return false;
    }
    if (m_stream.readRawData(destination, static_cast<qint64>(size)) !=
        static_cast<qint64>(size)) {
      fail(CodecError::Truncated);
      return false;
    }
    return true;
  }
  bool u32(quint32 &value) { return primitive(value); }
  bool u64(quint64 &value) { return primitive(value); }
  bool i64(qint64 &value) { return primitive(value); }
  bool real(double &value) { return primitive(value); }
  bool boolean(bool &value) { return primitive(value); }
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

void writeHandle(Writer &writer, const Handle &handle);
bool readHandle(Reader &reader, Handle &handle);
void writeSourceTruth(Writer &writer, const SourceTruth &truth);
bool readSourceTruth(Reader &reader, SourceTruth &truth);
void writeSupply(Writer &writer, const PowerSupply &supply);
bool readSupply(Reader &reader, PowerSupply &supply);
void writeComposite(Writer &writer, const CompositeBattery &composite);
bool readComposite(Reader &reader, CompositeBattery &composite);
void writeProfileState(Writer &writer, const ProfileState &profiles);
bool readProfileState(Reader &reader, ProfileState &profiles);
void writeInhibitor(Writer &writer, const Inhibitor &inhibitor);
bool readInhibitor(Reader &reader, Inhibitor &inhibitor);
void writeKeyboardBacklight(Writer &writer, const KeyboardBacklight &device);
bool readKeyboardBacklight(Reader &reader, KeyboardBacklight &device);
void writeInternalBacklight(Writer &writer, const InternalBacklight &device);
bool readInternalBacklight(Reader &reader, InternalBacklight &device);
void writeWaylandBinding(Writer &writer, const WaylandBinding &binding);
bool readWaylandBinding(Reader &reader, WaylandBinding &binding);

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

} // namespace QindaQt::Power::CodecPrivate
