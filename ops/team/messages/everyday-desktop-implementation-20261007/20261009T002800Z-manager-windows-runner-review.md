# Manager review: public WindowsRunner core

- Time: 2026-10-09T00:28:00+00:00
- Exact source candidate: 67a8a175c48c01e225cbf40ffc9cac8f5813e74a
- Windows-only documentation successor: 98366a7e60ab157f68e256d9eab574acea0602e7
- Decision: ACCEPT as a source-only public QtCore planning/coordinator boundary.

Independent source review checked canonical binding, complete-recipe recheck
after the checked-payload lease, literal argv, allowlisted environment, one
dispatch per call, Starting vs OutcomeUnknown and explicit unsupported Proton
adapter behavior. The borrowed ports leave payload ownership and final spawn
authority with the future adapter; no real Wine app starts from this core.
Before a concrete adapter is admitted, Refused must guarantee zero submission,
any uncertain post-submission state must become OutcomeUnknown, and a
registration/profile transaction lease must be checked at the actual dispatch
boundary. The author acknowledged that follow-up.

Author evidence: one 13.166s focused strict build, 2/2 CTests with 44 Qt
checks, strict MkDocs and 535-document links in a 28.14s bounded unit.
Manager retained both Android catalog and WindowsRunner rows through three
additive shared-registry conflicts; no duplicate full C++ build. Global Menu,
public catalog adapter, installed runtime/recipe producer, real Windows
application journey and Audio1 remain open.
