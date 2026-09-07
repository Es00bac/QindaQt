// SPDX-License-Identifier: GPL-3.0-or-later
#include "shortcutnotecontroller.h"
#include "shortcutnoteshortcut.h"

#include "globalshortcutregistrar.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <QQmlEngine>

#include <functional>
#include <memory>

using namespace QindaQt;
using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

namespace {

class FakeRegistrar final : public GlobalShortcutRegistrar {
public:
    GlobalShortcutRegistration registerShortcut(
        QAction &action, const QKeySequence &defaultShortcut, QObject &lifetime,
        std::function<void(bool)> activeBindingChanged) override
    {
        observedAction = &action;
        observedDefault = defaultShortcut;
        observedLifetime = &lifetime;
        changed = std::move(activeBindingChanged);
        return result;
    }

    GlobalShortcutRegistration result{true, false};
    QAction *observedAction = nullptr;
    QKeySequence observedDefault;
    QObject *observedLifetime = nullptr;
    std::function<void(bool)> changed;
};

struct BusHarness final {
    QProcess daemon;
    QString address;

    BusHarness()
    {
        daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                     {QStringLiteral("--session"), QStringLiteral("--nofork"),
                      QStringLiteral("--print-address=1")});
        QVERIFY(daemon.waitForStarted());
        QVERIFY(daemon.waitForReadyRead());
        address = QString::fromUtf8(daemon.readLine()).trimmed();
        QVERIFY(serviceBus().isConnected());
        QVERIFY(clientBus().isConnected());
    }

    ~BusHarness()
    {
        QDBusConnection::disconnectFromBus(serviceConnectionName());
        QDBusConnection::disconnectFromBus(clientConnectionName());
        daemon.kill();
        daemon.waitForFinished();
    }

    [[nodiscard]] static QString serviceConnectionName()
    {
        return QStringLiteral("shortcut-note-service-%1").arg(
            QCoreApplication::applicationPid());
    }

    [[nodiscard]] static QString clientConnectionName()
    {
        return QStringLiteral("shortcut-note-client-%1").arg(
            QCoreApplication::applicationPid());
    }

    [[nodiscard]] QDBusConnection serviceBus() const
    {
        return QDBusConnection::connectToBus(address, serviceConnectionName());
    }

    [[nodiscard]] QDBusConnection clientBus() const
    {
        return QDBusConnection::connectToBus(address, clientConnectionName());
    }
};

// One purpose-scoped client over the session-private bus. Declaration order
// keeps the transport alive longer than its borrower, mirroring the shell's
// quieting-bridge convention.
struct ClientHandle final {
    std::unique_ptr<QtSettingsTransport> transport;
    std::unique_ptr<SettingsClient> client;
};

struct ServiceHarness final {
    std::optional<SettingsSchema> active;
    std::optional<SettingsSchema> legacy;
    std::unique_ptr<ResidentSettingsService> service;

    ServiceHarness(const QDBusConnection &serviceConnection,
                   const QString &userLayer)
    {
        QString error;
        active = SettingsSchema::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"),
            nullptr, &error);
        legacy = SettingsSchema::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"),
            nullptr, &error, 1);
        QVERIFY2(active.has_value() && legacy.has_value(),
                 qPrintable(error));
        service = std::make_unique<ResidentSettingsService>(
            serviceConnection, *active, *legacy,
            QStringLiteral(
                QINDAQT_SOURCE_DIR
                "/data/settings/profile-defaults/qindaqt.json"),
            userLayer);
        QVERIFY(service->start().ok());
    }

    ~ServiceHarness()
    {
        if (service) {
            service->stop();
        }
    }

    [[nodiscard]] ClientHandle makeClient(const QDBusConnection &clientBus) const
    {
        ClientHandle handle;
        handle.transport =
            std::make_unique<QtSettingsTransport>(clientBus);
        handle.client = std::make_unique<SettingsClient>(
            *handle.transport,
            QStringList{ShortcutNoteController::settingsKey()},
            ClientTiming{.requestTimeoutMilliseconds = 500,
                         .debounceMilliseconds = 0,
                         .retryMilliseconds = {10, 20}});
        return handle;
    }
};

} // namespace

