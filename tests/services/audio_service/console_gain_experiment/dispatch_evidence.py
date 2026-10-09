# SPDX-License-Identifier: GPL-3.0-or-later
"""Cooperative attempt/evidence budgets; no process, signal or Portage actions."""
from __future__ import annotations
import time
from collections.abc import Callable
from process_evidence import ProcessCancelled


class DispatchBudget:
    """Borrowed non-raising monotonic clock; same-thread cooperative requests.

    The manager keeps this object alive through every runner and publication.
    A signal handler only stores a request, never raises or performs cleanup.
    Evidence checks before finish_attempt are a programming error; valid
    cancellation/deadline predicates never raise or reap another owner's child.
    finish_attempt is idempotent and cannot extend either absolute deadline.
    """
    def __init__(self, attempt_seconds: float, evidence_seconds: float, *,
                 clock: Callable[[], float] = time.monotonic):
        self.clock = clock
        self.started = clock()
        self.attempt_deadline = self.started + attempt_seconds
        self.total_deadline = self.attempt_deadline + evidence_seconds
        self.evidence_seconds = evidence_seconds
        self.attempt_ended = None
        self.evidence_deadline = None
        self.request_reason = None

    def request(self, reason) -> None:
        if self.request_reason is None:
            self.request_reason = reason

    def attempt_cancelled(self) -> bool:
        if self.clock() >= self.attempt_deadline:
            self.request("attempt-deadline")
        return self.request_reason is not None

    def finish_attempt(self) -> None:
        if self.attempt_ended is None:
            self.attempt_ended = self.clock()
            if self.attempt_ended >= self.attempt_deadline:
                self.request("attempt-deadline")
            self.evidence_deadline = min(self.total_deadline,
                self.attempt_ended + self.evidence_seconds)

    def evidence_cancelled(self) -> bool:
        if self.evidence_deadline is None:
            raise RuntimeError("attempt must settle before evidence admission")
        if self.clock() >= self.evidence_deadline:
            self.request("evidence-deadline")
        return self.request_reason is not None

    def snapshot(self) -> dict:
        completed = self.clock()
        if self.evidence_deadline is not None and completed >= self.evidence_deadline:
            self.request("evidence-deadline")
        return {"attemptedBuildSeconds": self.attempt_ended - self.started,
                "cleanupEvidenceSeconds": completed - self.attempt_ended,
                "completedSeconds": completed - self.started,
                "attemptBudgetSeconds": self.attempt_deadline - self.started,
                "evidenceBudgetSeconds": self.evidence_seconds,
                "attemptIncludesOwnedPhaseCleanup": True,
                "cancelReason": self.request_reason}


def finish_dispatch(budget: DispatchBudget, result: dict, original: BaseException | None,
                    final_inputs: Callable[[], None], raw_index: Callable[[], object],
                    write_result: Callable[[dict], None]) -> tuple[dict, BaseException | None]:
    """Publish candidate evidence, then decide admission after its write returns.

    Collaborators are borrowed synchronous calls, with cooperative loop checks
    where they read. The file never asserts final success: only this returned
    terminal report PLUS manager-owned successful wait/settlement can admit the
    attempt. Thus late result-write completion cannot leave a false success file.
    Primary exceptions survive all secondary verification/publication failures.
    """
    budget.finish_attempt()
    errors = []
    for name, action in (("finalVerificationError", final_inputs),
                         ("rawPublicationError", raw_index)):
        try:
            if budget.evidence_cancelled():
                raise ProcessCancelled("evidence cancelled before " + name)
            value = action()
            if name == "rawPublicationError": result["rawPublication"] = value
            if budget.evidence_cancelled():
                raise ProcessCancelled("evidence deadline crossed during " + name)
        except BaseException as error:
            result[name] = type(error).__name__ + ": " + str(error)
            result["success"] = False
            errors.append(error)
    candidate = original is None and not errors and result.get("success") is True
    stored = {**result, "success": False, "successCandidate": candidate,
              "requiresManagerOwnedSettlement": True, **budget.snapshot()}
    try:
        write_result(stored)
    except BaseException as error:
        result["resultPublicationError"] = type(error).__name__ + ": " + str(error)
        errors.append(error)
    if budget.evidence_cancelled():
        errors.append(ProcessCancelled("late/cancelled final result publication"))
    if original is None and errors:
        original = errors.pop(0)
    if original is not None:
        original.evidence_errors = tuple(errors)
        for error in errors:
            original.add_note("Final evidence error: " + type(error).__name__ + ": " + str(error))
    completed = budget.snapshot()
    if completed["cancelReason"] is not None and original is None:
        original = ProcessCancelled("completion snapshot reached deadline or cancellation")
    terminal = {**result, "success": original is None and candidate,
                "requiresManagerOwnedSettlement": True, **completed}
    return terminal, original

def emit_terminal(terminal: dict, original: BaseException | None,
                  emit: Callable[[dict], None], cancelled: Callable[[], bool], *,
                  diagnose: Callable[[str], None] | None = None) -> None:
    """Own report emission through its post-call refusal and exact error raise.

    Borrowed synchronous sinks/predicate outlive this call. Emit includes flush;
    its first error is authoritative only when no earlier build/evidence error
    exists. Diagnostic sinks are secondary and cannot replace either primary.
    Existing cleanup/evidence attachments survive terminal errors unchanged.
    """
    errors = []
    try:
        emit(terminal)
    except BaseException as error:
        errors.append(error)
    try:
        if cancelled():
            raise ProcessCancelled("terminal emission completed late or cancelled")
    except BaseException as error:
        errors.append(error)
    if original is None and errors:
        original = errors.pop(0)
    if original is None:
        return
    if diagnose is not None:
        try:
            diagnose("Terminal evidence failed; original exception retained.")
        except BaseException as error:
            errors.append(error)
    original.evidence_errors = tuple(getattr(original, "evidence_errors", ())) + tuple(errors)
    for error in errors:
        # Fixed type notes avoid a failing error formatter becoming a new
        # diagnostic failure; full secondary objects remain attached above.
        BaseException.add_note(original, "Terminal evidence error: " + type(error).__name__)
    raise original
