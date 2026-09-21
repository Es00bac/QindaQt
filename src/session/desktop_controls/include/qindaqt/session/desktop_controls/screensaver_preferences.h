// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Session::DesktopControls {

class ScreensaverCatalog;

// Idle screensaver preference. `saver` is the chosen token: the reserved
// token "none" disables the feature, the reserved token "blank" shows a plain
// dark screen once the session locks without running any program while
// unlocked, and any other value names a discovered installed saver (the
// program name from its desktop entry; see ScreensaverCatalog). `minutes` is
// the idle delay before the saver starts, clamped to
// 1..maximumTimeoutMinutes().
//
// This is display decoration only: it never locks the session and never
// touches the separate screen-lock or display-off preferences.
struct ScreensaverPreferences final {
    QString saver = QStringLiteral("none");
    int minutes = 5;

    [[nodiscard]] static constexpr int maximumTimeoutMinutes() noexcept { return 240; }
    [[nodiscard]] static constexpr int defaultTimeoutMinutes() noexcept { return 5; }

    // The two reserved tokens. Everything else comes from discovery, so no
    // list of savers lives here any more (ADR-0226).
    [[nodiscard]] static const QString &noneToken();
    [[nodiscard]] static const QString &blankToken();

    // True when a saver program should run while idle: any resolved token
    // other than "none" or "blank". "blank" is deliberately not enabled --
    // it is a lock-screen appearance, not a process to start.
    [[nodiscard]] bool enabled() const noexcept;

    // Maps the persisted Settings1 pair into a bounded preference. A token
    // that is neither reserved nor present in the catalog degrades to "none"
    // rather than being handed to QProcess as a program name: persistence can
    // never supply a program name of its own.
    [[nodiscard]] static ScreensaverPreferences
    fromPersisted(const QString &saver, qint64 persistedMinutes,
                  const ScreensaverCatalog &catalog);

    friend bool operator==(const ScreensaverPreferences &,
                           const ScreensaverPreferences &) = default;
};

// Seam over preference truth. Production reads the purpose-scoped Settings1
// keys `power.screensaver` and `power.screensaverMinutes`; tests inject a fake.
class ScreensaverPreferencesProvider : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~ScreensaverPreferencesProvider() override = default;

    ScreensaverPreferencesProvider(const ScreensaverPreferencesProvider &) = delete;
    ScreensaverPreferencesProvider &operator=(const ScreensaverPreferencesProvider &) = delete;

    [[nodiscard]] virtual ScreensaverPreferences currentPreferences() const = 0;
    virtual void refresh() = 0;

Q_SIGNALS:
    void preferencesChanged(
        QindaQt::Session::DesktopControls::ScreensaverPreferences preferences);
};

} // namespace QindaQt::Session::DesktopControls

Q_DECLARE_METATYPE(QindaQt::Session::DesktopControls::ScreensaverPreferences)
