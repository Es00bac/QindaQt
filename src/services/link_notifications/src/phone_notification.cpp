// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/phone_notification.h>

#include <QtCore/QRegularExpression>

namespace QindaQt::Link {
namespace {

bool str(const QJsonObject &o, const char *key, qsizetype min, qsizetype max, QString *out)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isString()) return false;
    const QString s = v.toString();
    if (s.size() < min || s.size() > max) return false;
    *out = s;
    return true;
}

bool wholeNumber(const QJsonValue &v, qint64 *out)
{
    if (!v.isDouble()) return false;
    const double d = v.toDouble();
    if (d < 0 || d > 9007199254740991.0 || d != double(qint64(d))) return false;
    *out = qint64(d);
    return true;
}

} // namespace

bool isIconHash(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("\\A[0-9a-f]{64}\\z"));
    return re.match(s).hasMatch();
}

bool PhoneNotification::hasReply() const
{
    for (const PhoneAction &a : actions) {
        if (a.reply) return true;
    }
    return false;
}

QJsonObject PhoneNotification::toJson() const
{
    QJsonArray acts;
    for (const PhoneAction &a : actions) {
        acts.append(QJsonObject{{QStringLiteral("id"), a.id}, {QStringLiteral("title"), a.title},
                                {QStringLiteral("reply"), a.reply}});
    }
    return {{QStringLiteral("key"), key}, {QStringLiteral("pkg"), pkg}, {QStringLiteral("app"), app},
            {QStringLiteral("title"), title}, {QStringLiteral("text"), text},
            {QStringLiteral("when"), double(when)}, {QStringLiteral("icon"), icon},
            {QStringLiteral("actions"), acts}};
}

bool parseNotification(const QJsonObject &o, PhoneNotification *out)
{
    PhoneNotification n;
    if (!str(o, "key", 1, NotifyLimits::kKey, &n.key) || !str(o, "pkg", 1, NotifyLimits::kPackage, &n.pkg)
        || !str(o, "app", 1, NotifyLimits::kApp, &n.app) || !str(o, "title", 0, NotifyLimits::kTitle, &n.title)
        || !str(o, "text", 0, NotifyLimits::kText, &n.text) || !wholeNumber(o.value(QStringLiteral("when")), &n.when)) {
        return false;
    }
    const QJsonValue icon = o.value(QStringLiteral("icon"));
    if (!icon.isString() || (!icon.toString().isEmpty() && !isIconHash(icon.toString()))) return false;
    n.icon = icon.toString();
    const QJsonValue acts = o.value(QStringLiteral("actions"));
    if (!acts.isArray() || acts.toArray().size() > NotifyLimits::kActions) return false;
    for (const QJsonValue &v : acts.toArray()) {
        const QJsonObject a = v.toObject();
        PhoneAction action;
        if (!v.isObject() || !str(a, "id", 1, NotifyLimits::kActionId, &action.id)
            || !str(a, "title", 1, NotifyLimits::kActionTitle, &action.title) || !a.value(QStringLiteral("reply")).isBool()) {
            return false;
        }
        action.reply = a.value(QStringLiteral("reply")).toBool();
        n.actions.append(action);
    }
    *out = std::move(n);
    return true;
}

bool parseSnapshot(const QJsonObject &b, QList<PhoneNotification> *out)
{
    const QJsonValue items = b.value(QStringLiteral("items"));
    if (!items.isArray() || items.toArray().size() > NotifyLimits::kSnapshotItems) return false;
    QList<PhoneNotification> list;
    for (const QJsonValue &v : items.toArray()) {
        PhoneNotification n;
        if (!v.isObject() || !parseNotification(v.toObject(), &n)) return false;
        list.append(n);
    }
    *out = std::move(list);
    return true;
}

Envelope makeDismiss(const QString &key)
{
    Envelope e;
    e.t = QStringLiteral("notify.dismiss");
    e.b = QJsonObject{{QStringLiteral("key"), key}};
    return e;
}

Envelope makeAction(const QString &key, const QString &actionId, const QString &title, const QString &replyText,
                    bool hasText)
{
    Envelope e;
    e.t = QStringLiteral("notify.action");
    e.b = QJsonObject{{QStringLiteral("key"), key}, {QStringLiteral("action"), actionId},
                  {QStringLiteral("title"), title.left(NotifyLimits::kActionTitle)}};
    if (hasText) e.b.insert(QStringLiteral("text"), replyText);
    return e;
}

Envelope makeIconRequest(const QString &hash)
{
    Envelope e;
    e.t = QStringLiteral("notify.icon");
    e.b = QJsonObject{{QStringLiteral("hash"), hash}};
    return e;
}

} // namespace QindaQt::Link
