// SPDX-License-Identifier: GPL-3.0-or-later
#include "qml_component_ready.h"

#include <QEventLoop>
#include <QQmlComponent>
#include <QString>
#include <QTimer>

namespace QindaQt::Apps::FileManager {
namespace {

constexpr int componentLoadTimeoutMs = 5'000;

} // namespace

bool awaitQmlComponentReady(QQmlComponent &component, QString *diagnostic) {
  if (component.status() == QQmlComponent::Loading) {
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&component, &QQmlComponent::statusChanged, &loop,
                     [&loop](QQmlComponent::Status status) {
                       if (status != QQmlComponent::Loading) {
                         loop.quit();
                       }
                     });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(componentLoadTimeoutMs);
    loop.exec();
  }

  if (component.isReady()) {
    return true;
  }

  if (diagnostic != nullptr) {
    const QString componentError = component.errorString().trimmed();
    if (!componentError.isEmpty()) {
      *diagnostic = componentError;
    } else if (component.status() == QQmlComponent::Loading) {
      *diagnostic =
          QStringLiteral("Timed out resolving QindaQt.Tokens after %1 ms")
              .arg(componentLoadTimeoutMs);
    } else {
      *diagnostic =
          QStringLiteral("QindaQt.Tokens component failed with status %1")
              .arg(static_cast<int>(component.status()));
    }
  }
  return false;
}

} // namespace QindaQt::Apps::FileManager
