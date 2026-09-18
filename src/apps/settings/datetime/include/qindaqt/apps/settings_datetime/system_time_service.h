// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace QindaQt::Apps::SettingsDateTime {

// Everything the route shows about the machine's clock and region, as one
// value. `available` false means the platform service could not be reached at
// all, which the page states rather than showing empty fields.
struct SystemTimeSnapshot final {
  QString timeZone;
  // Every zone the platform offers, for the picker. Empty when the service
  // could not list them; the page then shows the current zone read-only
  // rather than an empty picker that looks broken.
  QStringList timeZones;
  bool automaticTime = false;
  // False on a machine with no time-synchronization service, where the
  // automatic-time control must be unavailable rather than merely off.
  bool automaticTimeSupported = false;
  bool synchronized = false;
  // The system locale, e.g. "en_US.UTF-8", or empty when unknown.
  QString locale;
  bool available = false;

  [[nodiscard]] bool operator==(const SystemTimeSnapshot &) const = default;
};

// AGENT-CONTRACT: the one seam through which the Date & time route reaches
// the platform's clock and locale services (ADR-0200). An implementation
// reads and writes exactly these four things and nothing else: it never sets
// the wall-clock time directly, never touches the RTC mode, never changes the
// console or X11 keymap, and never caches a result past the next snapshot.
//
// AGENT-GUARD: changing a system-wide clock or locale setting is privileged.
// An implementation asks the platform *interactively*, so the user sees their
// own authentication agent and a refusal comes back as a message -- it must
// never try to acquire privilege itself, and must never present a control as
// having succeeded before the platform says it did.
class SystemTimeService : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~SystemTimeService() override = default;

  // Asks for a fresh snapshot. The answer arrives through snapshotChanged().
  virtual void refresh() = 0;
  virtual void setTimeZone(const QString &timeZone) = 0;
  virtual void setAutomaticTime(bool enabled) = 0;

signals:
  void snapshotChanged(const SystemTimeSnapshot &snapshot);
  // A refused or failed request, already bounded and safe to show. The
  // snapshot is unchanged, so the page still shows what is really set.
  void requestFailed(const QString &diagnostic);
};

using SystemTimeServicePtr = std::unique_ptr<SystemTimeService>;

// AGENT-CONTRACT: the first-day-of-week preference, which is a QindaQt
// setting rather than a system one -- `services.calendarWeekStart`, already
// read by the Calendar's month grid. Kept behind its own seam so the model is
// testable without a session bus.
class WeekStartPreference : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~WeekStartPreference() override = default;

  // "locale", "monday" or "sunday".
  [[nodiscard]] virtual QString weekStart() const = 0;
  [[nodiscard]] virtual bool editable() const = 0;
  virtual void setWeekStart(const QString &weekStart) = 0;

signals:
  void weekStartChanged();
};

using WeekStartPreferencePtr = std::unique_ptr<WeekStartPreference>;

} // namespace QindaQt::Apps::SettingsDateTime
