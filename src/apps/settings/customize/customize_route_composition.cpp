// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_route_composition.h"

#include "customize_catalog.h"
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

} // namespace

class CustomizeRouteComposition::Private final {
public:
    Private()
        : transport(QDBusConnection::sessionBus())
        , client(transport, {QString(LayoutProfileSettingsKey)})
    {
        const QString profileSource = QStringLiteral(
            QINDAQT_CUSTOMIZE_SOURCE_PROFILE_DIRECTORY);
        const QString manifestSource = QStringLiteral(
            QINDAQT_CUSTOMIZE_SOURCE_MANIFEST_DIRECTORY);
        const auto catalogs = loadCustomizeCatalogs(
            catalogDirectories(QStringLiteral("qindaqt/profiles"),
                               profileSource),
            catalogDirectories(QStringLiteral("qindaqt/applets"),
                               manifestSource));
        const QString userDirectory = QDir(
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
                                          .filePath(QStringLiteral("qindaqt/profiles"));
        const QVector<Applets::AppletManifest> manifests = catalogs.manifests;
        const EditorHostFactory factory =
            [manifests, userDirectory](const Profiles::LayoutProfile &profile) {
                return std::make_unique<RepositoryCustomizeEditorHost>(
                    profile,
                    QVector<ShellLayout::LogicalOutput>{
                        {QStringLiteral("primary"), QRect(0, 0, 1920, 1080), 1.0}},
                    manifests, userDirectory);
            };
        model = std::make_unique<CustomizeSettingsModel>(
            client, catalogs.profiles, catalogs.manifests, factory,
            catalogs.error);
        QString error;
        if (!client.start(&error) && catalogs.error.isEmpty()) {
            qWarning("qindaqt-settings: Customize Settings1 unavailable: %s",
                     qPrintable(error));
        }
    }

    Services::SettingsClient::QtSettingsTransport transport;
    Services::SettingsClient::SettingsClient client;
    std::unique_ptr<CustomizeSettingsModel> model;
};

CustomizeRouteComposition::CustomizeRouteComposition(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

CustomizeRouteComposition::~CustomizeRouteComposition() = default;

QObject *CustomizeRouteComposition::model() const
{
    return d->model.get();
}

} // namespace QindaQt::Apps::SettingsCustomize