class ShortcutNoteTests final : public QObject {
    Q_OBJECT

private slots:
    void shortcutExposesStableIdentityAndTracksOverrides();
    void noteIsVisibleWithoutAConfirmedSnapshot();
    void absentSettingsKeepsDismissalSessionOnly();
    void dismissalPersistsAcrossSessionsAndHotkeyReopens();
    void publishesThemeInsetsAndPrimaryScreenForCards();

private:
    QQmlEngine engine;
};

void ShortcutNoteTests::shortcutExposesStableIdentityAndTracksOverrides()
{
    FakeRegistrar registrar;
    SettingsClient nowhere(
        *new QtSettingsTransport(QDBusConnection::sessionBus()),
        QStringList{ShortcutNoteController::settingsKey()});
    ShortcutNoteController controller(*qGuiApp, engine, nowhere, registrar);

    QCOMPARE(registrar.observedAction, controller.shortcut()->action());
    QCOMPARE(registrar.observedLifetime, controller.shortcut());
    QCOMPARE(controller.shortcut()->action()->objectName(),
             QStringLiteral("qindaqt_toggle_shortcut_note"));
    QCOMPARE(ShortcutNoteShortcut::stableActionId(),
             controller.shortcut()->action()->objectName());
    // The reopen binding is the documented default; the public registrar seam
    // cannot report remapped sequences, so the card must label defaults.
    QCOMPARE(ShortcutNoteShortcut::defaultShortcut(),
             QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_F1));
    QCOMPARE(registrar.observedDefault, ShortcutNoteShortcut::defaultShortcut());
    QVERIFY(controller.shortcut()->registrationRequestAccepted());
    QVERIFY(!controller.shortcut()->activeBindingPresent());

    // Shortcut override behavior: a user remap (or disable) of the binding
    // arrives only through the activeBinding callback and never changes the
    // registered default or the toggle wiring.
    QSignalSpy bindingChanges(
        controller.shortcut(),
        &ShortcutNoteShortcut::activeBindingPresentChanged);
    registrar.changed(true);
    QVERIFY(controller.shortcut()->activeBindingPresent());
    QCOMPARE(bindingChanges.size(), 1);
    registrar.changed(true);
    QCOMPARE(bindingChanges.size(), 1);
    registrar.changed(false);
    QVERIFY(!controller.shortcut()->activeBindingPresent());
    QCOMPARE(bindingChanges.size(), 2);
    QCOMPARE(registrar.observedDefault,
             QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_F1));

    // The registered action drives the note toggle: visible -> dismissed.
    QVERIFY(controller.noteVisible());
    registrar.observedAction->trigger();
    QVERIFY(!controller.noteVisible());
}

void ShortcutNoteTests::noteIsVisibleWithoutAConfirmedSnapshot()
{
    FakeRegistrar registrar;
    SettingsClient unreachable(
        *new QtSettingsTransport(QDBusConnection::sessionBus()),
        QStringList{ShortcutNoteController::settingsKey()});
    ShortcutNoteController controller(*qGuiApp, engine, unreachable, registrar);

    // Fail visible: no confirmed snapshot means the shortcut cheatsheet must
    // still be on the desktop.
    QVERIFY(controller.noteVisible());
}

void ShortcutNoteTests::absentSettingsKeepsDismissalSessionOnly()
{
    FakeRegistrar registrar;
    SettingsClient unreachable(
        *new QtSettingsTransport(QDBusConnection::sessionBus()),
        QStringList{ShortcutNoteController::settingsKey()});
    ShortcutNoteController controller(*qGuiApp, engine, unreachable, registrar);
    QVERIFY(controller.noteVisible());

    QSignalSpy visibilityChanges(
        &controller, &ShortcutNoteController::noteVisibleChanged);
    controller.dismiss();
    QVERIFY(!controller.noteVisible());
    QCOMPARE(visibilityChanges.size(), 1);

    // The global shortcut still reopens the note for this session even though
    // nothing can be persisted.
    registrar.observedAction->trigger();
    QVERIFY(controller.noteVisible());
    QCOMPARE(visibilityChanges.size(), 2);
}

