// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/apps/settings_datetime/system_time_service.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVariantMap>

namespace QindaQt::Apps::SettingsDateTime {

// Production SystemTimeService (ADR-0200) on the platform's own
// `org.freedesktop.timedate1` and `org.freedesktop.locale1`, both already
// present on every systemd machine. The bus is injected -- production passes
// the system bus, where both live -- so a test can hand it a private one.
//
// AGENT-GUARD: every write goes out with systemd's `interactive` flag set, so
// the user's own authentication agent asks and a refusal comes back as a
// D-Bus error this class reports. It never acquires privilege, never retries
// a refused request, and never writes anything the route did not ask for --
// in particular it does not call SetTime, SetLocalRTC, SetVConsoleKeyboard or
// SetX11Keyboard, which are deliberately outside the seam.
//
// Not final: tst_datetime_settings_model.cpp uses the seam's own fake; this
// class's D-Bus edges are the overridable ones below so a row can answer them
// with canned replies and never touch a bus.
class SystemdTimeService : public SystemTimeService {
  Q_OBJECT

public:
  explicit SystemdTimeService(QDBusConnection bus, QObject *parent = nullptr);

  void refresh() override;
  void setTimeZone(const QString &timeZone) override;
  void setAutomaticTime(bool enabled) override;

protected:
  [[nodiscard]] virtual bool busAvailable() const;
  [[nodiscard]] virtual QDBusPendingCall timePropertiesCall() const;
  [[nodiscard]] virtual QDBusPendingCall localePropertiesCall() const;
  [[nodiscard]] virtual QDBusPendingCall timeZoneCatalogueCall() const;
  [[nodiscard]] virtual QDBusPendingCall setTimeZoneCall(const QString &timeZone) const;
  [[nodiscard]] virtual QDBusPendingCall setAutomaticTimeCall(bool enabled) const;

  // Applies one completed properties reply. Reachable by a test without a bus.
  void applyTimeProperties(const QVariantMap &properties);
  void applyLocaleProperties(const QVariantMap &properties);
  void applyTimeZoneCatalogue(const QStringList &zones);
  void publish();

private slots:
  // AGENT-NOTE: PropertiesChanged lands here rather than on refresh()
  // directly, because refresh() is a plain virtual of the seam and
  // QDBusConnection::connect() needs a real slot.
  void onPropertiesChanged();

private:
  void watch(const QDBusPendingCall &call, void (SystemdTimeService::*handler)(
                                               const QDBusMessage &));
  void onTimeProperties(const QDBusMessage &reply);
  void onLocaleProperties(const QDBusMessage &reply);
  void onTimeZoneCatalogue(const QDBusMessage &reply);
  void onWriteFinished(const QDBusMessage &reply);
  void subscribe();

  QDBusConnection m_bus;
  SystemTimeSnapshot m_snapshot;
  bool m_subscribed = false;
  bool m_catalogueRequested = false;
};

} // namespace QindaQt::Apps::SettingsDateTime
