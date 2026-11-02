// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_messages/message_model.h>

#include <QtCore/QJsonArray>

namespace QindaQt::Link {
namespace {

using namespace MessageBounds;

QString k(const char *s) { return QString::fromLatin1(s); }

bool isInt(const QJsonValue &v, qint64 min, qint64 max, qint64 *out = nullptr)
{
    if (!v.isDouble()) return false;
    const double d = v.toDouble();
    if (d != double(qint64(d)) || d < double(min) || d > double(max)) return false;
    if (out != nullptr) *out = qint64(d);
    return true;
}

constexpr qint64 kMaxId = (qint64{1} << 53) - 1;

bool str(const QJsonValue &v, int maxChars, QString *out)
{
    if (!v.isString()) return false;
    const QString s = v.toString();
    if (s.size() > maxChars) return false;
    *out = s;
    return true;
}

bool copyInt(const QJsonObject &in, QJsonObject *out, const char *key, qint64 min = 0, qint64 max = kMaxId)
{
    if (!isInt(in.value(k(key)), min, max)) return false;
    out->insert(k(key), in.value(k(key)));
    return true;
}

bool copyStr(const QJsonObject &in, QJsonObject *out, const char *key, int maxChars)
{
    QString s;
    if (!str(in.value(k(key)), maxChars, &s)) return false;
    out->insert(k(key), s);
    return true;
}

bool copyBool(const QJsonObject &in, QJsonObject *out, const char *key)
{
    if (!in.value(k(key)).isBool()) return false;
    out->insert(k(key), in.value(k(key)));
    return true;
}

std::optional<QJsonObject> thread(const QJsonValue &v)
{
    if (!v.isObject()) return std::nullopt;
    const QJsonObject in = v.toObject();
    QJsonObject out;
    if (!copyInt(in, &out, "id") || !copyInt(in, &out, "date") || !copyInt(in, &out, "unread", 0, 1'000'000)
        || !copyInt(in, &out, "count", 0, 10'000'000) || !copyStr(in, &out, "snippet", 200)) {
        return std::nullopt;
    }
    const QJsonValue addrs = in.value(k("addrs")), names = in.value(k("names"));
    if (!addrs.isArray() || !names.isArray() || addrs.toArray().size() > 20
        || addrs.toArray().size() != names.toArray().size()) {
        return std::nullopt;
    }
    QJsonArray a, n;
    QString s;
    for (const QJsonValue &x : addrs.toArray()) {
        if (!str(x, 64, &s)) return std::nullopt;
        a.append(s);
    }
    for (const QJsonValue &x : names.toArray()) {
        if (!str(x, 128, &s)) return std::nullopt;
        n.append(s);
    }
    out.insert(k("addrs"), a);
    out.insert(k("names"), n);
    return out;
}

std::optional<QJsonObject> part(const QJsonValue &v)
{
    if (!v.isObject()) return std::nullopt;
    const QJsonObject in = v.toObject();
    QJsonObject out;
    if (!copyInt(in, &out, "id") || !copyStr(in, &out, "mime", 128) || !copyStr(in, &out, "name", 128)
        || !copyInt(in, &out, "size", -1, qint64{1} << 40)) {
        return std::nullopt;
    }
    return out;
}

std::optional<QJsonObject> message(const QJsonValue &v)
{
    if (!v.isObject()) return std::nullopt;
    const QJsonObject in = v.toObject();
    QJsonObject out;
    const QString kind = in.value(k("kind")).toString(), box = in.value(k("box")).toString();
    static const QStringList boxes = {k("inbox"), k("sent"), k("outbox"), k("failed"), k("queued"), k("draft")};
    if (!copyInt(in, &out, "id") || !copyInt(in, &out, "date") || (kind != k("sms") && kind != k("mms"))
        || !boxes.contains(box) || !copyStr(in, &out, "addr", 64) || !copyStr(in, &out, "body", 4000)
        || !copyBool(in, &out, "trunc") || !copyBool(in, &out, "read") || !in.value(k("parts")).isArray()
        || in.value(k("parts")).toArray().size() > 16) {
        return std::nullopt;
    }
    out.insert(k("kind"), kind);
    out.insert(k("box"), box);
    QJsonArray parts;
    for (const QJsonValue &p : in.value(k("parts")).toArray()) {
        const auto sp = part(p);
        if (!sp) return std::nullopt;
        parts.append(*sp);
    }
    out.insert(k("parts"), parts);
    return out;
}

template <typename F>
std::optional<QJsonObject> list(const QJsonObject &body, const char *key, int max, F each, QJsonObject *out)
{
    const QJsonValue v = body.value(k(key));
    if (!v.isArray() || v.toArray().size() > max) return std::nullopt;
    QJsonArray arr;
    for (const QJsonValue &x : v.toArray()) {
        const auto s = each(x);
        if (!s) return std::nullopt;
        arr.append(*s);
    }
    out->insert(k(key), arr);
    return *out;
}

} // namespace

std::optional<QJsonObject> sanitizeThreadsReply(const QJsonObject &body)
{
    QJsonObject out;
    if (!copyBool(body, &out, "more") || !copyInt(body, &out, "unread", 0, 100'000'000)) return std::nullopt;
    return list(body, "threads", kMaxThreads, thread, &out);
}

std::optional<QJsonObject> sanitizeMessagesReply(const QJsonObject &body)
{
    QJsonObject out;
    if (!copyInt(body, &out, "thread") || !copyBool(body, &out, "more")) return std::nullopt;
    return list(body, "messages", kMaxMessages, message, &out);
}

std::optional<QJsonObject> sanitizePartReply(const QJsonObject &body)
{
    QJsonObject out;
    if (!copyInt(body, &out, "part") || !copyStr(body, &out, "mime", 128) || !copyInt(body, &out, "size")
        || !copyBool(body, &out, "scaled") || !body.value(k("data")).isString()
        || body.value(k("data")).toString().size() > kMaxPartBase64) {
        return std::nullopt;
    }
    out.insert(k("data"), body.value(k("data")));
    return out;
}

std::optional<QJsonObject> sanitizeSendReply(const QJsonObject &body)
{
    QJsonObject out;
    if (!copyInt(body, &out, "ref", 1)) return std::nullopt;
    return out;
}

std::optional<QJsonObject> sanitizeReadReply(const QJsonObject &body)
{
    QJsonObject out;
    if (!copyInt(body, &out, "thread") || !copyInt(body, &out, "unread", 0, 100'000'000)) return std::nullopt;
    return out;
}

std::optional<QJsonObject> sanitizeChanged(const QJsonObject &body)
{
    QJsonObject out;
    if (!copyBool(body, &out, "all") || !copyInt(body, &out, "unread", 0, 100'000'000)) return std::nullopt;
    return list(body, "threads", kMaxThreads, [](const QJsonValue &v) -> std::optional<QJsonValue> {
        if (!isInt(v, 0, kMaxId)) return std::nullopt;
        return v;
    }, &out);
}

std::optional<QJsonObject> sanitizeSentEvent(const QJsonObject &body)
{
    QJsonObject out;
    const QString state = body.value(k("state")).toString();
    if (!copyInt(body, &out, "ref", 1) || (state != k("sent") && state != k("failed") && state != k("delivered"))) {
        return std::nullopt;
    }
    out.insert(k("state"), state);
    if (state == k("failed")) {
        QString code;
        out.insert(k("code"), str(body.value(k("code")), 32, &code) ? code : k("unknown"));
    }
    return out;
}

bool UnreadTotals::set(const QString &deviceId, int unread)
{
    const int before = total();
    m_unread.insert(deviceId, unread);
    return total() != before;
}

bool UnreadTotals::remove(const QString &deviceId)
{
    const int before = total();
    m_unread.remove(deviceId);
    return total() != before;
}

int UnreadTotals::total() const
{
    int n = 0;
    for (int v : m_unread) n += v;
    return n;
}

} // namespace QindaQt::Link
