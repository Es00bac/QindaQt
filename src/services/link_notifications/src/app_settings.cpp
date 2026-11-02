// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/app_settings.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

#include <algorithm>

namespace QindaQt::Link {

void AppSettings::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(f.read(1'000'000)).object();
    const QJsonObject apps = root.value(QStringLiteral("apps")).toObject();
    for (auto it = apps.begin(); it != apps.end() && m_apps.size() < kMaxApps; ++it) {
        if (it.value().isString()) m_apps.insert(it.key(), it.value().toString());
    }
    for (const QJsonValue &v : root.value(QStringLiteral("muted")).toArray()) {
        if (v.isString() && m_muted.size() < kMaxApps) m_muted.insert(v.toString());
    }
}

bool AppSettings::save() const
{
    QJsonArray muted;
    QStringList sorted(m_muted.begin(), m_muted.end());
    sorted.sort();
    for (const QString &p : sorted) muted.append(p);
    QJsonObject apps;
    for (auto it = m_apps.begin(); it != m_apps.end(); ++it) apps.insert(it.key(), it.value());
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("muted"), muted}, {QStringLiteral("apps"), apps}};
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) return false;
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    return f.write(bytes) == bytes.size() && f.commit();
}

bool AppSettings::noteApp(const QString &pkg, const QString &label)
{
    const auto it = m_apps.constFind(pkg);
    if (it != m_apps.constEnd() && it.value() == label) return false;
    if (it == m_apps.constEnd() && m_apps.size() >= kMaxApps) return false;
    m_apps.insert(pkg, label);
    save();
    return true;
}

bool AppSettings::setMuted(const QString &pkg, bool muted)
{
    if (muted == m_muted.contains(pkg)) return false;
    if (muted) {
        if (m_muted.size() >= kMaxApps) return false;
        m_muted.insert(pkg);
    } else {
        m_muted.remove(pkg);
    }
    save();
    return true;
}

QJsonArray AppSettings::apps() const
{
    QList<QJsonObject> list;
    QSet<QString> all = m_muted;
    for (auto it = m_apps.begin(); it != m_apps.end(); ++it) all.insert(it.key());
    for (const QString &pkg : all) {
        list.append(QJsonObject{{QStringLiteral("pkg"), pkg}, {QStringLiteral("label"), m_apps.value(pkg, pkg)},
                                {QStringLiteral("muted"), m_muted.contains(pkg)}});
    }
    std::sort(list.begin(), list.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const int c = a.value(QStringLiteral("label")).toString().compare(b.value(QStringLiteral("label")).toString(), Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a.value(QStringLiteral("pkg")).toString() < b.value(QStringLiteral("pkg")).toString();
    });
    QJsonArray out;
    for (const QJsonObject &o : list) out.append(o);
    return out;
}

} // namespace QindaQt::Link
