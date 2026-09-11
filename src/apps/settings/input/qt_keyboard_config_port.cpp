// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_config_port.h>

#include <qindaqt/apps/settings_input/config_change_announcement.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QDBusConnectionInterface>
#include <QLoggingCategory>

namespace QindaQt::Apps::SettingsInput {
namespace {

Q_LOGGING_CATEGORY(lcKeyboardConfigPort, "qindaqt.settings.input.keyboardconfig",
                   QtInfoMsg)

constexpr auto KWinService = "org.kde.KWin";

constexpr int MinimumRepeatDelayMs = 100;
constexpr int MaximumRepeatDelayMs = 2000;
constexpr int MinimumRepeatRate = 1;
constexpr int MaximumRepeatRate = 100;

// AGENT-CONTRACT: Key names are the ones the pinned KWin reads from
// kcminputrc [Keyboard] (present in libkwin, ADR-0134). Renaming them
// silently breaks the desktop's repeat/NumLock behavior, not just this route.
constexpr auto KeyRepeatKey = "KeyRepeat";
constexpr auto RepeatDelayKey = "RepeatDelay";
constexpr auto RepeatRateKey = "RepeatRate";
constexpr auto NumLockKey = "NumLock";

bool desktopAuthorityPresent(const QDBusConnection &bus) {
    return bus.isConnected() && bus.interface() != nullptr &&
           bus.interface()->isServiceRegistered(QLatin1String(KWinService));
}

int clampDelay(int value) {
    return qBound(MinimumRepeatDelayMs, value, MaximumRepeatDelayMs);
}

int clampRate(int value) {
    return qBound(MinimumRepeatRate, value, MaximumRepeatRate);
}

} // namespace

bool isValidKeyboardConfig(const KeyboardConfig &config) noexcept {
    return config.repeatDelayMs >= MinimumRepeatDelayMs &&
           config.repeatDelayMs <= MaximumRepeatDelayMs &&
           config.repeatRate >= MinimumRepeatRate &&
           config.repeatRate <= MaximumRepeatRate &&
           config.numLockAtLogin >= 0 && config.numLockAtLogin <= 2;
}

KeyboardConfigPort::~KeyboardConfigPort() = default;

QtKeyboardConfigPort::QtKeyboardConfigPort(QString configFilePath,
                                           QDBusConnection bus)
    : m_configFilePath(std::move(configFilePath)), m_bus(std::move(bus)) {}

KeyboardConfig QtKeyboardConfigPort::read(QString *error) const {
    const KConfigGroup group(
        KSharedConfig::openConfig(m_configFilePath, KConfig::SimpleConfig),
        QStringLiteral("Keyboard"));
    KeyboardConfig config;
    config.keyRepeat = group.readEntry(KeyRepeatKey, true);
    config.repeatDelayMs =
        clampDelay(group.readEntry(RepeatDelayKey, config.repeatDelayMs));
    config.repeatRate = clampRate(group.readEntry(RepeatRateKey, config.repeatRate));
    const int numLock = group.readEntry(NumLockKey, config.numLockAtLogin);
    config.numLockAtLogin = qBound(0, numLock, 2);
    Q_UNUSED(error);
    return config;
}

StoreResult QtKeyboardConfigPort::write(const KeyboardConfig &config,
                                        QString *error) const {
    if (!isValidKeyboardConfig(config)) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Refusing to store keyboard repeat values outside the "
                "supported ranges");
        }
        return StoreResult::Failed;
    }
    // AGENT-NOTE: SimpleConfig avoids merging this route's writes into the
    // cascade of the user's real kcminputrc when a test relocates the path;
    // the desktop reads exactly this file.
    const KSharedConfigPtr configFile = KSharedConfig::openConfig(
        m_configFilePath, KConfig::SimpleConfig);
    KConfigGroup group(configFile, QStringLiteral("Keyboard"));
    group.writeEntry(KeyRepeatKey, config.keyRepeat);
    group.writeEntry(RepeatDelayKey, config.repeatDelayMs);
    group.writeEntry(RepeatRateKey, config.repeatRate);
    group.writeEntry(NumLockKey, config.numLockAtLogin);
    if (!group.sync()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write %1")
                         .arg(m_configFilePath);
        }
        return StoreResult::Failed;
    }
    QHash<QString, QByteArrayList> changes;
    changes.insert(QStringLiteral("Keyboard"),
                   QByteArrayList{QByteArray(KeyRepeatKey),
                                  QByteArray(RepeatDelayKey),
                                  QByteArray(RepeatRateKey),
                                  QByteArray(NumLockKey)});
    const bool announced =
        announceConfigChange(m_bus, m_configFilePath, changes);
    if (!announced || !desktopAuthorityPresent(m_bus)) {
        // AGENT-GUARD: File truth and live truth are reported separately.
        // The running KWin applies these values only after the announcement
        // reaches it; collapsing this branch into success would tell the
        // user the desktop changed behavior when it still runs old values.
        qCInfo(lcKeyboardConfigPort,
               "keyboard config stored, but no running desktop was told; "
               "the change applies on the next session");
        return StoreResult::StoredButReloadFailed;
    }
    return StoreResult::Stored;
}

} // namespace QindaQt::Apps::SettingsInput
