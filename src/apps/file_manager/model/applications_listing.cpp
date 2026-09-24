// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/applications_listing.h"

#include "model/applications_location.h"

#include "qindaqt/application_catalog/launch_support.h"

#include <QStringList>

namespace QindaQt::Apps::FileManager::ApplicationsListing {
namespace {

using QindaQt::ApplicationCatalog::CategoryNode;
using QindaQt::ApplicationCatalog::LaunchPreparation;
using QindaQt::ApplicationCatalog::LaunchSupport;
using QindaQt::ApplicationCatalog::ScannedApplication;

// Every icon-less entry still shows an application glyph, never a document.
constexpr auto fallbackIconName = "application-x-executable";

[[nodiscard]] LaunchPreparation planFor(const ScannedApplication &application)
{
    // Plans from the exact validated bytes the scan retained (ADR-0164);
    // never re-reads the file.
    return QindaQt::ApplicationCatalog::planApplicationLaunch(
        application.documentText, QString(), application.entry.name,
        application.desktopFilePath);
}

void collect(const CategoryNode &node, const QString &label,
             QHash<QString, QString> &labels)
{
    for (const auto &entry : node.entries) {
        labels.insert(entry.id, label);
    }
    for (const auto &child : node.children) {
        collect(child, label, labels);
    }
}

// Display-only rendering of a planned argv; nothing ever executes this text.
[[nodiscard]] QString commandText(const LaunchPreparation &preparation)
{
    if (preparation.program.isEmpty()) {
        return {};
    }
    QStringList parts{preparation.program};
    for (const auto &argument : preparation.arguments) {
        const bool plain = !argument.isEmpty()
            && !argument.contains(QLatin1Char(' '))
            && !argument.contains(QLatin1Char('"'));
        parts.append(plain ? argument
                           : QLatin1Char('"')
                                 + QString(argument).replace(
                                     QLatin1Char('"'), QStringLiteral("\\\""))
                                 + QLatin1Char('"'));
    }
    return parts.join(QLatin1Char(' '));
}

} // namespace

QHash<QString, QString> categoryLabels(const CategoryNode &tree)
{
    QHash<QString, QString> labels;
    // The root's children are the fixed presentation groups; every nested
    // additional-category folder belongs to its group's label.
    for (const auto &group : tree.children) {
        collect(group, group.label, labels);
    }
    return labels;
}

QString standaloneLimitation(const ScannedApplication &application,
                             bool chooserMode)
{
    if (chooserMode) {
        return {};
    }
    const auto preparation = planFor(application);
    switch (preparation.support) {
    case LaunchSupport::ProcessSpawn:
        return {};
    case LaunchSupport::TerminalRequired:
        return QStringLiteral(
            "Runs in a terminal: open it from a docked window or a workspace picker");
    case LaunchSupport::DbusActivatable:
        return QStringLiteral(
            "Starts through D-Bus activation: open it from a docked window or a "
            "workspace picker");
    case LaunchSupport::Unsupported:
        break;
    }
    return preparation.message.isEmpty()
        ? QStringLiteral("This application cannot be started directly")
        : preparation.message;
}

DirectoryEntry row(const ScannedApplication &application,
                   const QString &category, bool chooserMode)
{
    DirectoryEntry entry;
    entry.name = application.entry.name;
    entry.absolutePath = ApplicationsLocation::entryPath(application.entry.id);
    entry.applicationId = application.entry.id;
    entry.iconName = application.entry.iconName.isEmpty()
        ? QString::fromLatin1(fallbackIconName)
        : application.entry.iconName;
    entry.kindText = category;
    entry.note = standaloneLimitation(application, chooserMode);
    return entry;
}

QVariantMap describe(const ScannedApplication &application,
                     const QString &category, bool chooserMode)
{
    const auto &entry = application.entry;
    return {{QStringLiteral("id"), entry.id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("genericName"), entry.genericName},
            {QStringLiteral("comment"), entry.comment},
            {QStringLiteral("category"), category},
            {QStringLiteral("categories"),
             entry.categories.join(QStringLiteral(", "))},
            {QStringLiteral("command"), commandText(planFor(application))},
            {QStringLiteral("iconName"),
             entry.iconName.isEmpty() ? QString::fromLatin1(fallbackIconName)
                                      : entry.iconName},
            {QStringLiteral("desktopFilePath"), application.desktopFilePath},
            {QStringLiteral("note"),
             standaloneLimitation(application, chooserMode)}};
}

} // namespace QindaQt::Apps::FileManager::ApplicationsListing
