// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_preferences.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

namespace QindaQt::LinkApp {

AppPreferences::AppPreferences(QString path, QObject *parent) : QObject(parent), m_path(std::move(path))
{
    load();
}

QString AppPreferences::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/qindalink/app.json");
}

void AppPreferences::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue &v : root.value(QStringLiteral("favourites")).toArray()) {
        if (v.isString() && !m_favourites.contains(v.toString())) m_favourites << v.toString();
    }
    m_rcsDismissed = root.value(QStringLiteral("rcsNoticeDismissed")).toBool();
}

bool AppPreferences::save()
{
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) return false;
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("favourites"), QJsonArray::fromStringList(m_favourites)},
                                         {QStringLiteral("rcsNoticeDismissed"), m_rcsDismissed}})
                   .toJson(QJsonDocument::Indented));
    return file.commit();
}

bool AppPreferences::setFavourite(const QString &pkg, bool favourite)
{
    if (favourite == m_favourites.contains(pkg)) return true;
    if (favourite) m_favourites << pkg; else m_favourites.removeAll(pkg);
    return save();
}

bool AppPreferences::setRcsNoticeDismissed(bool dismissed)
{
    if (dismissed == m_rcsDismissed) return true;
    m_rcsDismissed = dismissed;
    Q_EMIT changed();
    return save();
}

} // namespace QindaQt::LinkApp
