// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_preferences.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace QindaQt::Apps::RemovableMedia {
namespace {
bool validMode(const QString &mode)
{
    return mode == QStringLiteral("ask") || mode == QStringLiteral("mount")
        || mode == QStringLiteral("read-only") || mode == QStringLiteral("ignore");
}
}
MediaPreferences::MediaPreferences(QString path) : m_path(std::move(path))
{
    if (m_path.isEmpty()) m_path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/media-choices.json");
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 256 * 1024) return;
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QStringLiteral("version")).toInt() != 1) return;
    const auto choices = root.value(QStringLiteral("choices")).toObject();
    for (auto it = choices.begin(); it != choices.end(); ++it) {
        const QString mode = it.value().toString();
        if (it.key().size() == 64 && validMode(mode)) m_modes.insert(it.key(), mode);
    }
}
QString MediaPreferences::mode(const QString &key) const
{
    return m_modes.value(key, QStringLiteral("ask"));
}
bool MediaPreferences::save(const QString &key, const QString &mode, QString *error)
{
    if (key.size() != 64 || !validMode(mode) || (m_modes.size() >= 1024 && !m_modes.contains(key))) {
        if (error) *error = QStringLiteral("This media has no stable identity or the choice is invalid.");
        return false;
    }
    auto next = m_modes;
    if (mode == QStringLiteral("ask")) next.remove(key);
    else next.insert(key, mode);
    QJsonObject choices;
    for (auto it = next.cbegin(); it != next.cend(); ++it) choices.insert(it.key(), it.value());
    QSaveFile file(m_path);
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()) || !file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Could not save this media's choice: ") + file.errorString();
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                                    {QStringLiteral("choices"), choices}}).toJson();
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = QStringLiteral("Could not save this media's choice: ") + file.errorString();
        return false;
    }
    m_modes = std::move(next);
    return true;
}
} // namespace QindaQt::Apps::RemovableMedia
