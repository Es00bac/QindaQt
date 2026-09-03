// SPDX-License-Identifier: GPL-3.0-or-later
#include "clipboard_route_composition.h"

#include <qindaqt/apps/settings_clipboard/clipboard_settings_model.h>
#include <qindaqt/services/clipboard_client/clipboard_client.h>
#include <qindaqt/services/clipboard_client/qt_clipboard_transport.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsClipboard {

class ClipboardRouteComposition::Private final {
public:
    Private()
        : settingsTransport(QDBusConnection::sessionBus())
        , settingsClient(settingsTransport,
                         {QString::fromLatin1(ClipboardHistorySettingsKey)})
        , clipboardTransport(QDBusConnection::sessionBus())
        , clipboardClient(&clipboardTransport)
        , model(settingsClient, clipboardClient)
    {
        // AGENT-NOTE: Like CustomizeRouteComposition, this QML singleton is
        // the route-local composition root. The public clients retain their
        // own independent request-token domains and the model sees no bus.
        QString ignoredError;
        const bool settingsStarted = settingsClient.start(&ignoredError);
        Q_UNUSED(settingsStarted);
        clipboardClient.start();
    }

    Services::SettingsClient::QtSettingsTransport settingsTransport;
    Services::SettingsClient::SettingsClient settingsClient;
    Services::Clipboard::QtClipboardTransport clipboardTransport;
    Services::Clipboard::ClipboardClient clipboardClient;
    ClipboardSettingsModel model;
};

ClipboardRouteComposition::ClipboardRouteComposition(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

ClipboardRouteComposition::~ClipboardRouteComposition() = default;

QObject *ClipboardRouteComposition::model() const
{
    return &d->model;
}

} // namespace QindaQt::Apps::SettingsClipboard
