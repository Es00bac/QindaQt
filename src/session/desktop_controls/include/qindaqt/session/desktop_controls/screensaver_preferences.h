// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Session::DesktopControls {

// Idle screensaver preference. `saver` is the chosen program token; the
// reserved token "none" disables the feature. `minutes` is the idle delay
// before the saver starts, clamped to 1..maximumTimeoutMinutes().
//
// This is display decoration only: it never locks the session and never
// touches the separate screen-lock or display-off preferences.
struct ScreensaverPreferences final {
    QString saver = QStringLiteral("none");
    int minutes = 5;

    [[nodiscard]] static constexpr int maximumTimeoutMinutes() noexcept { return 240; }
    [[nodiscard]] static constexpr int defaultTimeoutMinutes() noexcept { return 5; }

    // The reserved off token, and every saver Settings may offer. An
    // unrecognized persisted token degrades to "none" rather than being
    // handed to QProcess as a program name.
    [[nodiscard]] static const QString &noneToken();
    [[nodiscard]] static const QStringList &knownSavers();

    [[nodiscard]] bool enabled() const noexcept;
    // The program to launch, or an empty string when disabled.
    [[nodiscard]] QString program() const;
    // The fixed command line for the chosen saver: every output, no telemetry.
    [[nodiscard]] QStringList arguments() const;

    // Maps the persisted Settings1 pair into a bounded preference.
    [[nodiscard]] static ScreensaverPreferences fromPersisted(const QString &saver,
                                                              qint64 persistedMinutes);

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
