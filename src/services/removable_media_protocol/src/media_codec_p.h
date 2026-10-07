// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <QtCore/QtEndian>
#include <QtCore/QStringDecoder>
#include <array>
#include <utility>

namespace QindaQt::RemovableMedia::CodecPrivate {
// AGENT-GUARD: hostile lengths are checked against both per-field budget and
// remaining input before decoding/reserving. No QDataStream implicit arrays.
class Reader final {
public:
  Reader(QByteArrayView bytes, qsizetype maximum) : m_bytes(bytes) {
    if (bytes.size() > maximum) fail(CodecError::PayloadTooLarge);
  }
  template<class Integer> bool number(Integer &value) {
    constexpr auto size = static_cast<qsizetype>(sizeof(Integer));
    if (!room(size)) return false;
    value = qFromLittleEndian<Integer>(m_bytes.data() + m_offset);
    m_offset += size;
    return true;
  }
  bool boolean(bool &value) {
    quint8 byte = 0;
    if (!number(byte)) return false;
    if (byte > 1) { fail(CodecError::InvalidBoolean); return false; }
    value = byte == 1; return true;
  }
  bool text(QString &value, qsizetype maximum) {
    quint32 size = 0;
    if (!number(size)) return false;
    if (size > static_cast<quint32>(maximum)) { fail(CodecError::PayloadTooLarge); return false; }
    if (!room(static_cast<qsizetype>(size))) return false;
    const QByteArrayView bytes(m_bytes.data() + m_offset, static_cast<qsizetype>(size));
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless | QStringConverter::Flag::ConvertInitialBom);
    QString decoded = decoder.decode(bytes);
    if (decoder.hasError() || decoded.contains(QChar::Null)
        || decoded.toUtf8() != bytes) { fail(CodecError::InvalidUtf8); return false; }
    m_offset += static_cast<qsizetype>(size); value = std::move(decoded); return true;
  }
  bool count(quint32 &value, qsizetype maximum) {
    if (!number(value)) return false;
    if (value > static_cast<quint32>(maximum)) { fail(CodecError::PayloadTooLarge); return false; }
    return true;
  }
  bool header(MessageKind kind) {
    if (!room(4)) return false;
    if (m_bytes.first(4) != QByteArrayView(kEnvelopeMagic, 4)) {
      fail(CodecError::InvalidMagic); return false;
    }
    m_offset += 4;
    quint32 version = 0, type = 0;
    if (!number(version) || !number(type)) return false;
    if (version != kCodecVersion) { fail(CodecError::UnsupportedCodecVersion); return false; }
    if (type != static_cast<quint32>(kind)) { fail(CodecError::WrongMessageKind); return false; }
    return true;
  }
  template<class Enum> bool enumeration(Enum &value) {
    quint32 numberValue = 0;
    if (!number(numberValue)) return false;
    value = static_cast<Enum>(numberValue); return true;
  }
  void fail(CodecError error) { if (m_error == CodecError::None) m_error = error; }
  bool complete() {
    if (m_error == CodecError::None && m_offset != m_bytes.size()) fail(CodecError::TrailingBytes);
    return m_error == CodecError::None;
  }
  [[nodiscard]] CodecResult result() const { return {m_error, ValueError::None}; }
private:
  bool room(qsizetype size) {
    if (m_error != CodecError::None) return false;
    if (size > m_bytes.size() - m_offset) { fail(CodecError::Truncated); return false; }
    return true;
  }
  QByteArrayView m_bytes;
  qsizetype m_offset = 0;
  CodecError m_error = CodecError::None;
};
class Writer final {
public:
  Writer(MessageKind kind, qsizetype maximum) : m_maximum(maximum) {
    m_bytes.append(kEnvelopeMagic, 4); number(kCodecVersion); number(static_cast<quint32>(kind));
  }
  template<class Integer> void number(Integer value) {
    std::array<char, sizeof(Integer)> bytes{};
    qToLittleEndian<Integer>(value, bytes.data());
    append(QByteArrayView(bytes.data(), static_cast<qsizetype>(bytes.size())));
  }
  void boolean(bool value) { number(quint8(value ? 1 : 0)); }
  void text(const QString &value) {
    const QByteArray bytes = value.toUtf8();
    number(static_cast<quint32>(bytes.size())); append(bytes);
  }
  template<class Enum> void enumeration(Enum value) { number(static_cast<quint32>(value)); }
  [[nodiscard]] EncodeResult finish() {
    if (!m_good) return {{CodecError::PayloadTooLarge, ValueError::None}, {}};
    return {{}, std::move(m_bytes)};
  }
private:
  void append(QByteArrayView bytes) {
    if (!m_good) return;
    if (bytes.size() > m_maximum - m_bytes.size()) { m_good = false; return; }
    m_bytes.append(bytes.data(), bytes.size());
  }
  QByteArray m_bytes;
  qsizetype m_maximum;
  bool m_good = true;
};
inline EncodeResult invalid(ValidationResult result) {
  return {{CodecError::InvalidValue, result.error}, {}};
}
void writeLineage(Writer &writer, const Lineage &value);
bool readLineage(Reader &reader, Lineage &value);
void writeAttachment(Writer &writer, const Attachment &value);
bool readAttachment(Reader &reader, Attachment &value);
void writeDiagnostic(Writer &writer, const Diagnostic &value);
bool readDiagnostic(Reader &reader, Diagnostic &value);
void writeRequest(Writer &writer, const ActionRequest &value);
bool readRequest(Reader &reader, ActionRequest &value);
void writeRow(Writer &writer, const VolumeRow &value);
bool readRow(Reader &reader, VolumeRow &value);
template<class Value, class Validate> CodecResult publish(Reader &reader, Value &&value,
    Value &destination, Validate validate) {
  if (!reader.complete()) return reader.result();
  const auto result = validate(value);
  if (!result.accepted()) return {CodecError::InvalidValue, result.error};
  destination = std::move(value); return {};
}
} // namespace QindaQt::RemovableMedia::CodecPrivate
