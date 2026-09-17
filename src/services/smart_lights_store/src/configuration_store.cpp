// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/smart_lights_store/configuration_store.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

#include <utility>

namespace QindaQt::SmartLights
{
namespace
{

// A configuration document is small: a few dozen devices and presets. A file
// larger than this is not one of ours and is refused without being parsed.
constexpr qint64 maximumDocumentBytes = 512 * 1024;

} // namespace

ConfigurationStore::ConfigurationStore(QString path)
    : m_path(std::move(path))
{
}

QString ConfigurationStore::defaultPath()
{
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(root).filePath(QStringLiteral("qindaqt/smart-lights.json"));
}

StoredConfiguration ConfigurationStore::load(QString *error)
{
    m_unreadable = false;
    QFile file(m_path);
    if (!file.exists()) {
        return {};
    }
    if (file.size() > maximumDocumentBytes) {
        m_unreadable = true;
        if (error != nullptr) {
            *error = QStringLiteral("The smart-light configuration file is too large.");
        }
        return {};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_unreadable = true;
        if (error != nullptr) {
            *error = file.errorString();
        }
        return {};
    }
    const QByteArray document = file.readAll();
    file.close();

    const auto decoded = decodeConfiguration(document);
    if (!decoded.has_value()) {
        m_unreadable = true;
        if (error != nullptr) {
            *error = QStringLiteral(
                "The smart-light configuration file could not be read.");
        }
        return {};
    }
    return *decoded;
}

bool ConfigurationStore::save(const StoredConfiguration &configuration, QString *error)
{
    const QFileInfo info(m_path);
    const QDir directory = info.absoluteDir();
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error != nullptr) {
            *error = QStringLiteral("The configuration directory could not be created.");
        }
        return false;
    }

    if (m_unreadable && QFile::exists(m_path)) {
        // Preserve whatever we could not decode before writing our own.
        const QString quarantine = m_path + QStringLiteral(".invalid");
        QFile::remove(quarantine);
        if (!QFile::rename(m_path, quarantine)) {
            if (error != nullptr) {
                *error = QStringLiteral(
                    "An unreadable smart-light configuration file is in the way.");
            }
            return false;
        }
        m_unreadable = false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }
    const QByteArray document = encodeConfiguration(configuration);
    if (file.write(document) != document.size() || !file.commit()) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::SmartLights
