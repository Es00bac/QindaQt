// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

#include <qindaqt/apps/settings_input/keyboard_config_port.h>

namespace QindaQt::Apps::SettingsInput {

// Keyboard repeat and NumLock-at-login presentation over the keyboard
// config port. Values are applied only through apply() so the test field
// and slider drags never half-write the authority file.
class KeyboardSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool keyRepeat READ keyRepeat WRITE setKeyRepeat NOTIFY
                   keyRepeatChanged)
    Q_PROPERTY(int repeatDelayMs READ repeatDelayMs WRITE setRepeatDelayMs
                   NOTIFY repeatDelayChanged)
    Q_PROPERTY(int repeatRate READ repeatRate WRITE setRepeatRate NOTIFY
                   repeatRateChanged)
    Q_PROPERTY(int numLockAtLogin READ numLockAtLogin WRITE setNumLockAtLogin
                   NOTIFY numLockChanged)
    // False when the last apply was refused; the route shows the reason and
    // keeps the editor open instead of pretending the change stuck.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY applyingChanged)

public:
    explicit KeyboardSettingsModel(const KeyboardConfigPort &port,
                                   QObject *parent = nullptr);

    // Reads the authority file into the presented values; called when the
    // tab becomes visible, never during construction.
    Q_INVOKABLE void refresh();
    // Persists the presented values and reports the exact outcome in
    // statusText: applied, stored-but-not-reloaded, or refused.
    Q_INVOKABLE void apply();

    [[nodiscard]] bool keyRepeat() const { return m_config.keyRepeat; }
    [[nodiscard]] int repeatDelayMs() const {
        return m_config.repeatDelayMs;
    }
    [[nodiscard]] int repeatRate() const { return m_config.repeatRate; }
    [[nodiscard]] int numLockAtLogin() const {
        return m_config.numLockAtLogin;
    }
    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] QString statusText() const { return m_statusText; }
    [[nodiscard]] bool applying() const { return m_applying; }

    void setKeyRepeat(bool value);
    void setRepeatDelayMs(int value);
    void setRepeatRate(int value);
    void setNumLockAtLogin(int value);

Q_SIGNALS:
    void keyRepeatChanged();
    void repeatDelayChanged();
    void repeatRateChanged();
    void numLockChanged();
    void availableChanged();
    void statusTextChanged();
    void applyingChanged();

private:
    const KeyboardConfigPort &m_port;
    KeyboardConfig m_config;
    bool m_available = true;
    bool m_applying = false;
    QString m_statusText;
};

} // namespace QindaQt::Apps::SettingsInput
