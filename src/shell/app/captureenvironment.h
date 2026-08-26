// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Shell {

// This must run before QGuiApplication constructs a platform integration.
void configureCaptureEnvironment(int argumentCount, char *arguments[]);

} // namespace QindaQt::Shell
