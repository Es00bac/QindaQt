// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/runtime_port.h"

// AGENT-CONTRACT: Source compatibility only; these static-library types now
// belong to the service protocol, never a dependency on app implementation.
// All consumers rebuild together; see
// docs/wiki/adr/0372-bind-private-android-producers-to-kernel-lifetimes.md.
namespace QindaQt::AndroidManager {
using Services::AndroidRuntime::ManagedApplicationBinding;
using Services::AndroidRuntime::ManagedApplicationRegistration;
using Services::AndroidRuntime::RuntimeState;
using Services::AndroidRuntime::ApplicationState;
using Services::AndroidRuntime::LaunchPhase;
using Services::AndroidRuntime::ApplicationStatus;
using Services::AndroidRuntime::RuntimeSnapshot;
using Services::AndroidRuntime::LaunchRequest;
using Services::AndroidRuntime::LaunchSubmission;
using Services::AndroidRuntime::LaunchResult;
using Services::AndroidRuntime::RuntimePort;
using Services::AndroidRuntime::validateRuntimeSnapshot;
using Services::AndroidRuntime::validRuntimeMessage;
} // namespace QindaQt::AndroidManager
