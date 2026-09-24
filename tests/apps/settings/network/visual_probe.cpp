// SPDX-License-Identifier: GPL-3.0-or-later

// Headless capture of the production Settings Network page (plan W2 signal
// meters), so the bars can be reviewed as light and dark pictures rather than
// as a diff. Renders the real NetworkPage.qml against the page test's stub;
// nothing here touches a bus, a running service, or any user state.
//
// argv: <themeJson> <outputPng> <width> <height>

#include "stub_network_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest/QTest>

#include <cstdio>
#include <memory>

using QindaQt::Apps::SettingsNetwork::TestSupport::StubNetworkSettingsModel;

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  if (argc < 5) {
    std::fputs("usage: visual_probe <themeJson> <outputPng> <w> <h>\n", stderr);
    return 2;
  }
  const QString themeJson = QString::fromLocal8Bit(argv[1]);
  const QString output = QString::fromLocal8Bit(argv[2]);
  const QSize size(QString::fromLocal8Bit(argv[3]).toInt(),
                   QString::fromLocal8Bit(argv[4]).toInt());

  QQuickView view;
  view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString error;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *view.engine(), &error);
  if (facade == nullptr) {
    std::fprintf(stderr, "tokens: %s\n", qPrintable(error));
    return 3;
  }
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(themeJson);
  if (!loaded.ok) {
    std::fprintf(stderr, "theme: %s\n", qPrintable(loaded.error));
    return 4;
  }
  if (!facade->publish(loaded.theme, {}, &error)) {
    std::fprintf(stderr, "publish: %s\n", qPrintable(error));
    return 5;
  }

  StubNetworkSettingsModel model;
  // Rows that differ in everything beside the bar (digits, Saved versus
  // Connect, prompt length), so a capture shows whether the bars line up.
  QVariantMap saved = model.accessPoints.at(0).toMap();
  saved.insert(QStringLiteral("signalStrength"), 100);
  QVariantMap weak = model.accessPoints.at(1).toMap();
  weak.insert(QStringLiteral("signalStrength"), 5);
  weak.insert(QStringLiteral("promptStatusText"),
              QStringLiteral("A password prompt will appear if the desktop "
                             "password agent is still running when you connect."));
  QVariantMap other = weak;
  other.insert(QStringLiteral("id"), QString(64, u'e'));
  other.insert(QStringLiteral("displayName"),
               QStringLiteral("Library guest network with a long name"));
  other.insert(QStringLiteral("signalStrength"), 61);
  other.insert(QStringLiteral("promptStatusText"), QStringLiteral("Open network."));
  model.accessPoints = {saved, weak, other};
  QQmlComponent component(view.engine());
  component.loadUrl(
      QUrl::fromLocalFile(QStringLiteral(QINDAQT_NETWORK_PAGE_QML_PATH)));
  if (!component.isReady()) {
    std::fprintf(stderr, "page: %s\n", qPrintable(component.errorString()));
    return 7;
  }
  std::unique_ptr<QObject> object(component.createWithInitialProperties(
      {{QStringLiteral("networkSettings"),
        QVariant::fromValue(static_cast<QObject *>(&model))}}));
  auto *page = qobject_cast<QQuickItem *>(object.get());
  if (page == nullptr) {
    std::fprintf(stderr, "page: %s\n", qPrintable(component.errorString()));
    return 8;
  }

  view.resize(size);
  page->setParentItem(view.contentItem());
  page->setSize(size);
  view.show();
  if (!QTest::qWaitFor([&view] { return view.isExposed(); }))
    return 9;
  // Captures are taken tall enough to show the whole page unscrolled.
  QTest::qWait(300);

  const QImage image = view.grabWindow();
  if (image.isNull()) return 10;
  if (!image.save(output)) return 11;
  std::fprintf(stdout, "captured %dx%d to %s\n", image.width(), image.height(),
               qPrintable(output));
  return 0;
}
