// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_adb/adb_runner.h>

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <functional>

namespace QindaQt::Link {

// The USB grants QindaLink needs on a sideloaded Android 17 install
// (docs/wiki/architecture/qindalink.md, "Phone setup over ADB").
//
// Ownership: borrows the runner. Threading: one thread (the runner's). Error behavior: a failed
// step is recorded in the report and the remaining steps still run; nothing throws.
// AGENT-GUARD: only the commands listed in this class are issued, only for the QindaLink package.
// Output of the commands is never kept or reported, only exit codes, so no phone state leaks.
class PhoneSetup final : public QObject {
    Q_OBJECT
public:
    using Done = std::function<void(const QJsonObject &report)>;

    explicit PhoneSetup(AdbRunner &runner, QString package = QStringLiteral("org.qindaqt.link"),
                        QObject *parent = nullptr);
    // True for a serial shaped like an adb serial; everything else is refused before adb runs.
    [[nodiscard]] static bool validSerial(const QString &serial);
    // Runs every grant. Report: {"serial","ok":bool,"steps":[{"id","ok","code"}]}.
    void run(const QString &serial, Done done);
    // Reads the state without changing it. Report: {"serial","ok":bool,"steps":[{"id","ok","code"}]}
    // where "ok" means "already granted". Code is the adb exit code of the read.
    void status(const QString &serial, Done done);

private:
    struct Step {
        QString id;
        QStringList args;       // after `-s <serial> shell`
        QString expect;         // status only: substring that proves the grant
        bool wholeOutput = false;
    };
    [[nodiscard]] QList<Step> grantSteps() const;
    void runSteps(const QString &serial, QList<Step> steps, bool verify, Done done);

    AdbRunner &m_runner;
    QString m_package;
};

} // namespace QindaQt::Link
