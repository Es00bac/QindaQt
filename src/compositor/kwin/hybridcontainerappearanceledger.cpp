// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcontainerappearanceledger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace QindaQt::Compositor::KWinIntegration {
namespace {
constexpr qint64 MaxLedgerBytes = 1 * 1024 * 1024;
constexpr int SchemaVersion = 1;
} // namespace

ContainerAppearanceLedger::ContainerAppearanceLedger()
    : m_path(defaultStoragePath())
{
}

ContainerAppearanceLedger::ContainerAppearanceLedger(QString storagePath)
    : m_path(std::move(storagePath))
{
}

QString ContainerAppearanceLedger::defaultStoragePath()
{
    // AGENT-CONTRACT: This explicit directory is shared with saved
    // workspaces (KWinHybridSession::initializeSavedWorkspaces).
    // AppDataLocation would silently key it to KWin's executable/application
    // name instead of QindaQt's durable product identity.
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("qindaqt/container-appearance.json"));
}

QHash<QString, ContainerAppearance> ContainerAppearanceLedger::load() const
{
    QHash<QString, ContainerAppearance> result;
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxLedgerBytes) {
        return result;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()
        || document.object().value(QStringLiteral("schemaVersion")).toInt(-1)
            != SchemaVersion) {
        return result;
    }
    const auto containers =
        document.object().value(QStringLiteral("containers")).toObject();
    for (auto it = containers.constBegin(); it != containers.constEnd(); ++it) {
        const auto entry = it.value().toObject();
        ContainerAppearance appearance;
        appearance.name = entry.value(QStringLiteral("name")).toString();
        appearance.colorHex = entry.value(QStringLiteral("colorHex")).toString();
        if (!appearance.name.isEmpty() || !appearance.colorHex.isEmpty()) {
            result.insert(it.key(), appearance);
        }
    }
    return result;
}

bool ContainerAppearanceLedger::save(
    const QHash<QString, ContainerAppearance> &entries) const
{
    QJsonObject containers;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        if (it.value().name.isEmpty() && it.value().colorHex.isEmpty()) {
            continue;
        }
        containers.insert(it.key(),
                          QJsonObject{
                              {QStringLiteral("name"), it.value().name},
                              {QStringLiteral("colorHex"), it.value().colorHex},
                          });
    }
    const QJsonObject document{
        {QStringLiteral("schemaVersion"), SchemaVersion},
        {QStringLiteral("containers"), containers},
    };
    const auto bytes = QJsonDocument(document).toJson();
    if (bytes.size() > MaxLedgerBytes) {
        return false;
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        return false;
    }
    QSaveFile file(m_path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()
        && file.commit();
}

} // namespace QindaQt::Compositor::KWinIntegration
