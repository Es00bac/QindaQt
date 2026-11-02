// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_adb/adb_runner.h>

#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include <memory>

namespace QindaQt::Link {

QProcessAdbRunner::QProcessAdbRunner(QString adbPath, QObject *parent)
    : AdbRunner(parent), m_adbPath(std::move(adbPath))
{
}

QProcessAdbRunner::~QProcessAdbRunner() { stopTracking(); }

void QProcessAdbRunner::startTracking()
{
    if (m_tracker != nullptr) {
        return;
    }
    m_tracker = new QProcess(this);
    connect(m_tracker, &QProcess::readyReadStandardOutput, this,
            [this] { Q_EMIT trackData(m_tracker->readAllStandardOutput()); });
    connect(m_tracker, &QProcess::finished, this, [this] {
        m_tracker->deleteLater();
        m_tracker = nullptr;
        Q_EMIT trackEnded();
    });
    connect(m_tracker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_tracker->deleteLater();
            m_tracker = nullptr;
            Q_EMIT trackEnded();
        }
    });
    m_tracker->start(m_adbPath, {QStringLiteral("track-devices")});
}

void QProcessAdbRunner::stopTracking()
{
    if (m_tracker == nullptr) {
        return;
    }
    QProcess *p = m_tracker;  // our own child; killed by handle, never by name
    m_tracker = nullptr;
    p->disconnect(this);
    p->kill();
    p->waitForFinished(1000);
    delete p;
}

void QProcessAdbRunner::run(const QStringList &args, std::function<void(int, const QByteArray &)> done)
{
    auto *p = new QProcess(this);
    auto finished = std::make_shared<bool>(false);
    auto finish = [p, finished, done = std::move(done)](int code) {
        if (*finished) return;
        *finished = true;
        const QByteArray out = p->readAllStandardOutput();
        p->deleteLater();
        done(code, out);
    };
    connect(p, &QProcess::finished, p, [finish](int code, QProcess::ExitStatus) { finish(code); });
    connect(p, &QProcess::errorOccurred, p, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) finish(-1);
    });
    QTimer::singleShot(15000, p, [p, finish] {
        if (p->state() != QProcess::NotRunning) {
            p->kill();
        }
        finish(-1);
    });
    p->start(m_adbPath, args);
}

} // namespace QindaQt::Link
