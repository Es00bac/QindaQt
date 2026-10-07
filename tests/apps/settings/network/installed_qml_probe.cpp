// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  const auto arguments = app.arguments();
  if (arguments.size() != 4) {
    return 2;
  }
  QQmlEngine engine;
  // AGENT-GUARD: no developer or installed Network import root may rescue a
  // withheld staged module. The harness supplies only isolated public deps.
  engine.setImportPathList({arguments.at(1), arguments.at(2)});
  const QString module = arguments.at(1) +
      QStringLiteral("/QindaQt/SettingsApp/Network");
  QFile metadata(module + QStringLiteral("/qmldir"));
  if (!metadata.open(QIODevice::ReadOnly)) {
    return 3;
  }
  int count = 0;
  int failures = 0;
  QTextStream lines(&metadata);
  while (!lines.atEnd()) {
    const auto fields = lines.readLine().simplified().split(QLatin1Char(' '));
    if (fields.size() != 3 || fields.at(1) != QStringLiteral("1.0") ||
        !fields.at(2).endsWith(QStringLiteral(".qml"))) {
      continue;
    }
    ++count;
    QQmlComponent component(&engine);
    if (arguments.at(3) == QStringLiteral("disk")) {
      component.loadUrl(QUrl::fromLocalFile(module + QLatin1Char('/') +
                                           fields.at(2)));
    } else {
      component.loadFromModule(QStringLiteral("QindaQt.SettingsApp.Network"),
                               fields.at(0));
    }
    if (component.status() != QQmlComponent::Ready) {
      ++failures;
      qCritical().noquote() << fields.at(2) << component.errorString();
    }
  }
  qInfo() << "Network components:" << count << "failures:" << failures;
  return count == 7 && failures == 0 ? 0 : 1;
}
