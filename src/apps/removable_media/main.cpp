// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_appearance.h"
#include "media_controller.h"
#include "media_notifications.h"
#include "udisks_backend.h"
#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <QCommandLineParser>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdio>
#include <memory>

using namespace QindaQt::Apps::RemovableMedia;

namespace {
constexpr auto serviceName = "org.qindaqt.RemovableMedia1";
constexpr auto servicePath = "/org/qindaqt/RemovableMedia1";

// AGENT-GUARD: this activation object alone is exported. Storage operations
// remain local controller calls, never public session-bus methods.
class Activation final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.RemovableMedia1")
public:
    explicit Activation(MediaController &controller) : m_controller(controller) {}
public Q_SLOTS:
    void Activate() { m_controller.show(); }
private:
    MediaController &m_controller;
};

// A probe must not enumerate or mutate the user's real storage. This fixture
// also makes installed-stage QML construction checks deterministic.
class ProbeBackend final : public MediaBackend {
public:
    ProbeBackend() {
        Volume volume;
        volume.token = QStringLiteral("probe-attachment");
        volume.device = QStringLiteral("/dev/probe1");
        volume.label = QStringLiteral("Example USB drive");
        volume.kind = QStringLiteral("USB storage");
        volume.size = 16ULL * 1024ULL * 1024ULL * 1024ULL;
        volume.mountable = volume.canMountReadOnly = volume.canFormat = volume.canPowerOff = true;
        volume.preferenceKey = QString(64, QLatin1Char('a'));
        m_volumes.append(volume);
    }
    QVector<Volume> volumes() const override { return m_volumes; }
    bool available() const override { return true; }
    QString diagnostic() const override { return {}; }
    QStringList formatTypes() const override { return {QStringLiteral("vfat"), QStringLiteral("ext4")}; }
    void refresh() override { Q_EMIT changed(); }
    void execute(const Request &request) override {
        Q_EMIT finished(request.token, false, QStringLiteral("UI probe cannot operate on storage."), {});
    }
private:
    QVector<Volume> m_volumes;
};

void addImportPaths(QQmlApplicationEngine &engine) {
    const QString current = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString build = QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH)).canonicalFilePath();
    if (!build.isEmpty() && current == build) {
        engine.addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    }
    engine.addImportPath(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH)));
}

