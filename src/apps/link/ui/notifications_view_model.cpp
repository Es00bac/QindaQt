// SPDX-License-Identifier: GPL-3.0-or-later
#include "notifications_view_model.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <algorithm>

namespace QindaQt::LinkApp {

NotificationsViewModel::NotificationsViewModel(LinkApi &api, QObject *parent) : QObject(parent), m_api(api)
{
    connect(&m_api, &LinkApi::availableChanged, this, [this] { if (m_api.available()) refresh(); });
}

int NotificationsViewModel::mutedCount() const
{
    return int(std::count_if(m_all.cbegin(), m_all.cend(), [](const QVariantMap &r) { return r.value(QStringLiteral("muted")).toBool(); }));
}

void NotificationsViewModel::setSearchText(const QString &text)
{
    if (text == m_search) return;
    m_search = text;
    rebuild();
    Q_EMIT searchTextChanged();
}

void NotificationsViewModel::refresh()
{
    m_api.listApps([this](const QString &json, const QString &error) {
        if (!error.isEmpty()) {
            m_error = error == QLatin1String("unavailable") ? tr("The QindaLink service is not running.")
                                                           : tr("The notification list could not be loaded (%1).").arg(error);
            Q_EMIT stateChanged();
            return;
        }
        m_all.clear();
        for (const QJsonValue &v : QJsonDocument::fromJson(json.toUtf8()).array()) {
            const QJsonObject o = v.toObject();
            const QString pkg = o.value(QStringLiteral("pkg")).toString();
            if (pkg.isEmpty()) continue;
            m_all.append({{QStringLiteral("pkg"), pkg},
                          {QStringLiteral("label"), o.value(QStringLiteral("label")).toString(pkg)},
                          {QStringLiteral("muted"), o.value(QStringLiteral("muted")).toBool()}});
        }
        std::sort(m_all.begin(), m_all.end(), [](const QVariantMap &a, const QVariantMap &b) {
            return QString::localeAwareCompare(a.value(QStringLiteral("label")).toString(), b.value(QStringLiteral("label")).toString()) < 0;
        });
        m_error.clear();
        m_loaded = true;
        rebuild();
        Q_EMIT stateChanged();
    });
}

void NotificationsViewModel::setMuted(const QString &pkg, bool muted)
{
    for (QVariantMap &row : m_all) {
        if (row.value(QStringLiteral("pkg")).toString() == pkg) row.insert(QStringLiteral("muted"), muted);
    }
    m_api.setAppMuted(pkg, muted);
    rebuild();
    Q_EMIT stateChanged();
}

void NotificationsViewModel::rebuild()
{
    const QString needle = m_search.trimmed().toLower();
    QList<QVariantMap> rows;
    for (const QVariantMap &row : std::as_const(m_all)) {
        if (needle.isEmpty() || row.value(QStringLiteral("label")).toString().toLower().contains(needle)
            || row.value(QStringLiteral("pkg")).toString().toLower().contains(needle)) {
            rows.append(row);
        }
    }
    m_visible.setRows(rows);
}

} // namespace QindaQt::LinkApp
