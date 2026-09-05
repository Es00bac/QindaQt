# Launcher screenshot warning diagnosis and repair

The staged schema already contained both launcher keys and the real service was healthy. A new status assertion reproduced the screenshot warning after persistenceReady: the constructor's initial Unavailable notice was never retired on a confirmed baseline. Availability notices are now tracked separately from meaningful save/malformed-data errors and clear on recovery; save refusal explanations survive owner replacement.

Separately, one malformed desktop entry was surfaced as a global launcher error by diagnostic(). Usable catalogs now retain detailed diagnostics internally and via opt-in qindaqt.launcher.scan debug logging, without an unqualified user banner. Entirely unusable catalogs retain an actionable message.

Verification:5/5 focused launcher CTests pass (persistence, real Settings1 contract, controller, source boundary, contract text); focused build and strict MkDocs/link/source-shape checks pass. Evidence is under `.cache/evidence-launcher-usability`. No production compositor was launched by this worker; nested screenshot rerun belongs to manager after integration.