int verifyUi(QQmlApplicationEngine &engine) {
    QObject *root = engine.rootObjects().constFirst();
    const QStringList names = {QStringLiteral("mediaList"), QStringLiteral("mountOpenButton"),
        QStringLiteral("mountReadOnlyButton"), QStringLiteral("removeMediaButton"),
        QStringLiteral("insertionPreference"), QStringLiteral("formatMediaDialog"),
        QStringLiteral("confirmFormatButton"), QStringLiteral("mediaPassphrase")};
    for (const QString &name : names) {
        if (root->findChild<QObject *>(name) == nullptr) {
            std::fprintf(stderr, "qindaqt-removable-media: missing UI control %s\n", qPrintable(name));
            return 5;
        }
    }
    std::puts("removable-media-ui-ready fixture-only");
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    QGuiApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("QindaQt"));
    application.setOrganizationDomain(QStringLiteral("qindaqt.org"));
    application.setApplicationName(QStringLiteral("qindaqt-removable-media"));
    application.setApplicationDisplayName(QStringLiteral("Removable Media"));
    application.setDesktopFileName(QStringLiteral("org.qindaqt.RemovableMedia"));
    application.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Graphical removable-storage management"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("watch"), QStringLiteral("Watch insertions with the window initially hidden")});
    parser.addOption({QStringLiteral("check-ui-contract"), QStringLiteral("Verify the UI using synthetic media and exit")});
    parser.addOption({QStringLiteral("theme-directory"), QStringLiteral("Additional local theme directory"), QStringLiteral("path")});
    parser.process(application);
    const bool probe = parser.isSet(QStringLiteral("check-ui-contract"));
    const bool watch = parser.isSet(QStringLiteral("watch"));
    auto session = probe ? QDBusConnection(QStringLiteral("removable-media-ui-probe"))
                         : QDBusConnection::sessionBus();

    if (!probe && !session.registerService(QString::fromLatin1(serviceName))) {
        if (!session.isConnected()) {
            std::fprintf(stderr, "qindaqt-removable-media: session bus unavailable\n");
            return 2;
        }
        if (!watch) {
            QDBusInterface existing(QString::fromLatin1(serviceName), QString::fromLatin1(servicePath),
                                    QString::fromLatin1(serviceName), session);
            const QDBusMessage reply = existing.call(QStringLiteral("Activate"));
            if (reply.type() == QDBusMessage::ErrorMessage) {
                std::fprintf(stderr, "qindaqt-removable-media: could not activate the media window\n");
                return 2;
            }
        }
        return 0;
    }

    QTemporaryDir probeDirectory;
    if (probe && !probeDirectory.isValid()) return 2;
    MediaPreferences preferences(probe ? probeDirectory.filePath(QStringLiteral("preferences.json")) : QString());
    std::unique_ptr<MediaBackend> backend;
    if (probe) backend = std::make_unique<ProbeBackend>();
    else backend = std::make_unique<UDisksBackend>(QDBusConnection::systemBus());
    MediaController controller(*backend, preferences, !probe);
    std::unique_ptr<MediaNotifications> notifications;
    if (!probe) notifications = std::make_unique<MediaNotifications>(controller, session);
    Activation activation(controller);
    if (!probe && !session.registerObject(QString::fromLatin1(servicePath), &activation,
                                          QDBusConnection::ExportAllSlots)) return 2;
    if (probe) controller.select(QStringLiteral("probe-attachment"));

    QindaQt::Services::SettingsClient::QtSettingsTransport settingsTransport(session);
    QindaQt::Services::SettingsClient::SettingsClient settingsClient(settingsTransport,
        {QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme"), QStringLiteral("appearance.iconTheme")});
    if (!probe) {
        QString settingsError;
        if (!settingsClient.start(&settingsError))
            qWarning("qindaqt-removable-media: appearance preferences are unavailable");
    }
    QindaQt::AppAppearance::ApplicationAppearanceController appearance(settingsClient,
        QindaQt::AppAppearance::standardThemeDirectories(parser.value(QStringLiteral("theme-directory"))),
        QStringLiteral("qinda-dark"));
    QQmlApplicationEngine engine;
    addImportPaths(engine);
    QString appearanceError;
    auto *facade = ensureMediaTokenFacade(engine, &appearanceError);
    if (facade == nullptr || !applyMediaAppearance(appearance, *facade, application, &appearanceError)) {
        std::fprintf(stderr, "qindaqt-removable-media: %s\n", qPrintable(appearanceError));
        return 3;
    }
    QObject::connect(&appearance, &QindaQt::AppAppearance::ApplicationAppearanceController::appearanceChanged,
        &engine, [&appearance, facade, &application] {
            QString error;
            if (!applyMediaAppearance(appearance, *facade, application, &error))
                qWarning("qindaqt-removable-media: appearance update rejected");
        });
    engine.rootContext()->setContextProperty(QStringLiteral("mediaController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaWatchMode"), watch);
    engine.rootContext()->setContextProperty(QStringLiteral("mediaLaunchError"), QString());
    QObject::connect(&controller, &MediaController::openPathRequested, &engine, [&engine, &controller](const QString &path) {
        const QString program = QStandardPaths::findExecutable(QStringLiteral("qindaqt-file-manager"));
        // AGENT-GUARD: the path is one literal argv entry after the option
        // delimiter. Device labels and mount paths never become shell syntax.
        const bool started = !program.isEmpty() && QProcess::startDetached(program, {QStringLiteral("--"), path});
        engine.rootContext()->setContextProperty(QStringLiteral("mediaLaunchError"), started ? QString()
            : QStringLiteral("File Manager could not be started. The drive remains mounted; try Open again."));
        if (!started) controller.show();
    });
    engine.loadFromModule(QStringLiteral("QindaQt.RemovableMediaApp"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) return 4;
    if (probe) return verifyUi(engine);
    return application.exec();
}

#include "main.moc"
