// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "process_runner.h"

#include <QtCore/QObject>

namespace QindaQt::LinkApp {

// Opens other desktop programs the app points to. Today: Settings at its Macros destination.
class Launcher final : public QObject {
    Q_OBJECT
public:
    explicit Launcher(ProcessRunner &runner, QObject *parent = nullptr) : QObject(parent), m_runner(runner) {}
    // `qindaqt-settings --page input --destination macros` (docs/wiki/apps/input-settings.md).
    Q_INVOKABLE bool openMacroSettings()
    {
        return m_runner.startDetached(QStringLiteral("qindaqt-settings"),
                                      {QStringLiteral("--page"), QStringLiteral("input"), QStringLiteral("--destination"),
                                       QStringLiteral("macros")});
    }

private:
    ProcessRunner &m_runner;
};

} // namespace QindaQt::LinkApp
