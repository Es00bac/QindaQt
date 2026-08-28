// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionenvironment.h"

#include <QDir>

namespace QindaQt::Session {
namespace {

void setValue(const char *name, const QString &value)
{
    if (!value.isEmpty()) {
        qputenv(name, value.toUtf8());
    }
}

} // namespace

void SessionEnvironment::apply(const SessionOptions &options)
{
    qputenv("XDG_CURRENT_DESKTOP", "QindaQt");
    qputenv("XDG_SESSION_DESKTOP", "qindaqt");
    qputenv("XDG_SESSION_TYPE", "wayland");
    // AGENT-GUARD: External compositor mutation is a development-harness
    // capability, never an inherited production-session default. The KWin
    // endpoint additionally verifies both markers before enabling it. Output
    // hotplug has a third marker because KWin's generic virtual-output ABI is
    // safe only on the explicitly selected virtual backend.
    qunsetenv("QINDAQT_TEST_SCENARIO");
    qunsetenv("QINDAQT_DEVELOPMENT_CONTROL");
    qunsetenv("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND");
    if (!options.testScenario.isEmpty()) {
        setValue("QINDAQT_TEST_SCENARIO", options.testScenario);
        qputenv("QINDAQT_DEVELOPMENT_CONTROL", "1");
        if (options.backend == Backend::Virtual) {
            qputenv("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND", "virtual");
        }
    }

    if (!options.pluginRoot.isEmpty()) {
        const auto separator = QDir::listSeparator().toLatin1();
        QByteArray pluginPath = options.pluginRoot.toUtf8();
        const QByteArray inherited = qgetenv("QT_PLUGIN_PATH");
        if (!inherited.isEmpty()) {
            pluginPath.append(separator);
            pluginPath.append(inherited);
        }
        // AGENT-GUARD: Prepend instead of replacing. QindaQt's KWin plugin must
        // be discoverable without hiding the platform and image plugins KWin needs.
        qputenv("QT_PLUGIN_PATH", pluginPath);
    }
}

} // namespace QindaQt::Session
