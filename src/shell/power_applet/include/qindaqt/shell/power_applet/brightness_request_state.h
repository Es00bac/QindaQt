// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/power_applet/power_applet_types.h>

#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Shell::PowerApplet {

enum class RequestPhase : quint32 {
  Idle = 0,
  Pending = 1,
  Succeeded = 2,
  Failed = 3,
  Uncertain = 4,
};

// Typed failure feedback. It separates user-actionable upstream outcomes
// (rejected, busy, inhibited, ...) from the presentation-side honesty rules
// (stale reply, lineage mismatch, owner loss), so a future QML surface can
// choose wording without re-deriving cause.
enum class FailureKind : quint32 {
  None = 0,
  Rejected = 1,
  Unsupported = 2,
  Failed = 3,
  Busy = 4,
  AuthenticationRequired = 5,
  Inhibited = 6,
  ServiceUncertain = 7,
  LineageMismatch = 8,
  OwnerLost = 9,
  MalformedReply = 10,
};

// One bounded brightness control request. At most one request is described at
// a time; the machine is a pure value, so a shell facade owns its lifetime.
// AGENT-CONTRACT: Terminal requests are never mutated again and never replay
// automatically; `Uncertain` always instructs the user path to resnapshot.
struct BrightnessRequest {
  RequestPhase phase = RequestPhase::Idle;
  Power::Handle device;
  quint64 initiatingEpoch = 0;
  quint64 initiatingRevision = 0;
  FailureKind failure = FailureKind::None;
  QString reasonCode;
  QString diagnostic;
  QString feedback;

  friend bool operator==(const BrightnessRequest &,
                         const BrightnessRequest &) = default;
};

// Starts a keyboard-brightness request against `device` using `snapshot` as
// the initiating generation. A device that is absent, unidentified, or not
// adjustable yields a terminal Failed request with typed feedback instead of
// a Pending one, so callers cannot wait on a request that was never legal.
[[nodiscard]] BrightnessRequest
beginKeyboardBrightnessRequest(const Power::Snapshot &snapshot,
                               const Power::Handle &device);

// Applies one `OperationResult` to the request. Stale-lineage replies are
// discarded without changing the pending request. Success requires the
// observed generation to stay in the initiating epoch at or after the
// initiating revision; anything else becomes typed Uncertain, never a silent
// success.
[[nodiscard]] BrightnessRequest
applyOperationResult(const BrightnessRequest &request,
                     const Power::OperationResult &result);

// Re-evaluates the request against a freshly observed generation. Owner loss
// or an epoch replacement while pending yields typed Uncertain (OwnerLost);
// terminal requests are returned unchanged.
[[nodiscard]] BrightnessRequest
observeGeneration(const BrightnessRequest &request, bool powerOwnerAvailable,
                  quint64 observedEpoch);

} // namespace QindaQt::Shell::PowerApplet

Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::RequestPhase)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::FailureKind)
Q_DECLARE_METATYPE(QindaQt::Shell::PowerApplet::BrightnessRequest)
