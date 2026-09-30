// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyring_route_composition.h"

#include <qindaqt/apps/settings_keyring/keyring_preferences.h>
#include <qindaqt/apps/settings_keyring/keyring_settings_model.h>
#include <qindaqt/apps/settings_keyring/sensitive_clipboard.h>
#include <qindaqt/services/keyring_client/qt_keyring_gateway.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QDBusConnection>
#include <QUuid>

#include <memory>

namespace QindaQt::Apps::SettingsKeyring {
class KeyringRouteComposition::Private final {
public:
    Private()
        : connectionName(QStringLiteral("qindaqt-settings-keyring-%1")
                             .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
        , bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                            connectionName))
        , settingsTransport(std::make_unique<Services::SettingsClient::QtSettingsTransport>(bus))
        , settingsClient(std::make_unique<Services::SettingsClient::SettingsClient>(
              *settingsTransport, KeyringPreferences::scopedKeys()))
        , preferences(std::make_unique<KeyringPreferences>(*settingsClient))
        , gateway(std::make_unique<Services::KeyringClient::QtKeyringGateway>(bus))
        , model(std::make_unique<KeyringSettingsModel>(*gateway, preferences.get()))
        , clipboard(std::make_unique<SensitiveClipboard>())
    {
        QString ignoredError;
        const bool settingsStarted = settingsClient->start(&ignoredError);
        Q_UNUSED(settingsStarted);
        QObject::connect(model.get(), &KeyringSettingsModel::copyRequested,
                         model.get(), [this](const std::shared_ptr<qindaqt::keyring::SecureBuffer> &bytes) {
            const bool copied = clipboard->copy(bytes);
            model->acknowledgeCopy(copied);
        });
        QObject::connect(model.get(), &KeyringSettingsModel::deactivated,
                         clipboard.get(), &SensitiveClipboard::clear);
        QObject::connect(gateway.get(), &Services::KeyringClient::KeyringGateway::authorityChanged,
                         clipboard.get(), &SensitiveClipboard::clear);
        QObject::connect(gateway.get(), &Services::KeyringClient::KeyringGateway::secretsInvalidated,
                         clipboard.get(), &SensitiveClipboard::clear);
    }

    ~Private()
    {
        // Teardown follows the borrowed-lifetime chain before disconnecting the
        // route-private bus; no other Settings route loses its transport.
        model->deactivate();
        gateway->cancel();
        clipboard->clear();
        settingsClient->stop();
        settingsTransport->stop();
        model.reset();
        clipboard.reset();
        gateway.reset();
        preferences.reset();
        settingsClient.reset();
        settingsTransport.reset();
        QDBusConnection::disconnectFromBus(connectionName);
    }

    QString connectionName;
    QDBusConnection bus;
    std::unique_ptr<Services::SettingsClient::QtSettingsTransport> settingsTransport;
    std::unique_ptr<Services::SettingsClient::SettingsClient> settingsClient;
    std::unique_ptr<KeyringPreferences> preferences;
    std::unique_ptr<Services::KeyringClient::QtKeyringGateway> gateway;
    std::unique_ptr<KeyringSettingsModel> model;
    std::unique_ptr<SensitiveClipboard> clipboard;
};

KeyringRouteComposition::KeyringRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}
KeyringRouteComposition::~KeyringRouteComposition() = default;
QObject *KeyringRouteComposition::model() const { return d->model.get(); }
} // namespace QindaQt::Apps::SettingsKeyring
