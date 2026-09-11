// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QString>

#include <qindaqt/apps/settings_input/store_result.h>

namespace QindaQt::Apps::SettingsInput {

// Keyboard repeat and NumLock-at-login values as stored in the desktop
// input configuration (kcminputrc [Keyboard]). These are authority-file
// values, not Settings1 values: the Settings1 `input.*` keys are superseded
// and stay unconsumed (ADR-0134).
//
// numLockAtLogin: 0 = off at login, 1 = on at login, 2 = keep as is.
struct KeyboardConfig {
    bool keyRepeat = true;
    int repeatDelayMs = 500; // clamped to [100, 2000]
    int repeatRate = 25;     // repeats per second, clamped to [1, 100]
    int numLockAtLogin = 2;
};

[[nodiscard]] bool isValidKeyboardConfig(const KeyboardConfig &config) noexcept;

// Port to the keyboard repeat/NumLock configuration. The Qt adapter writes
// KConfig into the injected file location and then announces the change, distinguishing `Stored` from `StoredButReloadFailed` so the route
// never claims a live change it did not observe (ADR-0134).
class KeyboardConfigPort {
public:
    virtual ~KeyboardConfigPort();

    // Returns authority-file truth; a missing file is valid default truth,
    // a malformed file falls back to per-key defaults (the desktop does the
    // same). `error` is set only for storage-level failures.
    [[nodiscard]] virtual KeyboardConfig read(QString *error) const = 0;

    // Rejects out-of-range values (returns Failed) instead of clamping: the
    // UI clamps at edit time and silent rewrites hide route bugs.
    [[nodiscard]] virtual StoreResult
    write(const KeyboardConfig &config, QString *error) const = 0;
};

// Production adapter: KConfig file at `configFilePath` (kcminputrc under the
// composition root's config location) plus the ConfigChanged announcement
// over `bus` that makes a running KWin re-read it (ADR-0134).
class QtKeyboardConfigPort final : public KeyboardConfigPort {
public:
    QtKeyboardConfigPort(QString configFilePath, QDBusConnection bus);

    [[nodiscard]] KeyboardConfig read(QString *error) const override;
    [[nodiscard]] StoreResult
    write(const KeyboardConfig &config, QString *error) const override;

private:
    QString m_configFilePath;
    QDBusConnection m_bus;
};

} // namespace QindaQt::Apps::SettingsInput
