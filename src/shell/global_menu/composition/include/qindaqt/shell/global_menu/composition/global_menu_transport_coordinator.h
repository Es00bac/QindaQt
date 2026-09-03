// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/composition/registrar_window_id_source.h>
#include <qindaqt/shell/global_menu/exporter/menu_exporter.h>
#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>
#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>
#include <qindaqt/shell/global_menu/registrar/qt_bus_credential_source.h>
#include <qindaqt/shell/global_menu/registrar/registrar_registry.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{
class DbusMenuClient;
}

namespace QindaQt::Shell::GlobalMenu::Composition
{

// Shell-neutral join of focused-window facts, registrar ownership, dbusmenu
// snapshots, G0 lineage, and the applet facade. The later runtime lane owns
// instantiation and calls refreshFocus() after each authenticated focus fact.
// Dependencies and connection are injected and must outlive this object; the
// coordinator owns its client/exporter and no bus name. It and all dependencies
// are single-threaded Qt objects. Any missing/failed proof publishes unavailable.
class GlobalMenuTransportCoordinator final : public QObject,
                                             public Exporter::ExportLineageSource
{
    Q_OBJECT

public:
    GlobalMenuTransportCoordinator(
        QDBusConnection connection, const Ownership::ActiveWindowSource &activeWindowSource,
        const RegistrarWindowIdSource &windowIdSource,
        Registrar::RegistrarRegistry &registry, GlobalMenuAppletAccess &applet,
        QObject *parent = nullptr);
    ~GlobalMenuTransportCoordinator() override;

    void refreshFocus();
    void stop();
    [[nodiscard]] std::optional<Exporter::ExportLineage> lineageFor(
        const QUuid &ownerWindowId) const override;
    [[nodiscard]] std::optional<Protocol::MenuTree> publishedTree() const;

Q_SIGNALS:
    void activationRejected(QString reasonCode);

private:
    void clearAuthority();
    void bindRegistration(const Ownership::ActiveWindowObservation &focus,
                          const Registrar::AppMenuRegistration &registration);
    void publishClientTree();
    void activate(const QString &actionId);

    QDBusConnection m_connection;
    const Ownership::ActiveWindowSource &m_activeWindowSource;
    const RegistrarWindowIdSource &m_windowIdSource;
    Registrar::RegistrarRegistry &m_registry;
    GlobalMenuAppletAccess &m_applet;
    Registrar::QtBusCredentialSource m_credentials;
    Ownership::ProviderAuthenticator m_authenticator;
    Ownership::ActiveProviderSelector m_selector;
    std::unique_ptr<DbusMenu::DbusMenuClient> m_client;
    std::unique_ptr<Exporter::MenuExporter> m_exporter;
    quint64 m_registrationGeneration = 0;
    quint64 m_focusGeneration = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Composition
