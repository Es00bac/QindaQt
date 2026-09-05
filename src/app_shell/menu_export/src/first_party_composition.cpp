// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <qindaqt/app_shell/application_coordinator.h>

#include <QWindow>
#include <QtGlobal>

#include <cstdio>

namespace QindaQt::AppShell::MenuExport {
namespace {

#if defined(QINDAQT_ENABLE_APP_MENU_EXPORT_TEST_SEAM)
// Offscreen test rows have no real native window identifier. The seam injects
// the exact registrar window id the test announces as the compositor
// projection, and mirrors AppShell activation onto stdout so a hostile row can
// count real application activations. Compiled only into test builds.
class FixedWindowIdentityPublisher final
    : public WindowMenuIdentityPublisher {
public:
  explicit FixedWindowIdentityPublisher(quint32 id) : m_id(id) {}
  std::optional<WindowMenuIdentity>
  publish(QWindow &, const QString &, const QString &) override {
    return WindowMenuIdentity{
        .kind = WindowMenuIdentityKind::XWindow,
        .registrarWindowId = m_id};
  }
  void withdraw(QWindow &) override {}

private:
  quint32 m_id;
};
#endif

} // namespace

std::unique_ptr<QObject> composeFirstPartyMenuExport(
    ApplicationCoordinator &coordinator, QWindow &window,
    QDBusConnection sessionBus) {
#if defined(QINDAQT_ENABLE_APP_MENU_EXPORT_TEST_SEAM)
  bool idOk = false;
  const int testWindowIdValue =
      qEnvironmentVariableIntValue("QINDAQT_TEST_APPMENU_WINDOW_ID", &idOk);
  if (idOk && testWindowIdValue > 0) {
    const auto testWindowId = static_cast<quint32>(testWindowIdValue);
    if (qEnvironmentVariableIsSet("QINDAQT_TEST_APPMENU_TRACE_ACTIVATION")) {
      QObject::connect(
          &coordinator,
          &QindaQt::AppShell::ApplicationCoordinator::actionRequested,
          &coordinator, [](const QString &actionId) {
            std::fprintf(stdout, "ACTIVATED %s\n", qPrintable(actionId));
            std::fflush(stdout);
          });
    }
    auto composition =
        std::make_unique<ApplicationMenuExport>(coordinator, window, sessionBus,
                                                std::make_unique<
                                                    FixedWindowIdentityPublisher>(
                                                    testWindowId));
    (void)composition->start();
    return std::unique_ptr<QObject>(std::move(composition));
  }
#endif
  if (!sessionBus.isConnected()) {
    return {};
  }
  return std::unique_ptr<QObject>(
      ApplicationMenuExport::compose(coordinator, window, sessionBus));
}

} // namespace QindaQt::AppShell::MenuExport
