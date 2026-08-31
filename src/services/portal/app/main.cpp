// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/appearance_theme_catalog.h"
#include "qindaqt/services/portal/resident_portal_service.h"
#include "qindaqt/services/portal/settings1_appearance_source.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <cstdio>

using namespace QindaQt::Services::Portal;
using QindaQt::Services::SettingsClient::QtSettingsTransport;
using QindaQt::Services::SettingsClient::SettingsClient;

namespace {

QStringList themeDirectories()
{
    const QString override = qEnvironmentVariable("QINDAQT_PORTAL_THEME_DIRS");
    if (!override.isEmpty()) {
        return override.split(QDir::listSeparator(), Qt::KeepEmptyParts);
    }
    QStringList paths;
    for (const QString &dataRoot :
         QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        paths.append(QDir(dataRoot).filePath(QStringLiteral("qindaqt/themes")));
    }
    const QString besideExecutable =
        QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../share/qindaqt/themes"));
    if (!paths.contains(besideExecutable)) {
        paths.append(besideExecutable);
    }
    return paths;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QStringLiteral("xdg-desktop-portal-qindaqt"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    // AGENT-GUARD: One backend process and its Settings1 client belong to the
    // exact daemon that constructed them. Reconnecting after Local.Disconnected
    // would retain a stale comparison baseline under a new portal owner.
    if (!sessionBus.connect(QString{},
                            QStringLiteral("/org/freedesktop/DBus/Local"),
                            QStringLiteral("org.freedesktop.DBus.Local"),
                            QStringLiteral("Disconnected"), &application,
                            SLOT(quit()))) {
        std::fprintf(stderr,
                     "xdg-desktop-portal-qindaqt: cannot observe bus loss\n");
        return 3;
    }

    QString error;
    auto themes = loadPortalAppearanceThemes(themeDirectories(), &error);
    if (!themes.has_value()) {
        std::fprintf(stderr, "xdg-desktop-portal-qindaqt: %s\n",
                     qPrintable(error));
        return 2;
    }
    AppearancePolicyProjector projector(std::move(*themes));
    if (!projector.isValid()) {
        std::fprintf(stderr, "xdg-desktop-portal-qindaqt: %s\n",
                     qPrintable(projector.catalogError()));
        return 2;
    }

    QtSettingsTransport transport(sessionBus);
    SettingsClient client(transport,
                          AppearancePolicyProjector::scopedSettingsKeys());
    Settings1AppearanceSource source(client, projector);
    ResidentPortalService service(source, sessionBus);
    const PortalServiceStartStatus status = service.start(&error);
    if (status != PortalServiceStartStatus::Started) {
        std::fprintf(stderr, "xdg-desktop-portal-qindaqt: %s: %s\n",
                     qPrintable(portalServiceStartStatusName(status)),
                     qPrintable(error));
        return 3;
    }
    return application.exec();
}
