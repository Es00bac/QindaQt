// SPDX-License-Identifier: GPL-3.0-or-later
#include "message_json.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QLocale>
#include <QtCore/QObject>

#include <algorithm>

namespace QindaQt::LinkApp {

QString formatPairingCode(const QString &raw)
{
    if (raw.size() != 6) return raw;
    for (const QChar c : raw) {
        if (!c.isDigit()) return raw;
    }
    return raw.left(3) + QLatin1Char(' ') + raw.mid(3);
}

QString formatWhen(qint64 ms, qint64 nowMs)
{
    if (ms <= 0) return {};
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(ms);
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(nowMs);
    const qint64 days = when.date().daysTo(now.date());
    const QLocale locale;
    if (days == 0) return locale.toString(when.time(), QStringLiteral("HH:mm"));
    if (days > 0 && days < 7) return locale.toString(when.date(), QStringLiteral("ddd"));
    return locale.toString(when.date(), QStringLiteral("d MMM"));
}

QString messagesErrorText(const QString &code)
{
    if (code == QLatin1String("denied")) {
        return QObject::tr("The phone has not allowed QindaLink to use messages. Run Set up phone on the Devices page.");
    }
    if (code == QLatin1String("unsupported")) return QObject::tr("The phone does not support this.");
    if (code == QLatin1String("not-connected") || code == QLatin1String("unavailable")) {
        return QObject::tr("The phone is not connected.");
    }
    if (code == QLatin1String("timeout")) return QObject::tr("The phone did not answer in time.");
    if (code == QLatin1String("bad-request")) return QObject::tr("The phone refused the request.");
    return QObject::tr("The request failed (%1).").arg(code);
}

QVariantMap threadRow(const QJsonObject &thread, qint64 nowMs)
{
    QStringList addrs;
    QStringList names;
    for (const QJsonValue &a : thread.value(QStringLiteral("addrs")).toArray()) addrs << a.toString();
    for (const QJsonValue &n : thread.value(QStringLiteral("names")).toArray()) names << n.toString();
    QStringList labels;
    for (qsizetype i = 0; i < addrs.size(); ++i) {
        labels << (names.value(i).isEmpty() ? addrs.at(i) : names.at(i));
    }
    const QString title = labels.isEmpty() ? QStringLiteral("(no address)") : labels.join(QStringLiteral(", "));
    const qint64 date = qint64(thread.value(QStringLiteral("date")).toDouble());
    QVariantMap row;
    row.insert(QStringLiteral("threadId"), qint64(thread.value(QStringLiteral("id")).toDouble()));
    row.insert(QStringLiteral("title"), title);
    row.insert(QStringLiteral("snippet"), thread.value(QStringLiteral("snippet")).toString());
    row.insert(QStringLiteral("when"), formatWhen(date, nowMs));
    row.insert(QStringLiteral("date"), date);
    row.insert(QStringLiteral("unread"), thread.value(QStringLiteral("unread")).toInt());
    row.insert(QStringLiteral("count"), thread.value(QStringLiteral("count")).toInt());
    row.insert(QStringLiteral("addrs"), addrs);
    row.insert(QStringLiteral("names"), names);
    row.insert(QStringLiteral("searchKey"),
               (title + QLatin1Char(' ') + addrs.join(QLatin1Char(' ')) + QLatin1Char(' ')
                + thread.value(QStringLiteral("snippet")).toString())
                   .toLower());
    return row;
}

QVariantMap messageRow(const QJsonObject &message, qint64 nowMs)
{
    const QString kind = message.value(QStringLiteral("kind")).toString(QStringLiteral("sms"));
    const qint64 id = qint64(message.value(QStringLiteral("id")).toDouble());
    const qint64 date = qint64(message.value(QStringLiteral("date")).toDouble());
    const QString box = message.value(QStringLiteral("box")).toString();
    QVariantList parts;
    qint64 imagePart = -1;
    for (const QJsonValue &p : message.value(QStringLiteral("parts")).toArray()) {
        const QJsonObject part = p.toObject();
        const QString mime = part.value(QStringLiteral("mime")).toString();
        const qint64 partId = qint64(part.value(QStringLiteral("id")).toDouble());
        parts.append(QVariantMap{{QStringLiteral("id"), partId},
                                 {QStringLiteral("mime"), mime},
                                 {QStringLiteral("name"), part.value(QStringLiteral("name")).toString()},
                                 {QStringLiteral("size"), qint64(part.value(QStringLiteral("size")).toDouble(-1))}});
        if (imagePart < 0 && mime.startsWith(QLatin1String("image/"))) imagePart = partId;
    }
    QVariantMap row;
    row.insert(QStringLiteral("key"), kind + QLatin1Char(':') + QString::number(id));
    row.insert(QStringLiteral("kind"), kind);
    row.insert(QStringLiteral("id"), id);
    row.insert(QStringLiteral("date"), date);
    row.insert(QStringLiteral("when"), formatWhen(date, nowMs));
    row.insert(QStringLiteral("outgoing"), box != QLatin1String("inbox"));
    row.insert(QStringLiteral("box"), box);
    row.insert(QStringLiteral("body"), message.value(QStringLiteral("body")).toString());
    row.insert(QStringLiteral("trunc"), message.value(QStringLiteral("trunc")).toBool());
    row.insert(QStringLiteral("parts"), parts);
    row.insert(QStringLiteral("imagePart"), imagePart);
    row.insert(QStringLiteral("image"), QString());
    row.insert(QStringLiteral("imageState"), imagePart >= 0 ? QStringLiteral("none") : QStringLiteral("absent"));
    row.insert(QStringLiteral("status"), box == QLatin1String("failed") ? QStringLiteral("failed") : QString());
    row.insert(QStringLiteral("statusText"), QString());
    row.insert(QStringLiteral("local"), false);
    return row;
}

bool messageOlder(const QVariantMap &a, const QVariantMap &b)
{
    const qint64 da = a.value(QStringLiteral("date")).toLongLong();
    const qint64 db = b.value(QStringLiteral("date")).toLongLong();
    if (da != db) return da < db;
    // Newest-first wire order puts mms before sms, so oldest-first puts sms before mms.
    const bool mmsA = a.value(QStringLiteral("kind")).toString() == QLatin1String("mms");
    const bool mmsB = b.value(QStringLiteral("kind")).toString() == QLatin1String("mms");
    if (mmsA != mmsB) return !mmsA;
    return a.value(QStringLiteral("id")).toLongLong() < b.value(QStringLiteral("id")).toLongLong();
}

QList<QVariantMap> mergeMessages(const QList<QVariantMap> &existing, QList<QVariantMap> incoming)
{
    QHash<QString, QVariantMap> byKey;
    for (const QVariantMap &row : existing) byKey.insert(row.value(QStringLiteral("key")).toString(), row);
    for (QVariantMap &row : incoming) {
        const QString key = row.value(QStringLiteral("key")).toString();
        const auto old = byKey.constFind(key);
        if (old != byKey.cend()) {
            // Keep what the client resolved for this row (a loaded image) and replace the wire fields.
            row.insert(QStringLiteral("image"), old->value(QStringLiteral("image")));
            row.insert(QStringLiteral("imageState"), old->value(QStringLiteral("imageState")));
        }
        byKey.insert(key, row);
    }
    QList<QVariantMap> out = byKey.values();
    std::sort(out.begin(), out.end(), messageOlder);
    return out;
}

} // namespace QindaQt::LinkApp
