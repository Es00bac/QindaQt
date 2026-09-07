// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>

#include <optional>

namespace QindaQt::Session::DesktopControls {

// Bounded idle display-off preference. `minutes` <= 0 means "never turn the
// display off"; positive values are clamped to 1..MaximumTimeoutMinutes.
struct IdleDisplayPreferences final {
    bool enabled = false;
    int minutes = 10;

    [[nodiscard]] static constexpr int maximumTimeoutMinutes() noexcept { return 240; }
    [[nodiscard]] static constexpr int defaultTimeoutMinutes() noexcept { return 10; }

    // Maps the persisted signed minute value (-1 = never, 1..240 = timeout).
    [[nodiscard]] static IdleDisplayPreferences fromMinutes(qint64 persistedMinutes);
    [[nodiscard]] qint64 toPersistedMinutes() const noexcept;

    friend bool operator==(const IdleDisplayPreferences &,
                           const IdleDisplayPreferences &) = default;
};

// Seam over preference truth. Production reads the purpose-scoped Settings1
// key `power.idleDisplayOffMinutes`; tests inject a fake.
class IdlePreferencesProvider : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~IdlePreferencesProvider() override = default;

    IdlePreferencesProvider(const IdlePreferencesProvider &) = delete;
    IdlePreferencesProvider &operator=(const IdlePreferencesProvider &) = delete;

    [[nodiscard]] virtual IdleDisplayPreferences currentPreferences() const = 0;
    virtual void refresh() = 0;

Q_SIGNALS:
    void preferencesChanged(QindaQt::Session::DesktopControls::IdleDisplayPreferences preferences);
};

} // namespace QindaQt::Session::DesktopControls

Q_DECLARE_METATYPE(QindaQt::Session::DesktopControls::IdleDisplayPreferences)
