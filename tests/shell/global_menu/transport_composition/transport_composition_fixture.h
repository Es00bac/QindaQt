// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h>
#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_client.h>
#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>

#include "fake_dbusmenu_exporter.h"

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtTest/QSignalSpy>

#include <memory>
#include <optional>

namespace QindaQt::Shell::GlobalMenu::TestSupport {

class FakeActiveWindowSource final : public Ownership::ActiveWindowSource
{
public:
    std::optional<Ownership::ActiveWindowObservation> observation;

    [[nodiscard]] std::optional<Ownership::ActiveWindowObservation> activeWindow() const override
    {
        return observation;
    }
};

class FakeRegistrarWindowIdSource final : public Composition::RegistrarWindowIdSource
{
public:
    QUuid expectedWindow;
    quint32 registrarId = 0;

    [[nodiscard]] std::optional<quint32> registrarWindowIdFor(
        const Ownership::WindowIdentity &window) const override
    {
        return window.windowId == expectedWindow && registrarId != 0
            ? std::optional<quint32>(registrarId)
            : std::nullopt;
    }
};

inline QDBusMessage registrarCall(const QDBusConnection &connection, const QString &method,
                                  const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(Registrar::kRegistrarServiceName),
        QString::fromLatin1(Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Registrar::kRegistrarInterface), method);
    message.setArguments(arguments);
    QDBusPendingCallWatcher watcher(connection.asyncCall(message, 5'000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        (void)finished.wait(5'000);
    }
    return watcher.reply();
}

// Shared private-bus binding for the churn and replacement rows: one registrar,
// one fake provider owning window 77's menu at /Menu, and one focused observation.
struct BoundFixture {
    QString registrarName;
    QString providerName;
    QString shellName;
    // QDBusConnection has no default constructor; named unconnected placeholders
    // are overwritten by connectToBus in bindFixture.
    QDBusConnection registrarBus = QDBusConnection(QStringLiteral("placeholder-r"));
    QDBusConnection providerBus = QDBusConnection(QStringLiteral("placeholder-p"));
    QDBusConnection shellBus = QDBusConnection(QStringLiteral("placeholder-s"));
    std::unique_ptr<Test::FakeDbusMenuExporter> exporter;
    std::unique_ptr<Registrar::AppMenuRegistrar> registrar;
    QUuid windowId;
    FakeActiveWindowSource active;
    FakeRegistrarWindowIdSource resolver;
    quint64 generation = 5;
};

inline bool bindFixture(BoundFixture &fixture, const QString &tag)
{
    DbusMenu::registerDbusMenuWireTypes();
    Registrar::registerRegistrarWireTypes();
    fixture.registrarName = QStringLiteral("qindaqt-flicker-registrar-%1").arg(tag);
    fixture.providerName = QStringLiteral("qindaqt-flicker-provider-%1").arg(tag);
    fixture.shellName = QStringLiteral("qindaqt-flicker-shell-%1").arg(tag);
    fixture.registrarBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, fixture.registrarName);
    fixture.providerBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, fixture.providerName);
    fixture.shellBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, fixture.shellName);
    if (!fixture.registrarBus.isConnected() || !fixture.providerBus.isConnected()
        || !fixture.shellBus.isConnected()) {
        return false;
    }
    fixture.exporter = std::make_unique<Test::FakeDbusMenuExporter>();
    fixture.exporter->setLayout(1, Test::menuLayout());
    if (!fixture.providerBus.registerObject(
            QStringLiteral("/Menu"), fixture.exporter.get(),
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals
                | QDBusConnection::ExportScriptableProperties)) {
        return false;
    }
    fixture.registrar = std::make_unique<Registrar::AppMenuRegistrar>(fixture.registrarBus);
    if (fixture.registrar->start() != Registrar::RegistrarStartStatus::Started) {
        return false;
    }
    if (registrarCall(fixture.providerBus, QStringLiteral("RegisterWindow"),
                      {QVariant::fromValue(quint32{77}),
                       QVariant::fromValue(QDBusObjectPath(QStringLiteral("/Menu")))})
            .type()
        != QDBusMessage::ReplyMessage) {
        return false;
    }
    fixture.windowId = QUuid::createUuid();
    fixture.active.observation = Ownership::ActiveWindowObservation{
        .window = Ownership::WindowIdentity{
            .windowId = fixture.windowId,
            .processId = static_cast<qint64>(QCoreApplication::applicationPid())},
        .focusGeneration = fixture.generation};
    fixture.resolver.expectedWindow = fixture.windowId;
    fixture.resolver.registrarId = 77;
    return true;
}

inline void teardownFixture(BoundFixture &fixture)
{
    fixture.registrar->stop();
    QDBusConnection::disconnectFromBus(fixture.providerName);
    QDBusConnection::disconnectFromBus(fixture.shellName);
    QDBusConnection::disconnectFromBus(fixture.registrarName);
}

} // namespace QindaQt::Shell::GlobalMenu::TestSupport