void ShortcutNoteTests::dismissalPersistsAcrossSessionsAndHotkeyReopens()
{
    BusHarness bus;
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    const QString userLayerPath =
        storage.filePath(QStringLiteral("user.json"));
    {
        ServiceHarness service(bus.serviceBus(), userLayerPath);
        auto handle = service.makeClient(bus.clientBus());
        FakeRegistrar registrar;
        QString error;
        QVERIFY2(handle.client->start(&error), qPrintable(error));
        ShortcutNoteController controller(*qGuiApp, engine, *handle.client,
                                           registrar);
        // The real schema-v2 file backs the service: reaching a snapshot at
        // all proves the new shell.shortcutNoteDismissed key is valid, and
        // the schema default keeps a fresh session visible.
        QTRY_VERIFY_WITH_TIMEOUT(handle.client->snapshot().has_value(), 2'000);
        QCOMPARE(handle.client->snapshot()->values
                     .value(ShortcutNoteController::settingsKey())
                     .metaType()
                     .id(),
                 QMetaType::Bool);
        QTRY_VERIFY_WITH_TIMEOUT(controller.noteVisible(), 2'000);

        controller.dismiss();
        QVERIFY(!controller.noteVisible());
        QTRY_VERIFY_WITH_TIMEOUT(!handle.client->writeInFlight(), 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            handle.client->snapshot()
                    ->values.value(ShortcutNoteController::settingsKey())
                    .toBool(),
            2'000);
    }

    // Next session over the same persisted user layer: still dismissed.
    {
        ServiceHarness service(bus.serviceBus(), userLayerPath);
        auto handle = service.makeClient(bus.clientBus());
        FakeRegistrar registrar;
        QString error;
        QVERIFY2(handle.client->start(&error), qPrintable(error));
        ShortcutNoteController controller(*qGuiApp, engine, *handle.client,
                                           registrar);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.noteVisible(), 2'000);

        // Meta+Shift+F1 (registered through the seam) reopens and persists the
        // reopened state for the following session.
        registrar.observedAction->trigger();
        QVERIFY(controller.noteVisible());
        QTRY_VERIFY_WITH_TIMEOUT(!handle.client->writeInFlight(), 2'000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !handle.client->snapshot()
                     ->values.value(ShortcutNoteController::settingsKey())
                     .toBool(),
            2'000);
    }
}

void ShortcutNoteTests::publishesThemeInsetsAndPrimaryScreenForCards()
{
    FakeRegistrar registrar;
    SettingsClient nowhere(
        *new QtSettingsTransport(QDBusConnection::sessionBus()),
        QStringList{ShortcutNoteController::settingsKey()});
    ShortcutNoteController controller(*qGuiApp, engine, nowhere, registrar);

    const QVariantMap nightfall{
        {QStringLiteral("id"), QStringLiteral("qinda-dark")},
        {QStringLiteral("fontFamily"), QStringLiteral("Inter")},
        {QStringLiteral("monoFontFamily"), QStringLiteral("JetBrains Mono")},
        {QStringLiteral("colors"),
         QVariantMap{{QStringLiteral("surface"), QStringLiteral("#192939")},
                     {QStringLiteral("accent"), QStringLiteral("#D98A32")}}},
    };
    QSignalSpy themeChanges(&controller,
                            &ShortcutNoteController::themeChanged);
    controller.setTheme(nightfall);
    QCOMPARE(controller.theme(), nightfall);
    QCOMPARE(themeChanges.size(), 1);
    controller.setTheme(nightfall);
    QCOMPARE(themeChanges.size(), 1);

    QSignalSpy insetChanges(&controller,
                            &ShortcutNoteController::insetsChanged);
    controller.setPanelInsets(30, 72);
    QCOMPARE(controller.topInset(), 30);
    QCOMPARE(controller.rightInset(), 72);
    QCOMPARE(insetChanges.size(), 1);
    controller.setPanelInsets(-5, 72);
    QCOMPARE(controller.topInset(), 0);
    QCOMPARE(insetChanges.size(), 2);

    // The card uses the primary-output name to select its single surface.
    if (const QScreen *primary = qGuiApp->primaryScreen()) {
        QCOMPARE(controller.primaryScreenName(), primary->name());
    }
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    ShortcutNoteTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_shortcutnote.moc"
