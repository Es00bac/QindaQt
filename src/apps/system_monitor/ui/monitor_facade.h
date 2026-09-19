// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace QindaQt::SystemMonitor {

/**
 * The interface's service object: hardware telemetry on its own cadence, and
 * the number formatting every panel shares.
 *
 * Hardware is sampled separately from MonitorEngine because it is a different
 * kind of reading. GPU and sensor files are slower than procfs, some of them
 * wake a device, and none of them change fast enough to be worth reading at
 * the process interval. Sampling runs on a worker thread; a slow or wedged
 * sysfs read therefore cannot stall the frame.
 *
 * Formatting lives here rather than in QML so that every panel, the process
 * table and any future applet agree on what "2.4 GiB" and "3.3 KiB/s" mean,
 * and on the one rule that matters: an unavailable reading formats as a dash,
 * never as a zero.
 */
class MonitorFacade final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantMap hardware READ hardware NOTIFY hardwareChanged)
  Q_PROPERTY(bool hardwareAvailable READ hardwareAvailable NOTIFY hardwareChanged)
  Q_PROPERTY(int hardwareInterval READ hardwareInterval WRITE
                 setHardwareInterval NOTIFY hardwareIntervalChanged)
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

public:
  explicit MonitorFacade(QObject *parent = nullptr);
  ~MonitorFacade() override;

  [[nodiscard]] QVariantMap hardware() const { return m_hardware; }
  [[nodiscard]] bool hardwareAvailable() const { return m_sampled; }
  [[nodiscard]] int hardwareInterval() const;
  void setHardwareInterval(int milliseconds);
  [[nodiscard]] bool paused() const { return m_paused; }
  void setPaused(bool paused);

  Q_INVOKABLE void refreshHardware();

  /// Starts another instance showing one panel (ADR-0108's detachable
  /// views). Returns false when the process could not be started.
  Q_INVOKABLE bool openPanel(const QString &panelId);

  /// "2.4 GiB". Binary units, because every counter behind them is binary.
  Q_INVOKABLE [[nodiscard]] static QString bytes(const QVariant &value,
                                                 int precision = 1);
  /// "3.3 KiB/s", or a dash when the rate has no baseline yet.
  Q_INVOKABLE [[nodiscard]] static QString rate(const QVariant &value);
  /// "9d 23:19:04" / "23:19:04" / "19:04".
  Q_INVOKABLE [[nodiscard]] static QString duration(const QVariant &seconds);
  /// "94.2%", or a dash when unavailable.
  Q_INVOKABLE [[nodiscard]] static QString percent(const QVariant &value,
                                                   int precision = 1);
  /// A bare number with fixed decimals, or a dash. For table cells.
  Q_INVOKABLE [[nodiscard]] static QString number(const QVariant &value,
                                                  int precision = 1);
  /// The dash every unavailable reading shows. One spelling, one place.
  Q_INVOKABLE [[nodiscard]] static QString unavailable();
  /// True when a reading is present; false for null, invalid and NaN.
  Q_INVOKABLE [[nodiscard]] static bool known(const QVariant &value);

Q_SIGNALS:
  void hardwareChanged();
  void hardwareIntervalChanged();
  void pausedChanged();

private:
  QTimer m_timer;
  QFutureWatcher<QVariantMap> *m_watcher = nullptr;
  QVariantMap m_hardware;
  bool m_sampled = false;
  bool m_paused = false;
  bool m_inFlight = false;
};

} // namespace QindaQt::SystemMonitor
