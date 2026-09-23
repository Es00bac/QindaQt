// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_route_construction_probe.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QString>
#include <QTimer>

#include <cstdio>
#include <utility>

// Internal package-test observer. The registered route remains the source of
// expected identity/availability; a Loader signal from a different route or
// the wrong state cannot turn residence into a successful construction proof.
class RouteConstructionObserver final : public QObject {
  Q_OBJECT

public:
  RouteConstructionObserver(QString requestedRoute, bool available,
                            QCoreApplication &application,
                            QQmlApplicationEngine &engine)
      : m_requestedRoute(std::move(requestedRoute)), m_available(available),
        m_application(application), m_engine(engine) {}

public slots:
  void record(const QString &routeId, const QString &state) {
    if (m_reported) return;
    const QString expected = m_available ? QStringLiteral("ready")
                                         : QStringLiteral("diagnosed-unavailable");
    if (routeId != m_requestedRoute || state != expected) {
      std::fprintf(stderr, "qindaqt-settings: invalid route witness: %s %s\n",
                   qPrintable(routeId), qPrintable(state));
      m_reported = true;
      QTimer::singleShot(0, &m_application, [] { QCoreApplication::exit(3); });
      return;
    }
    m_reported = true;
    std::fprintf(stdout, "QINDAQT_ROUTE_CONSTRUCTED %s %s\n",
                 qPrintable(routeId), qPrintable(state));
    std::fflush(stdout);
    // Give deferred bindings one short event-loop turn after Loader.Ready;
    // warnings emitted by that route must still reach the package gate.
    QTimer::singleShot(30, &m_application, [this] {
      // Root QML must be destroyed while its route models are still alive.
      // Otherwise normal stack teardown emits binding warnings unrelated to
      // route construction and masks the warning gate.
      const auto roots = m_engine.rootObjects();
      for (QObject *root : roots) delete root;
      QCoreApplication::quit();
    });
  }

private:
  QString m_requestedRoute;
  bool m_available;
  QCoreApplication &m_application;
  QQmlApplicationEngine &m_engine;
  bool m_reported = false;
};

namespace QindaQt::Apps::SettingsCenter {
// The route probe is an internal package-test CLI. Keep observation and
// failure policy out of the normal composition path and below the main
// function source-shape limit; only the active requested route may pass.
int runRouteConstructionProbe(
    QCoreApplication &application, QQmlApplicationEngine &engine,
    const QString &page, bool available) {
  RouteConstructionObserver probe(page, available, application, engine);
  QObject *root = engine.rootObjects().first();
  const char *const hostNames[] = {"wideSettingsRouteHost",
                                   "compactSettingsRouteHost"};
  for (const char *name : hostNames) {
    QObject *host = root->findChild<QObject *>(QLatin1String(name));
    if (!host || !QObject::connect(
                     host, SIGNAL(routeConstructed(QString,QString)),
                     &probe, SLOT(record(QString,QString)))) {
      std::fprintf(stderr, "qindaqt-settings: route witness host missing: %s\n",
                   name);
      return 3;
    }
    // The Loader may have completed synchronously before C++ connected.
    QTimer::singleShot(0, &application, [host] {
      QMetaObject::invokeMethod(host, "reportConstructionWitness");
    });
  }
  return application.exec();
}
} // namespace QindaQt::Apps::SettingsCenter

#include "settings_route_construction_probe.moc"
