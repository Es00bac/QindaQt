// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_messages/messages_feature.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QPointer>

namespace QindaQt::Link {
namespace {

using namespace MessageBounds;
constexpr qsizetype kMaxTrackedRefs = 64;

QString k(const char *s) { return QString::fromLatin1(s); }

void cursor(QJsonObject *b, qint64 before, qint64 beforeId)
{
    if (before > 0) { // 0 and below mean "newest"; a real date is never 0
        b->insert(k("before"), before);
        b->insert(k("beforeId"), qMax<qint64>(beforeId, 0));
    }
}

} // namespace

MessagesFeature::MessagesFeature(SessionLookup lookup, QObject *parent)
    : QObject(parent), m_lookup(std::move(lookup))
{
}

void MessagesFeature::ask(const QString &deviceId, const QString &type, const QJsonObject &body,
                          std::function<std::optional<QJsonObject>(const QJsonObject &)> validate,
                          std::function<void(const QJsonObject &)> onOk, Reply done)
{
    Session *s = m_lookup ? m_lookup(deviceId) : nullptr;
    if (s == nullptr || s->state() != SessionState::Ready) {
        done({}, QStringLiteral("not-connected"));
        return;
    }
    if (!s->peerHasCap(QStringLiteral("sms"))) {
        done({}, QStringLiteral("unsupported"));
        return;
    }
    Envelope e;
    e.t = type;
    e.b = body;
    QPointer<MessagesFeature> self(this);
    const QString requestType = e.t;
    s->request(e, [self, requestType, validate = std::move(validate), onOk = std::move(onOk), done = std::move(done)](const Envelope &r) {
        if (r.t == QLatin1String("error")) {
            done({}, r.b.value(QStringLiteral("code")).toString(QStringLiteral("internal")));
            return;
        }
        // A reply must be the request's own type; anything else is a confused or hostile phone.
        const auto clean = r.t == requestType ? validate(r.b) : std::nullopt;
        if (!clean) {
            qWarning("link sms: reply failed validation (%.32s)", qPrintable(r.t));
            done({}, QStringLiteral("bad-reply"));
            return;
        }
        if (self) onOk(*clean);
        done(*clean, QString());
    });
}

void MessagesFeature::listThreads(const QString &deviceId, int limit, qint64 before, qint64 beforeId, Reply done)
{
    QJsonObject b{{k("limit"), qBound(1, limit, kMaxThreads)}};
    cursor(&b, before, beforeId);
    QPointer<MessagesFeature> self(this);
    ask(deviceId, k("sms.threads"), b, sanitizeThreadsReply,
        [self, deviceId](const QJsonObject &r) {
            if (self && self->m_unread.set(deviceId, r.value(k("unread")).toInt())) {
                Q_EMIT self->unreadTotalChanged(self->m_unread.total());
            }
        },
        std::move(done));
}

void MessagesFeature::listMessages(const QString &deviceId, qint64 thread, int limit, qint64 before,
                                   const QString &beforeKind, qint64 beforeId, Reply done)
{
    if (!beforeKind.isEmpty() && beforeKind != k("sms") && beforeKind != k("mms")) {
        done({}, QStringLiteral("bad-request"));
        return;
    }
    QJsonObject b{{k("thread"), thread}, {k("limit"), qBound(1, limit, kMaxMessages)}};
    cursor(&b, before, beforeId);
    if (before > 0 && !beforeKind.isEmpty()) b.insert(k("beforeKind"), beforeKind);
    ask(deviceId, k("sms.messages"), b, sanitizeMessagesReply, [](const QJsonObject &) {}, std::move(done));
}

void MessagesFeature::getPart(const QString &deviceId, qint64 part, int maxEdge, Reply done)
{
    ask(deviceId, k("sms.part"), QJsonObject{{k("part"), part}, {k("maxEdge"), qBound(64, maxEdge, 2048)}},
        sanitizePartReply, [](const QJsonObject &) {}, std::move(done));
}

void MessagesFeature::send(const QString &deviceId, const QStringList &addrs, const QString &text, int subscription,
                           Reply done)
{
    bool ok = !addrs.isEmpty() && addrs.size() <= kMaxSendAddrs && !text.isEmpty() && text.size() <= kMaxSendText;
    for (const QString &a : addrs) ok = ok && !a.trimmed().isEmpty() && a.size() <= 32;
    if (!ok) {
        done({}, QStringLiteral("bad-request"));
        return;
    }
    QJsonObject b{{k("addrs"), QJsonArray::fromStringList(addrs)}, {k("text"), text}};
    if (subscription >= 0) b.insert(k("sub"), subscription);
    QPointer<MessagesFeature> self(this);
    ask(deviceId, k("sms.send"), b, sanitizeSendReply,
        [self, deviceId](const QJsonObject &r) {
            if (!self) return;
            QSet<qint64> &refs = self->m_sendRefs[deviceId];
            if (refs.size() >= kMaxTrackedRefs) refs.clear();
            refs.insert(qint64(r.value(k("ref")).toDouble()));
        },
        std::move(done));
}

void MessagesFeature::markRead(const QString &deviceId, qint64 thread, Reply done)
{
    QPointer<MessagesFeature> self(this);
    ask(deviceId, k("sms.read"), QJsonObject{{k("thread"), thread}}, sanitizeReadReply,
        [self, deviceId](const QJsonObject &r) {
            if (self && self->m_unread.set(deviceId, r.value(k("unread")).toInt())) {
                Q_EMIT self->unreadTotalChanged(self->m_unread.total());
            }
        },
        std::move(done));
}

void MessagesFeature::handle(Session &session, const Envelope &e)
{
    const QString id = session.deviceId();
    if (e.t == QLatin1String("sms.changed")) {
        const auto clean = sanitizeChanged(e.b);
        if (!clean) return;
        if (m_unread.set(id, clean->value(k("unread")).toInt())) Q_EMIT unreadTotalChanged(m_unread.total());
        Q_EMIT threadsChanged(id, QString::fromUtf8(QJsonDocument(*clean).toJson(QJsonDocument::Compact)));
        return;
    }
    if (e.t == QLatin1String("sms.sent")) {
        const auto clean = sanitizeSentEvent(e.b);
        const qint64 ref = clean ? qint64(clean->value(k("ref")).toDouble()) : 0;
        if (!clean || !m_sendRefs.value(id).contains(ref)) return;
        const QJsonArray unsent = clean->value(k("unsent")).toArray();
        const QString detail = unsent.isEmpty() ? QString()
            : QString::fromUtf8(QJsonDocument(QJsonObject{{k("unsent"), unsent}}).toJson(QJsonDocument::Compact));
        Q_EMIT sendStatus(id, ref, clean->value(k("state")).toString(), clean->value(k("code")).toString(), detail);
        return;
    }
    if (e.id != 0) {
        session.send(makeError(QStringLiteral("unsupported"), QStringLiteral("sms requests are desktop to phone"), e.id));
    }
}

void MessagesFeature::sessionClosed(const QString &deviceId)
{
    m_sendRefs.remove(deviceId);
    if (m_unread.remove(deviceId)) Q_EMIT unreadTotalChanged(m_unread.total());
}

} // namespace QindaQt::Link
