// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QVariantMap>

namespace QindaQt::LinkApp {

// Row builders for the Link1 Messages JSON (docs/wiki/reference/link1-v1.md, "Messages"). Pure and
// stateless so view models and tests share one reading of the wire shape.

// "007123" -> "007 123"; anything that is not six digits is returned unchanged.
[[nodiscard]] QString formatPairingCode(const QString &raw);
// Same-day times as HH:mm, within six days as the weekday, else a short date.
[[nodiscard]] QString formatWhen(qint64 ms, qint64 nowMs);

// {threadId,title,snippet,when,date,unread,count,addrs,names,searchKey}
[[nodiscard]] QVariantMap threadRow(const QJsonObject &thread, qint64 nowMs);
// {key,kind,id,date,when,outgoing,box,body,trunc,parts,imagePart,image,imageState,status,local}
[[nodiscard]] QVariantMap messageRow(const QJsonObject &message, qint64 nowMs);

// A user-facing reason for a Messages error code (denied, unsupported, not-connected, timeout, ...).
// `sending` words a phone-side `denied` for a send (permission or a non-dialable number).
// "caller-denied" is the service refusing THIS program (ADR-0438); "rate-limited" is its send limit.
[[nodiscard]] QString messagesErrorText(const QString &code, bool sending = false);

// Total order of the provider: oldest first by (date, kind with sms before mms, id). Newest-first
// is the reverse; the wire pages in newest-first order (mms before sms among equals).
[[nodiscard]] bool messageOlder(const QVariantMap &a, const QVariantMap &b);

// Merges a page into the existing chronological rows: incoming rows win for an equal key, the
// result is unique and sorted oldest first.
[[nodiscard]] QList<QVariantMap> mergeMessages(const QList<QVariantMap> &existing, QList<QVariantMap> incoming);

} // namespace QindaQt::LinkApp
