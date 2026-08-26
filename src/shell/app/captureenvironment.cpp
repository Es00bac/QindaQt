// SPDX-License-Identifier: GPL-3.0-or-later
#include "captureenvironment.h"

#include <QByteArray>

namespace QindaQt::Shell {
namespace {

bool requestsScreenshot(int argumentCount, char *arguments[])
{
    for (int index = 1; index < argumentCount; ++index) {
        const QByteArray argument(arguments[index]);
        if (argument == "--screenshot" || argument.startsWith("--screenshot=")) {
            return true;
        }
    }
    return false;
}

void setUnlessConfigured(const char *name, const char *value)
{
    if (qEnvironmentVariableIsEmpty(name)) {
        qputenv(name, value);
    }
}

} // namespace

void configureCaptureEnvironment(int argumentCount, char *arguments[])
{
    if (!requestsScreenshot(argumentCount, arguments)) {
        return;
    }

    // AGENT-CONTRACT: Screenshot mode uses a fixed software stack unless the
    // harness explicitly supplies an alternative, keeping CI baselines stable.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
        && qEnvironmentVariableIsEmpty("DISPLAY")
        && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    setUnlessConfigured("QT_QUICK_BACKEND", "software");
    setUnlessConfigured("QSG_RENDER_LOOP", "basic");
    setUnlessConfigured("QT_SCALE_FACTOR", "1");
}

} // namespace QindaQt::Shell
