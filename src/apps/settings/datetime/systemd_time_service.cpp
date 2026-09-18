// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemd_time_service.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QStringList>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Apps::SettingsDateTime {
namespace {

constexpr auto timedateService = "org.freedesktop.timedate1";
constexpr auto timedatePath = "/org/freedesktop/timedate1";
constexpr auto timedateInterface = "org.freedesktop.timedate1";
constexpr auto localeService = "org.freedesktop.locale1";
constexpr auto localePath = "/org/freedesktop/locale1";
constexpr auto localeInterface = "org.freedesktop.locale1";
constexpr auto propertiesInterface = "org.freedesktop.DBus.Properties";
// AGENT-GUARD: systemd's `interactive` flag. True is what lets the user's own
// authentication agent ask; false would make every write fail outright for a
// non-root session.
constexpr bool interactive = true;
// A hostile or unusual catalogue cannot grow the picker without bound.
constexpr int maximumTimeZones = 2000;

[[nodiscard]] QDBusMessage propertiesCall(const char *service, const char *path,
                                          const char *interfaceName) {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(service), QString::fromLatin1(path),
      QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
  call.setArguments({QString::fromLatin1(interfaceName)});
  return call;
}

// locale1 publishes assignments, e.g. {"LANG=en_US.UTF-8"}. LANG is the one
// the route reports; LC_ALL wins over it when present.
[[nodiscard]] QString localeFromAssignments(const QStringList &assignments) {
  QString language;
  for (const QString &assignment : assignments) {
    const qsizetype separator = assignment.indexOf(QLatin1Char('='));
    if (separator <= 0) {
      continue;
    }
    const QString name = assignment.left(separator);
    const QString value = assignment.mid(separator + 1);
    if (name == QLatin1String("LC_ALL")) {
      return value;
    }
    if (name == QLatin1String("LANG")) {
      language = value;
    }
  }
  return language;
}

} // namespace

SystemdTimeService::SystemdTimeService(QDBusConnection bus, QObject *parent)
    : SystemTimeService(parent), m_bus(std::move(bus)) {}

bool SystemdTimeService::busAvailable() const { return m_bus.isConnected(); }

QDBusPendingCall SystemdTimeService::timePropertiesCall() const {
  return m_bus.asyncCall(
      propertiesCall(timedateService, timedatePath, timedateInterface));
}

QDBusPendingCall SystemdTimeService::localePropertiesCall() const {
  return m_bus.asyncCall(
      propertiesCall(localeService, localePath, localeInterface));
}

QDBusPendingCall SystemdTimeService::timeZoneCatalogueCall() const {
  return m_bus.asyncCall(QDBusMessage::createMethodCall(
      QString::fromLatin1(timedateService), QString::fromLatin1(timedatePath),
      QString::fromLatin1(timedateInterface), QStringLiteral("ListTimezones")));
}

QDBusPendingCall SystemdTimeService::setTimeZoneCall(const QString &timeZone) const {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(timedateService), QString::fromLatin1(timedatePath),
      QString::fromLatin1(timedateInterface), QStringLiteral("SetTimezone"));
  call.setArguments({timeZone, interactive});
  return m_bus.asyncCall(call);
}

QDBusPendingCall SystemdTimeService::setAutomaticTimeCall(const bool enabled) const {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(timedateService), QString::fromLatin1(timedatePath),
      QString::fromLatin1(timedateInterface), QStringLiteral("SetNTP"));
  call.setArguments({enabled, interactive});
  return m_bus.asyncCall(call);
}

void SystemdTimeService::watch(const QDBusPendingCall &call,
                               void (SystemdTimeService::*handler)(
                                   const QDBusMessage &)) {
  auto *watcher = new QDBusPendingCallWatcher(call, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, handler](QDBusPendingCallWatcher *finished) {
            finished->deleteLater();
            (this->*handler)(finished->reply());
          });
}

