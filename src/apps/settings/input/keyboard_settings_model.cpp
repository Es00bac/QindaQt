// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_settings_model.h>

namespace QindaQt::Apps::SettingsInput {

KeyboardSettingsModel::KeyboardSettingsModel(const KeyboardConfigPort &port,
                                             QObject *parent)
    : QObject(parent), m_port(port) {}

void KeyboardSettingsModel::refresh() {
    QString error;
    const KeyboardConfig config = m_port.read(&error);
    m_available = error.isEmpty();
    m_config = config;
    Q_EMIT availableChanged();
    Q_EMIT keyRepeatChanged();
    Q_EMIT repeatDelayChanged();
    Q_EMIT repeatRateChanged();
    Q_EMIT numLockChanged();
}

void KeyboardSettingsModel::apply() {
    if (m_applying) {
        return;
    }
    m_applying = true;
    Q_EMIT applyingChanged();
    QString error;
    const StoreResult result = m_port.write(m_config, &error);
    m_applying = false;
    Q_EMIT applyingChanged();
    switch (result) {
    case StoreResult::Stored:
        m_statusText = tr("Keyboard settings applied");
        break;
    case StoreResult::StoredButReloadFailed:
        m_statusText = tr(
            "Saved. The running desktop could not be told to reload, so it "
            "applies on the next session.");
        break;
    case StoreResult::Failed:
        m_statusText = error.isEmpty()
                           ? tr("Keyboard settings could not be stored")
                           : error;
        break;
    }
    Q_EMIT statusTextChanged();
}

void KeyboardSettingsModel::setKeyRepeat(bool value) {
    if (value == m_config.keyRepeat) {
        return;
    }
    m_config.keyRepeat = value;
    Q_EMIT keyRepeatChanged();
}

void KeyboardSettingsModel::setRepeatDelayMs(int value) {
    const int clamped = qBound(100, value, 2000);
    if (clamped == m_config.repeatDelayMs) {
        return;
    }
    m_config.repeatDelayMs = clamped;
    Q_EMIT repeatDelayChanged();
}

void KeyboardSettingsModel::setRepeatRate(int value) {
    const int clamped = qBound(1, value, 100);
    if (clamped == m_config.repeatRate) {
        return;
    }
    m_config.repeatRate = clamped;
    Q_EMIT repeatRateChanged();
}

void KeyboardSettingsModel::setNumLockAtLogin(int value) {
    const int clamped = qBound(0, value, 2);
    if (clamped == m_config.numLockAtLogin) {
        return;
    }
    m_config.numLockAtLogin = clamped;
    Q_EMIT numLockChanged();
}

} // namespace QindaQt::Apps::SettingsInput
