// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0260 provider selection end to end on a private bus: the production
// global-menu composition (registrar, authenticated dbusmenu transport,
// facade) and the desktop menu composition share one facade and one
// compositor identity client. No active window (which is also how the
// compositor reports the desktop surface itself being active) shows the
// desktop menu; an authenticated application window replaces it; focus
// handed back to the desktop restores it. Every stock layout is resolved to
// prove the desktop menu exists exactly where a global menu is hosted.

#include "desktopmenucomposition.h"
#include "globalmenuappletcomposition.h"

#include "fake_dbusmenu_exporter.h"

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/compositor/shellwindowidentity.h>
#include <qindaqt/profiles/profile_catalog.h>
#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_client.h>
#include <qindaqt/shell_window_actions_client/shell_window_actions_transport.h>

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt;
using Presence = Shell::DesktopMenu::DesktopMenuController::Presence;

namespace {

struct CatalogFixture final {
    Applets::ManifestCatalog catalog;
    AppletHost::CapabilityPolicy policy;

    bool load(QString *error)
    {
        if (!catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), error)) {
            return false;
        }
        const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
        if (!loaded.ok) {
            *error = loaded.error;
            return false;
        }
        policy = loaded.policy;
        return true;
    }
};

// Stands in for the compositor's exact-owner identity endpoint. A snapshot
// without an active window is what KWin publishes when nothing, the desktop
// layer, or a shell surface holds focus (shellvisibilitywindowadmission).
class FakeIdentityTransport final : public ShellWindowActionsClient::ShellWindowActionsTransport {
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void request(quint64, const QString &, Compositor::ShellWindowAction, const QString &,
                 const Compositor::ShellWindowGeneration &) override
    {
    }
    void requestIdentity(quint64 token, const QString &owner) override
    {
        m_request = qMakePair(token, owner);
        ++requests;
    }

    int requests = 0;

    void publishOwner(const QString &owner) { Q_EMIT serviceOwnerChanged(owner); }

    void publish(std::optional<Compositor::ShellWindowIdentityFacts> active, quint64 revision)
    {
        QVERIFY(m_request.has_value());
        const Compositor::ShellWindowIdentitySnapshot snapshot{
            Compositor::ShellWindowIdentityStatus::Ok,
            QStringLiteral("test-identity-epoch"),
            revision,
            {QStringLiteral("action-epoch"), 3},
            std::move(active),
            {},
            {}};
        Q_EMIT identityReplyReceived(m_request->first, m_request->second,
                                     Compositor::encodeShellWindowIdentitySnapshot(snapshot));
    }

    void publishApplication(quint32 registrarWindowId, quint64 revision)
    {
        publish(Compositor::ShellWindowIdentityFacts{
                    QStringLiteral("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"),
                    static_cast<qint64>(QCoreApplication::applicationPid()),
                    registrarWindowId, std::nullopt, std::nullopt},
                revision);
    }

    // Withdraws the snapshot and waits for the client's reread request, as the
    // compositor's directed invalidation does.
    void invalidate()
    {
        const int before = requests;
        Q_EMIT identityInvalidated(QStringLiteral(":1.900"));
        QTRY_VERIFY(requests > before);
    }

private:
    std::optional<QPair<quint64, QString>> m_request;
};

QDBusMessage registrarCall(const QDBusConnection &connection, const QString &method,
                           const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(Shell::GlobalMenu::Registrar::kRegistrarServiceName),
        QString::fromLatin1(Shell::GlobalMenu::Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Shell::GlobalMenu::Registrar::kRegistrarInterface), method);
    message.setArguments(arguments);
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        (void)finished.wait(5'000);
    }
    return watcher.reply();
}

QString topText(const Shell::GlobalMenu::GlobalMenuAppletAccess &access)
{
    return access.items().value(0).toMap().value(QStringLiteral("text")).toString();
}

QString generation(const Shell::GlobalMenu::GlobalMenuAppletAccess &access)
{
    return access.items().value(0).toMap().value(QStringLiteral("generation")).toString();
}

} // namespace

class DesktopMenuSelectionTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void desktopMenuShowsUntilAnApplicationProvesItsMenu();
    void identityStatesMapToPresence();
    void desktopMenuExistsExactlyWhereAGlobalMenuIsHosted();
};

