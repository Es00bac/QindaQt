// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_focus/focus_store.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <cmath>

namespace QindaQt::Link {
namespace {

bool token(const QString &v, int minLen, int maxLen)
{
    if (v.size() < minLen || v.size() > maxLen) return false;
    for (QChar c : v) {
        if (c.unicode() <= 0x20 || c.unicode() >= 0x7f) return false;
    }
    return true;
}

QString sanitizeTitle(const QString &raw)
{
    QString out;
    int count = 0;
    const QList<uint> cps = raw.toUcs4();
    for (uint cp : cps) {
        if (count >= FocusStore::kMaxTitleChars) break;
        const bool control = cp < 0x20 || (cp >= 0x7f && cp <= 0x9f);
        const bool bidi = (cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069) || cp == 0x200e
            || cp == 0x200f || cp == 0x061c;
        const bool surrogate = cp >= 0xd800 && cp <= 0xdfff;
        out += (control || bidi || surrogate || cp > 0x10ffff) ? QStringLiteral(" ") : QString::fromUcs4(reinterpret_cast<const char32_t *>(&cp), 1);
        ++count;
    }
    return out.trimmed();
}

} // namespace

FocusStore::Result FocusStore::publish(const QByteArray &json)
{
    if (json.size() > kMaxJsonBytes) return Result::Rejected;
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return Result::Rejected;
    const QJsonObject o = doc.object();
    if (o.isEmpty() || o.value(QStringLiteral("none")).toBool(false)) {
        clear();
        return Result::Cleared;
    }
    const QJsonValue uuid = o.value(QStringLiteral("uuid"));
    const QJsonValue app = o.value(QStringLiteral("appId"));
    const QJsonValue title = o.value(QStringLiteral("title"));
    const QJsonValue rev = o.value(QStringLiteral("revision"));
    if (!uuid.isString() || !app.isString() || !title.isString() || !rev.isDouble()) return Result::Rejected;
    const double r = rev.toDouble();
    if (!std::isfinite(r) || r < 0 || r >= 9007199254740992.0 || r != std::floor(r)) return Result::Rejected;
    if (!token(uuid.toString(), 1, 64) || !token(app.toString(), 0, 128)) return Result::Rejected;
    FocusFact fact;
    fact.uuid = uuid.toString();
    fact.appId = app.toString();
    fact.title = sanitizeTitle(title.toString());
    fact.revision = quint64(r);
    const bool same = m_fact && m_fact->uuid == fact.uuid && m_fact->appId == fact.appId
        && m_fact->title == fact.title && m_fact->revision == fact.revision;
    m_fact = std::move(fact);
    if (!same) Q_EMIT changed();
    return Result::Accepted;
}

void FocusStore::clear()
{
    if (!m_fact) return;
    m_fact.reset();
    Q_EMIT changed();
}

} // namespace QindaQt::Link