void SystemdTimeService::subscribe() {
  if (m_subscribed) {
    return;
  }
  m_subscribed = true;
  // timedate1 and locale1 both emit PropertiesChanged, so a change made
  // elsewhere (timedatectl, another settings app) reaches this page too.
  m_bus.connect(QString::fromLatin1(timedateService),
                QString::fromLatin1(timedatePath),
                QString::fromLatin1(propertiesInterface),
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged()));
  m_bus.connect(QString::fromLatin1(localeService), QString::fromLatin1(localePath),
                QString::fromLatin1(propertiesInterface),
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged()));
}

void SystemdTimeService::onPropertiesChanged() { refresh(); }

void SystemdTimeService::refresh() {
  if (!busAvailable()) {
    m_snapshot = {};
    publish();
    return;
  }
  subscribe();
  watch(timePropertiesCall(), &SystemdTimeService::onTimeProperties);
  watch(localePropertiesCall(), &SystemdTimeService::onLocaleProperties);
  if (!m_catalogueRequested) {
    m_catalogueRequested = true;
    watch(timeZoneCatalogueCall(), &SystemdTimeService::onTimeZoneCatalogue);
  }
}

void SystemdTimeService::applyTimeProperties(const QVariantMap &properties) {
  m_snapshot.available = true;
  m_snapshot.timeZone = properties.value(QStringLiteral("Timezone")).toString();
  m_snapshot.automaticTime = properties.value(QStringLiteral("NTP")).toBool();
  m_snapshot.automaticTimeSupported =
      properties.value(QStringLiteral("CanNTP")).toBool();
  m_snapshot.synchronized =
      properties.value(QStringLiteral("NTPSynchronized")).toBool();
}

void SystemdTimeService::applyLocaleProperties(const QVariantMap &properties) {
  m_snapshot.locale =
      localeFromAssignments(properties.value(QStringLiteral("Locale")).toStringList());
}

void SystemdTimeService::applyTimeZoneCatalogue(const QStringList &zones) {
  m_snapshot.timeZones = zones.size() > maximumTimeZones
                             ? zones.first(maximumTimeZones)
                             : zones;
}

void SystemdTimeService::publish() { Q_EMIT snapshotChanged(m_snapshot); }

void SystemdTimeService::onTimeProperties(const QDBusMessage &reply) {
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
    // The clock service is the one this route cannot do without, so its
    // absence makes the whole page unavailable rather than half-filled.
    m_snapshot = {};
    publish();
    return;
  }
  applyTimeProperties(qdbus_cast<QVariantMap>(reply.arguments().constFirst()));
  publish();
}

void SystemdTimeService::onLocaleProperties(const QDBusMessage &reply) {
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
    // A missing locale service leaves the locale unknown; the clock half of
    // the page still works.
    m_snapshot.locale.clear();
    publish();
    return;
  }
  applyLocaleProperties(qdbus_cast<QVariantMap>(reply.arguments().constFirst()));
  publish();
}

void SystemdTimeService::onTimeZoneCatalogue(const QDBusMessage &reply) {
  if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
    // No catalogue means the picker stays read-only; the current zone is
    // still shown.
    m_snapshot.timeZones.clear();
    publish();
    return;
  }
  applyTimeZoneCatalogue(reply.arguments().constFirst().toStringList());
  publish();
}

void SystemdTimeService::onWriteFinished(const QDBusMessage &reply) {
  if (reply.type() != QDBusMessage::ReplyMessage) {
    Q_EMIT requestFailed(reply.errorMessage().isEmpty()
                             ? tr("The system refused the change.")
                             : reply.errorMessage());
    return;
  }
  // Read back rather than trusting the write: the page must show what is set,
  // not what was asked for.
  refresh();
}

void SystemdTimeService::setTimeZone(const QString &timeZone) {
  if (!busAvailable()) {
    Q_EMIT requestFailed(tr("The system clock service is unavailable."));
    return;
  }
  watch(setTimeZoneCall(timeZone), &SystemdTimeService::onWriteFinished);
}

void SystemdTimeService::setAutomaticTime(const bool enabled) {
  if (!busAvailable()) {
    Q_EMIT requestFailed(tr("The system clock service is unavailable."));
    return;
  }
  watch(setAutomaticTimeCall(enabled), &SystemdTimeService::onWriteFinished);
}

} // namespace QindaQt::Apps::SettingsDateTime
