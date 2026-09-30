// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace QindaQt::Platform::Compositor::Private {
bool nativeName(const QString &name);
int connectPeer(const QString &runtime, const QString &name, qint64 expectedPid,
                int *pidfd = nullptr);
} // namespace QindaQt::Platform::Compositor::Private
