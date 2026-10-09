// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/windows_runner/executable_open.h>

namespace QindaQt::WindowsRunner {
WindowsExecutableOpenResult routeWindowsExecutableOpen(
    const QString &canonicalPath, const WindowsExecutableOpenRequest &request,
    WindowsExecutableOpenPort &port)
{
    if (!port.handlesRequest(canonicalPath, request)) {
        return {ExecutableOpenDisposition::UseDesktopHandler, {}};
    }
    auto inspected = inspectWindowsExecutable(canonicalPath);
    if (!inspected.file) {
        return {ExecutableOpenDisposition::Failed, inspected.message};
    }
    if (!inspected.file->stillCurrent()) {
        return {ExecutableOpenDisposition::Failed,
                QStringLiteral("The selected Windows program changed. Select it again.")};
    }
    return port.open(std::move(inspected.file), request);
}
} // namespace QindaQt::WindowsRunner
