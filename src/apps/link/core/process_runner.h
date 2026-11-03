// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <functional>

namespace QindaQt::LinkApp {

// The only way the app starts other programs (the phone-app mirror, Settings), so tests and
// `--fake` renders substitute it and never start a real window.
//
// AGENT-GUARD: arguments are always separate argv elements, never a shell string.
class ProcessRunner : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    // Starts a program that outlives the app. Returns false when it could not be started.
    virtual bool startDetached(const QString &program, const QStringList &arguments) = 0;
    // Runs a program to completion and reports its exit code (-1 if it could not start or timed
    // out) and stdout. `done` runs on the GUI thread.
    virtual void capture(const QString &program, const QStringList &arguments,
                         std::function<void(int exitCode, const QByteArray &out)> done) = 0;
};

class QProcessRunner final : public ProcessRunner {
    Q_OBJECT
public:
    using ProcessRunner::ProcessRunner;
    bool startDetached(const QString &program, const QStringList &arguments) override;
    void capture(const QString &program, const QStringList &arguments,
                 std::function<void(int, const QByteArray &)> done) override;
};

// Test and `--fake` runner: records detached starts and answers captures from `captureOutput`.
class FakeProcessRunner final : public ProcessRunner {
    Q_OBJECT
public:
    using ProcessRunner::ProcessRunner;
    bool startDetached(const QString &program, const QStringList &arguments) override;
    void capture(const QString &program, const QStringList &arguments,
                 std::function<void(int, const QByteArray &)> done) override;

    QList<QStringList> started;        // program followed by its arguments
    QList<QStringList> captured;
    QByteArray captureOutput;
    int captureExit = 0;
    bool startSucceeds = true;
};

} // namespace QindaQt::LinkApp
