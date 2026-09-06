// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/applet/globalmenuappletaccess.h>
#include <qindaqt/shell/global_menu/composition/announced_menu_address_source.h>
#include <qindaqt/shell/global_menu/composition/registrar_window_id_source.h>
#include <qindaqt/shell/global_menu/exporter/menu_exporter.h>
#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>
#include <qindaqt/shell/global_menu/ownership/provider_authenticator.h>
#include <qindaqt/shell/global_menu/registrar/qt_bus_credential_source.h>
#include <qindaqt/shell/global_menu/registrar/registrar_registry.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Shell::GlobalMenu::DbusMenu
{
class DbusMenuClient;
}

class QDBusServiceWatcher;

namespace QindaQt::Shell::GlobalMenu::Composition
{

// Shell-neutral join of focused-window facts, registrar ownership, dbusmenu
// snapshots, G0 lineage, and the applet facade. Shell runtime composition owns
// instantiation and calls refreshFocus() after each authenticated identity fact.
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
    GlobalMenuTransportCoordinator(
        QDBusConnection connection, const Ownership::ActiveWindowSource &activeWindowSource,
        const RegistrarWindowIdSource &windowIdSource,
        const AnnouncedMenuAddressSource &announcedMenuSource,
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
    void hostedMenuChanged(QString providerUniqueName, QString objectPath,
                           bool hosted);

private:
    struct ProviderEndpoint final {
        QString uniqueOwner;
        QString objectPath;
        QString announcedService;
        quint64 registrationGeneration = 0;

        bool operator==(const ProviderEndpoint &) const = default;
    };

    [[nodiscard]] std::optional<ProviderEndpoint> endpointFor(
        const Ownership::ActiveWindowObservation &focus);
    void watchAnnouncedService(const QString &serviceName);
    void clearAuthority();
    // Revokes invocation authority synchronously (selector cleared, so the
    // invocation guard can never admit an action) but retains the last
    // published presentation AND facade availability for a bounded grace
    // period. The compositor identity channel invalidates and republishes on
    // every visibility-affecting change — including pure geometry changes and
    // the menu popup's own surface appearing — so dropping facade
    // availability on each transient withdrawal closes the open popup and
    // disables delegates mid-click, the user-visible "menu items are dead"
    // defect. Only a reread proving a different window/endpoint fences the
    // retained entries (bindRegistration opens an inert transition); when the
    // grace expires without the same provider re-proving itself, the retained
    // presentation is published unavailable.
    void suspendAuthority();
    void expirePresentationGrace();
    void renewBoundProvider(const Ownership::ActiveWindowObservation &focus);
    void bindRegistration(const Ownership::ActiveWindowObservation &focus,
                          const ProviderEndpoint &endpoint);
    void publishClientTree();
    void activate(const QString &actionId);
    void refreshHostedMenu();
    void withdrawHostedMenu(const ProviderEndpoint &endpoint);

    QDBusConnection m_connection;
    const Ownership::ActiveWindowSource &m_activeWindowSource;
    const RegistrarWindowIdSource &m_windowIdSource;
    const AnnouncedMenuAddressSource *m_announcedMenuSource = nullptr;
    Registrar::RegistrarRegistry &m_registry;
    GlobalMenuAppletAccess &m_applet;
    Registrar::QtBusCredentialSource m_credentials;
    Ownership::ProviderAuthenticator m_authenticator;
    Ownership::ActiveProviderSelector m_selector;
    std::unique_ptr<DbusMenu::DbusMenuClient> m_client;
    std::unique_ptr<Exporter::MenuExporter> m_exporter;
    QDBusServiceWatcher *m_announcedServiceWatcher = nullptr;
    ProviderEndpoint m_boundEndpoint;
    ProviderEndpoint m_lastHostedEndpoint;
    QUuid m_lastHostedWindowId;
    // Window the current/last binding was authenticated for; retained through a
    // presentation grace so a reread of the same window can resume in place.
    Ownership::WindowIdentity m_boundWindow{};
    QString m_watchedAnnouncedService;
    QTimer m_presentationGraceTimer;
    quint64 m_focusGeneration = 0;
    quint64 m_clientGeneration = 0;
    bool m_hosted = false;
    bool m_authoritySuspended = false;
};

} // namespace QindaQt::Shell::GlobalMenu::Composition
