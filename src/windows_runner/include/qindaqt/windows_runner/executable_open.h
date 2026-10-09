// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/windows_runner/executable_file.h>

namespace QindaQt::WindowsRunner {
enum class ExecutableOpenMode { DefaultHandler, OpenWith };
struct WindowsExecutableOpenRequest final {
    ExecutableOpenMode mode = ExecutableOpenMode::DefaultHandler;
    // Exact scan-retained handler ID for Open With; empty means use current
    // user default/profile policy. Never an argv, executable path or title.
    QString chosenApplicationId;
    // SHA-256 of exact scan-retained desktop bytes. The owner checks current
    // handler provenance/digest; a native override with the same ID must not
    // inherit Windows routing. Empty on default-handler requests.
    QString chosenHandlerContentDigest;
};
enum class ExecutableOpenDisposition {
    UseDesktopHandler, Starting, SetupOpened, Failed, OutcomeUnknown,
};
struct WindowsExecutableOpenResult final {
    ExecutableOpenDisposition disposition = ExecutableOpenDisposition::Failed;
    QString message;
};

class WindowsExecutableOpenPort {
public:
    virtual ~WindowsExecutableOpenPort() = default;
    // Current user defaults/explicit chosen handler decide routing. A filename
    // extension can be a discovery hint, never executable/managed authority.
    // For OpenWith, recheck exact current handler provenance and content digest;
    // an ID/title match alone is insufficient to take over a native override.
    [[nodiscard]] virtual bool handlesRequest(
        const QString &canonicalPath, const WindowsExecutableOpenRequest &request) const = 0;
    // Own the selected PE handle across visible first-run/profile choice. An
    // unknown program has user-file provenance, never fabricated Portage
    // recipe identity. The owner distinguishes portable/installer setup from
    // explicit user choice/recipe policy, not a filename heuristic, and retains
    // its per-app prefix/data. Runtime installation remains Portage-owned.
    // At actual dispatch hold current registration/profile authority
    // transactionally, recheck this file lease and consume checked handles.
    // SetupOpened means a real visible setup was opened; Starting means one
    // launch was submitted. Failed/UseDesktopHandler must have no launch or
    // setup side effect. Any post-submission uncertainty is OutcomeUnknown.
    [[nodiscard]] virtual WindowsExecutableOpenResult open(
        std::unique_ptr<WindowsExecutableFile> file,
        const WindowsExecutableOpenRequest &request) = 0;
};

// Shared bounded route for File Manager double-click and Open With. User
// defaults are checked before PE admission, preserving deliberate native
// handlers. A selected Windows route never falls back after dispatch/failure.
[[nodiscard]] WindowsExecutableOpenResult routeWindowsExecutableOpen(
    const QString &canonicalPath, const WindowsExecutableOpenRequest &request,
    WindowsExecutableOpenPort &port);
} // namespace QindaQt::WindowsRunner
