// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_route_composition.h"

#include "qindaqt/apps/settings_customize/customize_settings_model.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace QindaQt::Apps::SettingsCustomize {
namespace {

QStringList installedDirectories(const QString &suffix)
{
    QStringList directories;
    QStringList roots = QStandardPaths::standardLocations(
        QStandardPaths::GenericDataLocation);
    std::reverse(roots.begin(), roots.end());
    for (const QString &root : roots) {
        const QString candidate = QDir(root).filePath(suffix);
        if (QDir(candidate).exists()) {
            directories.append(QDir::cleanPath(candidate));
        }
    }
    directories.removeDuplicates();
    return directories;
}

bool isBuildExecutable()
{
    const QString running = QFileInfo(QCoreApplication::applicationFilePath())
                                .canonicalFilePath();
    const QString built = QFileInfo(QStringLiteral(
        QINDAQT_CUSTOMIZE_BUILD_EXECUTABLE_PATH)).canonicalFilePath();
    return !running.isEmpty() && running == built;
}

QStringList catalogDirectories(const QString &suffix,
                               const QString &sourceDirectory)
{
    QStringList directories = installedDirectories(suffix);
    // AGENT-CONTRACT: The SettingsAppearanceRuntime component installs the
    // audited catalogs beside the executable prefix, independent of XDG paths.
    const QString bundled = QDir(QCoreApplication::applicationDirPath())
                                .filePath(QStringLiteral(
                                    QINDAQT_CUSTOMIZE_INSTALL_DATA_RELATIVE_PATH)
                                              + QLatin1Char('/')
                                              + QFileInfo(suffix).fileName());
    if (QDir(bundled).exists()) {
        directories.prepend(QDir::cleanPath(bundled));
    }
    if (isBuildExecutable() && QDir(sourceDirectory).exists()) {
        directories.prepend(sourceDirectory);
    }
    return directories;
}

// The installed catalogs and the writable user store, kept apart: the page
// tells built-ins, edited built-ins and the user's own presets apart by which
// side a profile came from (ADR-0267). Precedence matches the shell's.
PresetLocations presetLocations()
{
    const QString userDirectory = QDir::cleanPath(
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
            .filePath(QStringLiteral("qindaqt/profiles")));
    QStringList stock = catalogDirectories(
        QStringLiteral("qindaqt/profiles"),
        QStringLiteral(QINDAQT_CUSTOMIZE_SOURCE_PROFILE_DIRECTORY));
    // AGENT-GUARD: the user store appears among the standard data locations.
    // Listed as installed it would make every own preset look built-in and
    // every edited built-in look unmodified.
    const QString userCanonical = QFileInfo(userDirectory).canonicalFilePath();
    stock.removeIf([&](const QString &directory) {
        const QString canonical = QFileInfo(directory).canonicalFilePath();
        return QDir::cleanPath(directory) == userDirectory
            || (!userCanonical.isEmpty() && canonical == userCanonical);
    });
    return {stock, userDirectory};
}

} // namespace

class CustomizeRouteComposition::Private final {
public:
    Private()
        : transport(QDBusConnection::sessionBus())
        , client(transport, {QString(LayoutProfileSettingsKey),
                             QString(PanelHideDelaySettingsKey)})
        , model(client, presetLocations())
    {
        QString error;
        if (!client.start(&error)) {
            qWarning("qindaqt-settings: Customize Settings1 unavailable: %s",
                     qPrintable(error));
        }
    }

    Services::SettingsClient::QtSettingsTransport transport;
    Services::SettingsClient::SettingsClient client;
    CustomizeSettingsModel model;
};

CustomizeRouteComposition::CustomizeRouteComposition(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

CustomizeRouteComposition::~CustomizeRouteComposition() = default;

QObject *CustomizeRouteComposition::model() const
{
    return &d->model;
}

} // namespace QindaQt::Apps::SettingsCustomize
