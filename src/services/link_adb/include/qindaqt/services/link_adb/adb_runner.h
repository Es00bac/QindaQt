// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QStringList>

#include <functional>

namespace QindaQt::Link {

// Everything that touches the adb executable goes through this interface so
// tests substitute a fake and never run adb against a real device.
//
// AGENT-CONTRACT: trackData() carries raw bytes of `adb track-devices`
// (4 hex digits of length, then "serial\tstate\n" lines). trackEnded() is
// emitted when that process exits for any reason. run() invokes `done`
// exactly once, on the owner's thread, with the adb exit code (-1 when adb
// cannot start or times out) and its stdout.
class AdbRunner : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void startTracking() = 0;
    virtual void stopTracking() = 0;
    virtual void run(const QStringList &args, std::function<void(int exitCode, const QByteArray &out)> done) = 0;

Q_SIGNALS:
    void trackData(const QByteArray &bytes);
    void trackEnded();
};

// Production runner: QProcess around an absolute adb path (Portage's
// /usr/bin/adb by default).
class QProcessAdbRunner final : public AdbRunner {
    Q_OBJECT
public:
    explicit QProcessAdbRunner(QString adbPath = QStringLiteral("/usr/bin/adb"), QObject *parent = nullptr);
    ~QProcessAdbRunner() override;
    void startTracking() override;
    void stopTracking() override;
    void run(const QStringList &args, std::function<void(int, const QByteArray &)> done) override;

private:
    QString m_adbPath;
    QProcess *m_tracker = nullptr;
};

} // namespace QindaQt::Link
