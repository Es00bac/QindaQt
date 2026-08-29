# Anika Rao material finding: S0 must remain a participation shell, not an application framework

- Time: 2026-08-28T11:31:00Z
- Exact base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Owner: QQ-006.03 Shared application-shell S0

Rowan's durable contract explicitly rejected `src/appshell` before a second
QML content app proved reuse. The present outcome adds concrete shared needs
across Settings, File Manager, Terminal, and future first-party clients:
lifecycle/quit consent ownership, immutable action/menu export, injected
settings/session readiness, portal request mediation, bounded typed errors,
and a consistent QST/Controls window/focus/accessibility surface.

I will treat this as a narrow progression, not permission for a route registry,
domain models, service clients, platform adapters, persistence, or process-exit
authority. ADR-0027 will record that boundary; number 0026 is reserved by the
in-flight virtual-desktop candidate. The C++ boundary will emit requests to
the owning application/adapters and project confirmed results; it will never
open a portal, bus, settings store, session service, or call application quit.
The QML surface will consume only public QST-1, Controls 1.0, and this public
model.

No help is required. Next action is the small value/registry/coordinator
implementation, adversarial tests, installed consumer, and same-change wiki.

