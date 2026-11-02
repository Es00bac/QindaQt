// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_protocol/frame_codec.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>
#include <QtCore/QStringDecoder>

#include <cmath>

namespace QindaQt::Link {
namespace {

// Reads an optional request id. Returns false on a wrong type or range.
bool readId(const QJsonObject &o, const QString &key, qint64 *out)
{
    const auto it = o.constFind(key);
    if (it == o.constEnd()) {
        *out = 0;
        return true;
    }
    if (!it->isDouble()) {
        return false;
    }
    const double d = it->toDouble();
    if (d != std::floor(d) || d < 1.0 || d > double(kMaxEnvelopeId)) {
        return false;
    }
    *out = qint64(d);
    return true;
}

DecodeResult fail(DecodeStatus status)
{
    DecodeResult r;
    r.status = status;
    return r;
}

} // namespace

DecodeResult decodePayload(const QByteArray &payload)
{
    // AGENT-GUARD: Qt's JSON parser accepts some invalid UTF-8 and skips a
    // BOM, so both are rejected here before parsing. Vectors invalid-utf8 and
    // utf8-bom pin this.
    if (payload.startsWith("\xEF\xBB\xBF")) {
        return fail(DecodeStatus::BadPayload);
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString text = utf8.decode(payload);
    if (utf8.hasError()) {
        return fail(DecodeStatus::BadPayload);
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return fail(DecodeStatus::BadPayload);
    }
    const QJsonObject root = doc.object();

    Envelope e;
    const QJsonValue t = root.value(QStringLiteral("t"));
    if (!t.isString() || !isValidFrameType(t.toString())) {
        return fail(DecodeStatus::BadEnvelope);
    }
    e.t = t.toString();
    if (!readId(root, QStringLiteral("id"), &e.id) || !readId(root, QStringLiteral("re"), &e.re)) {
        return fail(DecodeStatus::BadEnvelope);
    }
    const auto body = root.constFind(QStringLiteral("b"));
    if (body != root.constEnd()) {
        if (!body->isObject()) {
            return fail(DecodeStatus::BadEnvelope);
        }
        e.b = body->toObject();
    }
    DecodeResult r;
    r.status = DecodeStatus::Frame;
    r.envelope = std::move(e);
    return r;
}

void FrameDecoder::append(const QByteArray &bytes)
{
    if (m_failed == DecodeStatus::NeedsMore) {
        m_buffer.append(bytes);
    }
}

DecodeResult FrameDecoder::next()
{
    if (m_failed != DecodeStatus::NeedsMore) {
        return fail(m_failed);
    }
    if (m_buffer.size() < 4) {
        return fail(DecodeStatus::NeedsMore);
    }
    const quint32 length = (quint32(quint8(m_buffer[0])) << 24) | (quint32(quint8(m_buffer[1])) << 16)
        | (quint32(quint8(m_buffer[2])) << 8) | quint32(quint8(m_buffer[3]));
    if (length == 0 || length > quint32(kMaxPayloadBytes)) {
        m_failed = DecodeStatus::BadLength;
        m_buffer.clear();
        return fail(m_failed);
    }
    if (m_buffer.size() < qsizetype(4 + length)) {
        return fail(DecodeStatus::NeedsMore);
    }
    const QByteArray payload = m_buffer.mid(4, qsizetype(length));
    m_buffer.remove(0, qsizetype(4 + length));
    DecodeResult r = decodePayload(payload);
    if (r.status != DecodeStatus::Frame) {
        m_failed = r.status;
        m_buffer.clear();
    }
    return r;
}

QByteArray encodeFrame(const Envelope &envelope)
{
    QJsonObject root;
    root.insert(QStringLiteral("t"), envelope.t);
    if (envelope.id != 0) {
        root.insert(QStringLiteral("id"), double(envelope.id));
    }
    if (envelope.re != 0) {
        root.insert(QStringLiteral("re"), double(envelope.re));
    }
    if (!envelope.b.isEmpty()) {
        root.insert(QStringLiteral("b"), envelope.b);
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (payload.isEmpty() || payload.size() > kMaxPayloadBytes) {
        return {};
    }
    QByteArray frame;
    frame.reserve(payload.size() + 4);
    const quint32 n = quint32(payload.size());
    frame.append(char(n >> 24)).append(char(n >> 16)).append(char(n >> 8)).append(char(n));
    frame.append(payload);
    return frame;
}

} // namespace QindaQt::Link
