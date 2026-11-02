// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/ring_feature.h>
#include <qindaqt/services/link_transport/session.h>
#include <qindaqt/services/link_transport/status_feature.h>

namespace QindaQt::Link {
namespace {

bool oneOf(const QString &v, std::initializer_list<const char *> allowed)
{
    for (const char *a : allowed) {
        if (v == QLatin1String(a)) return true;
    }
    return false;
}

// Validates the protocol-page status shape. Missing groups are allowed (the
// phone sends the whole snapshot but a merge keeps older groups).
bool validate(const QJsonObject &b, QJsonObject *out)
{
    const auto bat = b.constFind(QStringLiteral("battery"));
    if (bat != b.constEnd()) {
        const QJsonObject o = bat->toObject();
        const QJsonValue level = o.value(QStringLiteral("level"));
        if (!bat->isObject() || !level.isDouble() || level.toDouble() < 0 || level.toDouble() > 100
            || level.toDouble() != int(level.toDouble()) || !o.value(QStringLiteral("charging")).isBool()
            || !oneOf(o.value(QStringLiteral("plugged")).toString(), {"none", "usb", "ac", "wireless", "dock"})) {
            return false;
        }
        out->insert(QStringLiteral("battery"), o);
    }
    const auto net = b.constFind(QStringLiteral("network"));
    if (net != b.constEnd()) {
        const QJsonObject o = net->toObject();
        const QJsonValue sig = o.value(QStringLiteral("signal"));
        const bool sigOk = sig.isNull() || (sig.isDouble() && sig.toDouble() >= 0 && sig.toDouble() <= 4
                                            && sig.toDouble() == int(sig.toDouble()));
        if (!net->isObject() || !sigOk
            || !oneOf(o.value(QStringLiteral("type")).toString(), {"none", "wifi", "cellular", "ethernet", "other"})) {
            return false;
        }
        out->insert(QStringLiteral("network"), o);
    }
    return !out->isEmpty();
}

} // namespace

void StatusFeature::handle(Session &session, const Envelope &e)
{
    QJsonObject valid;
    if (!validate(e.b, &valid)) {
        if (e.id != 0) {
            session.send(makeError(QStringLiteral("bad-request"), QStringLiteral("invalid status"), e.id));
        }
        return;
    }
    QJsonObject merged = m_status.value(session.deviceId());
    for (auto it = valid.begin(); it != valid.end(); ++it) {
        merged.insert(it.key(), it.value());
    }
    m_status.insert(session.deviceId(), merged);
    Q_EMIT statusChanged(session.deviceId());
}

void StatusFeature::sessionClosed(const QString &deviceId)
{
    if (m_status.remove(deviceId) > 0) {
        Q_EMIT statusChanged(deviceId);
    }
}

void RingFeature::handle(Session &session, const Envelope &e)
{
    if (e.id != 0) {
        session.send(makeError(QStringLiteral("unsupported"), QStringLiteral("ring is desktop to phone"), e.id));
    }
}

void RingFeature::ring(Session &session, std::function<void(bool, const QString &)> done)
{
    if (session.state() != SessionState::Ready || !session.peerHasCap(QStringLiteral("ring"))) {
        done(false, QStringLiteral("unsupported"));
        return;
    }
    Envelope e;
    e.t = QStringLiteral("ring");
    session.request(e, [done = std::move(done)](const Envelope &reply) {
        done(reply.t == QLatin1String("ok"),
             reply.t == QLatin1String("ok") ? QString() : reply.b.value(QStringLiteral("code")).toString());
    });
}

} // namespace QindaQt::Link
