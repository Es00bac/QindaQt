// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/foundation_composition.h>
#include <qindaqt/services/portal/access_adaptor.h>
#include <qindaqt/services/portal/process_consent.h>
#include <qindaqt/services/portal/notification_adaptor.h>
#include <qindaqt/services/portal/inhibit_adaptor.h>
#include <qindaqt/services/portal/email_adaptor.h>
#include <qindaqt/services/portal/file_chooser_adaptor.h>
#include <qindaqt/services/portal/app_chooser_adaptor.h>
#include <qindaqt/services/portal/process_chooser.h>
#include "../misc_families/process_misc.h"
#include "../misc_families/account_adaptor.h"
#include "../misc_families/usb_adaptor.h"
#include "../misc_families/launcher_adaptor.h"
#include "../misc_families/print_adaptor.h"
#include "../shortcuts/global_shortcuts_adaptor.h"
#include "../shortcuts/shortcut_ui.h"
#include <qindaqt/services/shortcuts_client/transport.h>

#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/application_catalog/application_directory_scan.h>
namespace QindaQt::Services::Portal {
class PortalFoundationComposition::Private {
public:
    PortalSessionBinding session;
    RequestRegistry requests;
    ProcessAccessConsent consent;
    ProcessChooser chooser;
    ProcessShortcuts shortcutUi;
    QindaQt::Services::Shortcuts::QtShortcutTransport shortcutNative;
    ProcessMisc misc;
    QtNativeNotifications notifications;
    QindaQt::Power::QtPowerTransport power;
    PowerIdleInhibition idle;
    std::unique_ptr<QindaQt::Apps::SettingsDefaultApps::DefaultApplicationsStore> store;
    QindaQt::Services::ApplicationUri::DefaultApplicationUriOpener uri;
    // AGENT-GUARD: Adaptors are destroyed before every borrowed port/store.
    // Their QObject host parent is independent ownership, removed on deletion.
    std::unique_ptr<AccessAdaptor> access;
    std::unique_ptr<NotificationAdaptor> notification;
    std::unique_ptr<InhibitAdaptor> inhibit;
    std::unique_ptr<EmailAdaptor> email;
    std::unique_ptr<FileChooserAdaptor> fileChooser;
    std::unique_ptr<AppChooserAdaptor> appChooser;
    std::unique_ptr<GlobalShortcutsAdaptor> shortcuts;
    std::unique_ptr<AccountAdaptor> account;
    std::unique_ptr<UsbAdaptor> usb;
    std::unique_ptr<LauncherAdaptor> launcher;
    std::unique_ptr<PrintAdaptor> print;

    Private(QObject &host, QDBusConnection bus, QString runtime, QString helper,
        QString relay, const QStringList &roots,
        QindaQt::ApplicationCatalog::DirectoryScan scan, QString chooserHelper, QString /*captureHelper*/, QString shortcutHelper, QString miscHelper)
        : session(bus, runtime), requests(bus), consent(session, bus, std::move(helper)),
          chooser(session, consent, std::move(chooserHelper)),
          shortcutUi(session, consent, std::move(shortcutHelper)), shortcutNative(bus),
          misc(session, consent, std::move(miscHelper)),
          notifications(bus), power(bus), idle(power, [this] { return consent.admitted(); }),
          store(QindaQt::Apps::SettingsDefaultApps::createSessionDefaultApplicationsStore(roots, scan)),
          uri(*store, std::move(scan), std::move(relay),
              [this](quint64 token) { return requests.live(token) && consent.admitted(); },
              [this] { return session.openDisplay(); }),
          access(std::make_unique<AccessAdaptor>(host, requests, consent)),
          notification(std::make_unique<NotificationAdaptor>(host, requests, notifications, bus)),
          inhibit(std::make_unique<InhibitAdaptor>(host, requests, idle, bus)),
          email(std::make_unique<EmailAdaptor>(host, requests, uri, [this] { return consent.admitted(); })),
          fileChooser(std::make_unique<FileChooserAdaptor>(host, requests, chooser)),
          appChooser(std::make_unique<AppChooserAdaptor>(host, requests, chooser, [roots] {
              return QindaQt::ApplicationCatalog::scanApplicationDirectories(roots,
                  QindaQt::ApplicationCatalog::ApplicationVisibility::IncludeNoDisplay);
          }, bus)),
          shortcuts(std::make_unique<GlobalShortcutsAdaptor>(host, requests, shortcutUi, shortcutNative, bus)),
          account(std::make_unique<AccountAdaptor>(host, requests, misc)),
          usb(std::make_unique<UsbAdaptor>(host, requests, misc)),
          launcher(std::make_unique<LauncherAdaptor>(host, requests, misc)),
          print(std::make_unique<PrintAdaptor>(host, requests, misc)) {
        // AGENT-GUARD: capture and remote input live together in the inherited-
        // capability broker (ADR-0341), never on this ordinary backend host.
        QObject::connect(&consent, &AccessConsent::authorityLost, &requests, [this] {
            requests.retireAll(); idle.revoke();
        });
    }
};
PortalFoundationComposition::PortalFoundationComposition(QObject &host, QDBusConnection bus,
    QString runtime, QString consent, QString relay, QStringList roots)
    : PortalFoundationComposition(host, std::move(bus), std::move(runtime), std::move(consent),
        std::move(relay), std::move(roots), QString{}) {}
PortalFoundationComposition::PortalFoundationComposition(QObject &host, QDBusConnection bus,
    QString runtime, QString consent, QString relay, QStringList roots, QString chooser)
    : PortalFoundationComposition(host, std::move(bus), std::move(runtime), std::move(consent),
        std::move(relay), std::move(roots), std::move(chooser), QString{}) {}
PortalFoundationComposition::PortalFoundationComposition(QObject &host, QDBusConnection bus,
    QString runtime, QString consent, QString relay, QStringList roots, QString chooser, QString capture)
    : PortalFoundationComposition(host, std::move(bus), std::move(runtime), std::move(consent), std::move(relay), std::move(roots), std::move(chooser), std::move(capture), QString{}) {}
PortalFoundationComposition::PortalFoundationComposition(QObject &host, QDBusConnection bus,
    QString runtime, QString consent, QString relay, QStringList roots, QString chooser, QString capture, QString shortcuts)
    : PortalFoundationComposition(host, std::move(bus), std::move(runtime), std::move(consent),
        std::move(relay), std::move(roots), std::move(chooser), std::move(capture), std::move(shortcuts), QString{}) {}
PortalFoundationComposition::PortalFoundationComposition(QObject &host, QDBusConnection bus,
    QString runtime, QString consent, QString relay, QStringList roots, QString chooser, QString capture, QString shortcuts, QString misc)
    : d(std::make_unique<Private>(host, bus, std::move(runtime), std::move(consent), std::move(relay), roots,
        QindaQt::ApplicationCatalog::scanApplicationDirectories(roots,
            QindaQt::ApplicationCatalog::ApplicationVisibility::IncludeNoDisplay), std::move(chooser), std::move(capture), std::move(shortcuts), std::move(misc))) {}
PortalFoundationComposition::~PortalFoundationComposition() { stop(); }
bool PortalFoundationComposition::start() { return d->session.start(); }
void PortalFoundationComposition::stop() { d->requests.retireAll(); d->idle.revoke(); d->session.stop(); }
} // namespace QindaQt::Services::Portal
