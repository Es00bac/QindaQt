// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Services::StreamingPreferences {

// Same-thread, borrowed interface; the composition owns its lifetime.
// Only confirmed Settings1 snapshots may change these published values.
// A setter returning true means the asynchronous request was admitted, not
// saved; observe writePending/writeStatusChanged and the next snapshot.
// UI text from writeStatusText is diagnostic, not a stable machine protocol.
class StreamingPreferences : public QObject {
    Q_OBJECT
public:
    explicit StreamingPreferences(QObject *parent = nullptr) : QObject(parent) {}
    ~StreamingPreferences() override = default;

    [[nodiscard]] virtual bool isLoaded() const = 0;
    [[nodiscard]] virtual int webSocketPort() const = 0;
    [[nodiscard]] virtual bool autoConnect() const = 0;
    [[nodiscard]] virtual bool startObsAtLogin() const = 0;
    [[nodiscard]] virtual bool writePending() const { return false; }
    [[nodiscard]] virtual QString writeStatusText() const { return {}; }
    virtual bool setWebSocketPort(int port) = 0;
    virtual bool setAutoConnect(bool enabled) = 0;
    virtual bool setStartObsAtLogin(bool enabled) = 0;

Q_SIGNALS:
    void preferencesChanged();
    void writeStatusChanged();
};

} // namespace QindaQt::Services::StreamingPreferences