void DesktopMenuSelectionTest::desktopMenuShowsUntilAnApplicationProvesItsMenu()
{
    auto shellBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                  QStringLiteral("desktop-menu-shell"));
    auto providerBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                     QStringLiteral("desktop-menu-provider"));
    QVERIFY(shellBus.isConnected());
    QVERIFY(providerBus.isConnected());
    Shell::GlobalMenu::DbusMenu::registerDbusMenuWireTypes();
    Shell::GlobalMenu::Registrar::registerRegistrarWireTypes();
    Shell::GlobalMenu::Test::FakeDbusMenuExporter exporter;
    exporter.setLayout(1, Shell::GlobalMenu::Test::menuLayout());
    QVERIFY(providerBus.registerObject(QStringLiteral("/Menu"), &exporter,
                                       QDBusConnection::ExportScriptableSlots
                                           | QDBusConnection::ExportScriptableSignals
                                           | QDBusConnection::ExportScriptableProperties));
    QSignalSpy applicationEvents(&exporter,
                                 &Shell::GlobalMenu::Test::FakeDbusMenuExporter::eventObserved);

    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    FakeIdentityTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    QVERIFY(client.start());
    transport.publishOwner(QStringLiteral(":1.900"));
    // Nothing (or only the desktop surface) is active.
    transport.publish(std::nullopt, 1);
    QTRY_VERIFY(client.identityAvailable());

    Shell::GlobalMenuAppletComposition composition(fixture.catalog, fixture.policy, shellBus,
                                                   client);
    composition.start();
    QCOMPARE(static_cast<int>(composition.status()),
             static_cast<int>(Shell::GlobalMenuRuntimeStatus::Ready));
    auto *access = composition.access();

    int gatherToggles = 0;
    int settingsRoutes = 0;
    Shell::ShellDesktopMenuTargets::Hooks hooks;
    hooks.toggleGatherOverview = [&gatherToggles] { ++gatherToggles; };
    // The Settings route owner makes the File Manager (system) menu present,
    // so the bar leads with it as it does in the production shell.
    hooks.openSettingsRoute = [&settingsRoutes](const QString &, const QString &) {
        ++settingsRoutes;
        return true;
    };
    Shell::DesktopMenuComposition desktop({access, &client, {}, std::move(hooks)});
    QVERIFY(desktop.controller() != nullptr);
    QCOMPARE(desktop.controller()->presence(), Presence::NoApplication);
    // Not hosted yet: a layout without a global menu shows no desktop menu.
    QVERIFY(!access->desktopMenuShown());
    desktop.followLayout(true, {});
    QVERIFY(access->desktopMenuShown());
    QVERIFY(access->available());
    QCOMPARE(topText(*access), QStringLiteral("File Manager"));
    QCOMPARE(access->desktopMenuTitle(), QStringLiteral("File Manager"));

    // An application window gains focus and proves its menu.
    QCOMPARE(registrarCall(providerBus, QStringLiteral("RegisterWindow"),
                           {QVariant::fromValue(quint32{77}),
                            QVariant::fromValue(QDBusObjectPath(QStringLiteral("/Menu")))})
                 .type(),
             QDBusMessage::ReplyMessage);
    transport.invalidate();
    transport.publishApplication(77, 2);
    QTRY_COMPARE(desktop.controller()->presence(), Presence::ApplicationActive);
    // Never actionable for the new focus, from the first observation.
    QVERIFY(!desktop.controller()->actionable());
    QTRY_VERIFY_WITH_TIMEOUT(access->available() && topText(*access) == QStringLiteral("File"),
                             5'000);
    QVERIFY(!access->desktopMenuShown());
    QVERIFY(access->applicationAvailable());
    QTRY_VERIFY_WITH_TIMEOUT(!desktop.controller()->published(), 2'000);

    // The application's menu still invokes through its own authenticated
    // path, and only there.
    access->activate(QStringLiteral("1"));
    QTRY_COMPARE_WITH_TIMEOUT(applicationEvents.size(), 1, 5'000);
    QCOMPARE(gatherToggles, 0);

    // Focus returns to the desktop: after the application's presentation
    // grace the desktop menu is back and actionable.
    transport.invalidate();
    transport.publish(std::nullopt, 3);
    QTRY_COMPARE(desktop.controller()->presence(), Presence::NoApplication);
    QVERIFY(desktop.controller()->actionable());
    QTRY_VERIFY_WITH_TIMEOUT(access->desktopMenuShown(), 5'000);
    QVERIFY(access->available());
    QCOMPARE(topText(*access), QStringLiteral("File Manager"));
    QVERIFY(!access->applicationAvailable());
    access->activate(QStringLiteral("desktop.gather-overview"), generation(*access));
    QCOMPARE(gatherToggles, 1);
    QCOMPARE(settingsRoutes, 0);
    QCOMPARE(applicationEvents.size(), 1);

    // Leaving the hosting layout withdraws it with the global menu.
    desktop.followLayout(false, {});
    QVERIFY(!access->desktopMenuShown());
    QVERIFY(access->items().isEmpty());

    composition.stop();
    client.stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("desktop-menu-provider"));
    QDBusConnection::disconnectFromBus(QStringLiteral("desktop-menu-shell"));
}

void DesktopMenuSelectionTest::identityStatesMapToPresence()
{
    FakeIdentityTransport transport;
    ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
    // No identity yet: unknown, never "no application".
    QCOMPARE(Shell::DesktopMenuComposition::presenceFrom(client), Presence::Unknown);
    QVERIFY(client.start());
    transport.publishOwner(QStringLiteral(":1.900"));
    transport.publish(std::nullopt, 1);
    QTRY_VERIFY(client.identityAvailable());
    QCOMPARE(Shell::DesktopMenuComposition::presenceFrom(client), Presence::NoApplication);
    // A withdrawn identity (the reread after any visibility change) is unknown.
    transport.invalidate();
    QTRY_VERIFY(!client.identityAvailable());
    QCOMPARE(Shell::DesktopMenuComposition::presenceFrom(client), Presence::Unknown);
    transport.publishApplication(77, 2);
    QTRY_VERIFY(client.identityAvailable());
    QCOMPARE(Shell::DesktopMenuComposition::presenceFrom(client), Presence::ApplicationActive);
    client.stop();
    QCOMPARE(Shell::DesktopMenuComposition::presenceFrom(client), Presence::Unknown);
}

void DesktopMenuSelectionTest::desktopMenuExistsExactlyWhereAGlobalMenuIsHosted()
{
    // AGENTS.md shell rule: resolve every stock profile through the same
    // AppletInstanceResolver path the panel dispatcher uses.
    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectories({QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles")},
                                      &error),
             qPrintable(error));
    struct Expected {
        bool globalMenu;
        bool launcher;
        bool clipboard;
    };
    const QHash<QString, Expected> hosting{
        {QStringLiteral("qindaqt"), {true, true, true}},
        {QStringLiteral("macos-inspired"), {true, true, true}},
        // Unity's rail uses the application-launcher control, which has no
        // open request; the desktop menu then omits Find….
        {QStringLiteral("unity-inspired"), {true, false, true}},
    };
    int seen = 0;
    for (const auto &profile : profiles.profiles()) {
        const bool globalMenu = Shell::GlobalMenuAppletComposition::layoutHostsGlobalMenu(
            profile, fixture.catalog, fixture.policy);
        const auto hosted = Shell::DesktopMenuComposition::hostedApplets(profile, fixture.catalog,
                                                                         fixture.policy);
        if (hosting.contains(profile.id)) {
            const Expected expected = hosting.value(profile.id);
            QVERIFY2(globalMenu == expected.globalMenu, qPrintable(profile.id));
            QVERIFY2(hosted.launcher == expected.launcher, qPrintable(profile.id));
            QVERIFY2(hosted.clipboard == expected.clipboard, qPrintable(profile.id));
            ++seen;
        } else {
            // ADR-0130 layouts keep menus in windows and show no desktop menu.
            QVERIFY2(!globalMenu, qPrintable(profile.id));
        }

        // The runtime rule (ShellRuntimeApplication::followDesktopMenuLayout)
        // over a live facade and a proven empty state.
        Shell::GlobalMenu::GlobalMenuAppletAccess access;
        FakeIdentityTransport transport;
        ShellWindowActionsClient::ShellWindowActionsClient client(transport, 500);
        QVERIFY(client.start());
        transport.publishOwner(QStringLiteral(":1.900"));
        transport.publish(std::nullopt, 1);
        QTRY_VERIFY(client.identityAvailable());
        Shell::ShellDesktopMenuTargets::Hooks hooks;
        hooks.toggleGatherOverview = [] {};
        Shell::DesktopMenuComposition desktop({&access, &client, {}, std::move(hooks)});
        desktop.followLayout(globalMenu, hosted);
        QVERIFY2(access.desktopMenuShown() == globalMenu, qPrintable(profile.id));
        client.stop();
    }
    QCOMPARE(seen, 3);
}

QTEST_GUILESS_MAIN(DesktopMenuSelectionTest)

#include "tst_desktop_menu_selection_private_bus.moc"
