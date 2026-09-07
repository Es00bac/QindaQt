// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QtTypes>

namespace QindaQt::SessionSupervisor {

// One optional supervised session child with an independent one-restart
// budget. `start` is given the already-resolved program and silently declines
// an empty or non-executable one: an absent optional helper must never make
// the session unusable. The child never participates in readiness and cannot
// end the session; `stopRequested(role)` fires only when a live child is
// about to be terminated.
class OptionalSessionChild final : public QObject {
    Q_OBJECT

public:
    OptionalSessionChild(QString role, QStringList arguments, QObject *parent = nullptr);
    ~OptionalSessionChild() override;

    OptionalSessionChild(const OptionalSessionChild &) = delete;
    OptionalSessionChild &operator=(const OptionalSessionChild &) = delete;

    void start(const QString &resolvedProgram);
    void stop() noexcept;
    void setRestartLimit(int limit) noexcept { m_restartLimit = limit; }

    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] qint64 processId() const noexcept;
    [[nodiscard]] int restartCount() const noexcept;
    [[nodiscard]] const QString &role() const noexcept { return m_role; }

Q_SIGNALS:
    void restarted(const QString &role, qint64 previousProcessId,
                   qint64 processId);
    void stopRequested(const QString &role);

private:
    void ended();

    const QString m_role;
    const QStringList m_arguments;
    QProcess m_process;
    qint64 m_processId = 0;
    qint64 m_previousProcessId = 0;
    int m_restartCount = 0;
    int m_restartLimit = 1;
    bool m_stopping = false;
};

} // namespace QindaQt::SessionSupervisor
