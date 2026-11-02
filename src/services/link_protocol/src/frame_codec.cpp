// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_protocol/frame_codec.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonParseError>
#include <QtCore/QJsonValue>
#include <QtCore/QStringDecoder>

#include <QtCore/QHash>
#include <QtCore/QRegularExpression>

#include <algorithm>

namespace QindaQt::Link {
namespace {

constexpr int kMaxNestingDepth = 64;

// Raw JSON tokens of the top-level "id" and "re" values, plus nesting depth.
// Qt's parser turns 1.0, 1e0 and -0 into plain doubles, so the integer-token
// rule of the protocol page needs the original text.
struct RawScan {
    int maxDepth = 0;
    QHash<QString, QByteArray> tokens;  // key -> raw number token, or "?" for non-numbers
};

RawScan scanPayload(const QByteArray &p)
{
    RawScan scan;
    int depth = 0;
    bool expectKey = false;
    QString lastKey;
    const qsizetype n = p.size();
    for (qsizetype i = 0; i < n; ++i) {
        const char c = p[i];
        if (c == '"') {
            qsizetype j = i + 1;
            QByteArray raw;
            while (j < n && p[j] != '"') {
                if (p[j] == '\\' && j + 1 < n) {
                    raw.append(p[j]);
                    ++j;
                }
                raw.append(p[j]);
                ++j;
            }
            if (depth == 1 && expectKey) {
                lastKey = QString::fromUtf8(raw);
                expectKey = false;
            } else if (depth == 1 && (lastKey == QLatin1String("id") || lastKey == QLatin1String("re"))) {
                scan.tokens.insert(lastKey, "?");
                lastKey.clear();
            }
            i = j;
        } else if (c == '{' || c == '[') {
            if (depth == 1 && !lastKey.isEmpty() && (lastKey == QLatin1String("id") || lastKey == QLatin1String("re"))) {
                scan.tokens.insert(lastKey, "?");
                lastKey.clear();
            }
            ++depth;
            scan.maxDepth = std::max(scan.maxDepth, depth);
            expectKey = c == '{' && depth == 1;
        } else if (c == '}' || c == ']') {
            --depth;
        } else if (c == ',' && depth == 1) {
            expectKey = true;
        } else if (depth == 1 && !expectKey && c != ':' && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            qsizetype j = i;
            while (j < n && p[j] != ',' && p[j] != '}' && p[j] != ' ' && p[j] != '\t' && p[j] != '\n' && p[j] != '\r') {
                ++j;
            }
            if (lastKey == QLatin1String("id") || lastKey == QLatin1String("re")) {
                scan.tokens.insert(lastKey, p.mid(i, j - i));
            }
            lastKey.clear();
            i = j - 1;
        }
    }
    return scan;
}

// Reads an optional request id. Returns false on a wrong type, a non-integer
// token (1.0, 1e0, -0, +1) or a value outside 1..2^53-1.
bool readId(const QJsonObject &o, const RawScan &scan, const QString &key, qint64 *out)
{
    const auto it = o.constFind(key);
    if (it == o.constEnd()) {
        *out = 0;
        return true;
    }
    if (!it->isDouble()) {
        return false;
    }
    const QByteArray token = scan.tokens.value(key);
    static const QRegularExpression integerToken(QRegularExpression::anchoredPattern(QStringLiteral("[1-9][0-9]{0,15}")));
    if (!integerToken.match(QString::fromLatin1(token)).hasMatch()) {
        return false;
    }
    const double d = it->toDouble();
    if (d < 1.0 || d > double(kMaxEnvelopeId)) {
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
    const RawScan scan = scanPayload(payload);
    if (scan.maxDepth > kMaxNestingDepth) {
        return fail(DecodeStatus::BadPayload);
    }
    const QJsonObject root = doc.object();

    Envelope e;
    const QJsonValue t = root.value(QStringLiteral("t"));
    if (!t.isString() || !isValidFrameType(t.toString())) {
        return fail(DecodeStatus::BadEnvelope);
    }
    e.t = t.toString();
    if (!readId(root, scan, QStringLiteral("id"), &e.id) || !readId(root, scan, QStringLiteral("re"), &e.re)) {
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
        // Compact consumed bytes first so the buffer stays bounded by one frame.
        if (m_offset > 0 && m_offset >= m_buffer.size() / 2) {
            m_buffer.remove(0, m_offset);
            m_offset = 0;
        }
        m_buffer.append(bytes);
    }
}

DecodeResult FrameDecoder::next()
{
    if (m_failed != DecodeStatus::NeedsMore) {
        return fail(m_failed);
    }
    const char *base = m_buffer.constData() + m_offset;
    const qsizetype available = m_buffer.size() - m_offset;
    if (available < 4) {
        return fail(DecodeStatus::NeedsMore);
    }
    const quint32 length = (quint32(quint8(base[0])) << 24) | (quint32(quint8(base[1])) << 16)
        | (quint32(quint8(base[2])) << 8) | quint32(quint8(base[3]));
    if (length == 0 || length > quint32(kMaxPayloadBytes)) {
        m_failed = DecodeStatus::BadLength;
        m_buffer.clear();
        m_offset = 0;
        return fail(m_failed);
    }
    if (available < qsizetype(4 + length)) {
        return fail(DecodeStatus::NeedsMore);
    }
    const QByteArray payload = m_buffer.mid(m_offset + 4, qsizetype(length));
    m_offset += qsizetype(4 + length);
    if (m_offset == m_buffer.size()) {
        m_buffer.clear();
        m_offset = 0;
    }
    DecodeResult r = decodePayload(payload);
    if (r.status != DecodeStatus::Frame) {
        m_failed = r.status;
        m_buffer.clear();
        m_offset = 0;
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
