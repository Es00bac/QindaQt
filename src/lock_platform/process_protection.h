// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace QindaQt::LockPlatform {
// Native authority startup only; Linux GUI thread before Qt/PAM/credentials.
// No system settings changed. Core/dump protection persists for this process.
bool protectAuthority();
bool restrictedPtracePolicy();
bool rootManagedPath(const QString &path, bool directory = false);
// Qt native greeter accepts only compiled resources and these root-managed
// module/plugin directories; user preferences may select data, never code.
bool trustedQtPaths();
}
