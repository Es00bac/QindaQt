// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitor_facade.h"

#include "hardware_sampler.h"

#include <QCoreApplication>
#include <QProcess>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace QindaQt::SystemMonitor {

namespace {

constexpr const char *kDash = "—";

// Binary prefixes: every counter these format comes from procfs or sysfs in
// bytes, so decimal prefixes would misreport the number the kernel gave.
const char *const kUnits[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};

QString scaled(double value, const char *suffix, int precision) {
  int unit = 0;
  double amount = value;
  while (amount >= 1024.0 && unit < int(std::size(kUnits)) - 1) {
    amount /= 1024.0;
    ++unit;
  }
  // Whole bytes have no fractional part worth showing.
  const int digits = unit == 0 ? 0 : precision;
  return QStringLiteral("%1 %2%3")
      .arg(amount, 0, 'f', digits)
      .arg(QString::fromLatin1(kUnits[unit]), QString::fromLatin1(suffix));
}

} // namespace

MonitorFacade::MonitorFacade(QObject *parent) : QObject(parent) {
  m_timer.setInterval(2000);
  connect(&m_timer, &QTimer::timeout, this, &MonitorFacade::refreshHardware);

  m_watcher = new QFutureWatcher<QVariantMap>(this);
  connect(m_watcher, &QFutureWatcher<QVariantMap>::finished, this, [this] {
    m_inFlight = false;
    if (m_watcher->future().resultCount() > 0) {
      m_hardware = m_watcher->result();
      m_sampled = true;
      Q_EMIT hardwareChanged();
    }
  });

  refreshHardware();
  m_timer.start();
}

MonitorFacade::~MonitorFacade() {
  // The worker holds no reference to this object, but waiting keeps the
  // watcher from reporting into a half-destroyed QObject.
  m_watcher->waitForFinished();
}

int MonitorFacade::hardwareInterval() const { return m_timer.interval(); }

void MonitorFacade::setHardwareInterval(int milliseconds) {
  const int wanted = std::clamp(milliseconds, 500, 30000);
  if (m_timer.interval() != wanted) {
    m_timer.setInterval(wanted);
    Q_EMIT hardwareIntervalChanged();
  }
}

void MonitorFacade::setPaused(bool paused) {
  if (m_paused == paused) {
    return;
  }
  m_paused = paused;
  if (m_paused) {
    m_timer.stop();
  } else {
    m_timer.start();
    refreshHardware();
  }
  Q_EMIT pausedChanged();
}

void MonitorFacade::refreshHardware() {
  // AGENT-GUARD: never queue a second read behind a slow one. A wedged sysfs
  // node (a GPU resuming, a disconnected sensor) would otherwise pile up one
  // pending sample per tick until the thread pool is exhausted.
  if (m_inFlight) {
    return;
  }
  m_inFlight = true;
  m_watcher->setFuture(QtConcurrent::run([] {
    const HardwareSampler sampler;
    return sampler.sample();
  }));
}

bool MonitorFacade::openPanel(const QString &panelId) {
  // AGENT-NOTE: a separate process, not a second window in this one. Each
  // detached view then samples independently and, more importantly, a crash
  // or a wedged sysfs read in one cannot take the others down with it.
  return QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                 {QStringLiteral("--panel"), panelId});
}

bool MonitorFacade::known(const QVariant &value) {
  if (!value.isValid() || value.isNull()) {
    return false;
  }
  if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float) {
    return std::isfinite(value.toDouble());
  }
  return true;
}

QString MonitorFacade::unavailable() { return QString::fromUtf8(kDash); }

QString MonitorFacade::bytes(const QVariant &value, int precision) {
  if (!known(value)) {
    return unavailable();
  }
  return scaled(value.toDouble(), "", precision);
}

QString MonitorFacade::rate(const QVariant &value) {
  if (!known(value)) {
    return unavailable();
  }
  return scaled(value.toDouble(), "/s", 1);
}

QString MonitorFacade::percent(const QVariant &value, int precision) {
  if (!known(value)) {
    return unavailable();
  }
  return QStringLiteral("%1%").arg(value.toDouble(), 0, 'f', precision);
}

QString MonitorFacade::number(const QVariant &value, int precision) {
  if (!known(value)) {
    return unavailable();
  }
  return QString::number(value.toDouble(), 'f', precision);
}

QString MonitorFacade::duration(const QVariant &seconds) {
  if (!known(seconds)) {
    return unavailable();
  }
  const qint64 total = qint64(seconds.toDouble());
  const qint64 days = total / 86400;
  const qint64 hours = (total % 86400) / 3600;
  const qint64 minutes = (total % 3600) / 60;
  const qint64 rest = total % 60;
  if (days > 0) {
    return QStringLiteral("%1d %2:%3:%4")
        .arg(days)
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(rest, 2, 10, QLatin1Char('0'));
  }
  if (hours > 0) {
    return QStringLiteral("%1:%2:%3")
        .arg(hours)
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(rest, 2, 10, QLatin1Char('0'));
  }
  return QStringLiteral("%1:%2")
      .arg(minutes)
      .arg(rest, 2, 10, QLatin1Char('0'));
}

} // namespace QindaQt::SystemMonitor
