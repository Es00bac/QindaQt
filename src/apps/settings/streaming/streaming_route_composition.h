// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsStreaming {

// Process-lifetime QML singleton composition for the Streaming route. It
// owns the production obs-websocket transport and client, the scoped keyring
// store, and the purpose-scoped Settings1 client.
//
// AGENT-CONTRACT: This is the only place that names production transports.
// The model sees interfaces; QML sees the model. Construction performs no
// socket connect, no keyring read and no file IO, so importing this module
// stays cheap for every other route and the Settings window still opens when
// OBS, the keyring or Settings1 are all absent.
class StreamingRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *streaming READ streaming CONSTANT)

public:
    explicit StreamingRouteComposition(QObject *parent = nullptr);
    ~StreamingRouteComposition() override;

    [[nodiscard]] QObject *streaming() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsStreaming
