// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionenvironment.h"

#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Session {
namespace {

void setValue(const char *name, const QString &value)
{
    if (!value.isEmpty()) {
        qputenv(name, value.toUtf8());
    }
}

// AGENT-CONTRACT: the input-method variables have to be in the environment of
// every application the session starts, which is here and nowhere later.
// QindaQt's voice input (docs/wiki/architecture/voice-input.md) delivers text
// through the focused application's input context first, and a Qt or GTK
// application only has an IBus input context when these are set. Without them
// dictation silently falls back to less reliable routes.
//
// AGENT-GUARD: never override a value the user chose, and never point at an
// input method that is not installed — an unresolvable QT_IM_MODULE costs
// every Qt application a warning and gains nothing.
void applyInputMethodDefaults()
{
    constexpr const char *kQtModule = "QT_IM_MODULE";
    constexpr const char *kGtkModule = "GTK_IM_MODULE";
    constexpr const char *kXModifiers = "XMODIFIERS";
    if (qEnvironmentVariableIsSet(kQtModule) || qEnvironmentVariableIsSet(kGtkModule)
        || qEnvironmentVariableIsSet(kXModifiers)) {
        return;
    }
    if (QStandardPaths::findExecutable(QStringLiteral("ibus-daemon")).isEmpty()) {
        return;
    }
    qputenv(kQtModule, "ibus");
    qputenv(kGtkModule, "ibus");
    qputenv(kXModifiers, "@im=ibus");
}

} // namespace

void SessionEnvironment::apply(const SessionOptions &options)
{
    qputenv("XDG_CURRENT_DESKTOP", "QindaQt");
    qputenv("XDG_SESSION_DESKTOP", "qindaqt");
    qputenv("XDG_SESSION_TYPE", "wayland");
    // AGENT-CONTRACT: One Qt platform theme supplies QindaQt and other Qt
    // applications. Explicit user toolkit overrides always retain authority.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "qindaqt");
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE"))
        qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    applyInputMethodDefaults();
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
