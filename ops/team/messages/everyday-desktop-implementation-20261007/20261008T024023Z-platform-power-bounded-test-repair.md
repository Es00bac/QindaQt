# Power bounded-return fixture correction

- Prior source bbe80536de954f19f630a37389c79188fc37f2fa preserved, native unrun. Manager source review found no production blocker but identified tautological full-startup assertion.
- Removed constant desktopCanContinue=true/verification from owning tst_replaced_power_owner.cpp; renamed row unretiredOwnerReturnIsBounded and scoped comment to measured bounded return plus retained owner. Production predicate, private/pidfd/owner fences and main ordering unchanged.
- No full startup continuation test or new bootstrap suite claimed. Existing main's best-effort continuation is source policy only until its owning integrated startup qualification.
- git diff --check0; no compiler/native/bus/Portage gate executed. Candidate source needs focused native lease/qualification; R17 exact98/b128 remains independent and urgent.
