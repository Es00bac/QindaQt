// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/runtime_port.h"
#include <memory>
namespace QindaQt::Services::AndroidRuntime {
// Owns only its explicitly started native collector. GUI-thread start/destruction;
// RuntimePort is borrowed and must not outlive this owner. Closed installed
// helper/recipe paths, verified current Session1 executable/lifetime, no guest
// start or public fd/PID/argv mode. Missing facts stay Unavailable (ADR-0375).
class NativeRuntimeBootstrap final {
public:
    NativeRuntimeBootstrap();
    ~NativeRuntimeBootstrap();
    NativeRuntimeBootstrap(const NativeRuntimeBootstrap &)=delete;
    NativeRuntimeBootstrap &operator=(const NativeRuntimeBootstrap &)=delete;
    bool start();
    RuntimePort *port() const;
    QString error() const;
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::AndroidRuntime
