// SPDX-License-Identifier: GPL-3.0-or-later
// Import only the staged Audio module. Presentation bootstrap mirrors
// Settings Center and audio_page_test_support, without activating Audio1.
#include "stub_audio_settings_model.h"
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/shell/icons/icon_runtime.h>
#include <qindaqt/themes/theme_loader.h>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>
#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)
using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root->objectName() == name) return root;
  for (auto *child : root->childItems())
    if (auto *found = findItem(child, name)) return found;
  return nullptr;
}
}
int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  const auto args = application.arguments();
  if (args.size() != 7) return 2;
  QQuickView view;
  auto &engine = *view.engine();
  // AGENT-GUARD: neither system QindaQt nor source/build Audio QML imports
  // can rescue a staged missing module/file. Public deps exclude QindaQt.
  engine.setImportPathList({args.at(1), args.at(2)});
  const auto module = args.at(1) + "/QindaQt/SettingsApp/Audio";
  QFile metadata(module + "/qmldir");
  if (!metadata.open(QIODevice::ReadOnly)) return 3;
  QString error;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(engine, &error);
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(args.at(4));
  if (!facade || !theme.ok || !facade->publish(theme.theme, {}, &error) ||
      !QindaQt::Shell::Icons::IconRuntime::install(engine, {args.at(5)}, {"QindaQt"})) {
    qCritical().noquote() << "presentation bootstrap failed" << error;
    return 4;
  }
  QStringList types;
  QTextStream lines(&metadata);
  while (!lines.atEnd()) {
    const auto fields = lines.readLine().simplified().split(' ');
    if (fields.size() != 3 || fields.at(1) != "1.0" ||
        !fields.at(2).endsWith(".qml")) continue;
    if (types.contains(fields.at(0))) return 5;
    types.append(fields.at(0));
    QQmlComponent component(&engine);
    if (args.at(3) == "disk")
      component.loadUrl(QUrl::fromLocalFile(module + '/' + fields.at(2)));
    else
      component.loadFromModule("QindaQt.SettingsApp.Audio", fields.at(0));
    if (!component.isReady()) {
      qCritical().noquote() << fields.at(2) << component.errorString();
      return 1;
    }
  }
  if (types.size() != 23 || !types.contains("AudioConsoleSection") ||
      !types.contains("AudioConsoleStrip") || !types.contains("AudioDeviceSection"))
    return 5;
  StubAudioSettingsModel model;
  QQmlComponent pageComponent(&engine);
  if (args.at(3) == "disk")
    pageComponent.loadUrl(QUrl::fromLocalFile(module + "/qml/AudioPage.qml"));
  else
    pageComponent.loadFromModule("QindaQt.SettingsApp.Audio", "AudioPage");
  std::unique_ptr<QObject> page(pageComponent.createWithInitialProperties(
      {{"audioSettings", QVariant::fromValue(static_cast<QObject *>(&model))}}));
  auto *item = qobject_cast<QQuickItem *>(page.get());
  if (!item) {
    qCritical().noquote() << pageComponent.errorString();
    return 1;
  }
  view.resize(1280, 720);
  item->setParentItem(view.contentItem());
  item->setSize(QSizeF(1280, 720));
  view.show();
  if (!QTest::qWaitForWindowExposed(&view)) return 6;
  auto *volume = findItem(item, "audioOutputVolume_10");
  if (!volume || !QTest::qWaitFor([volume] {
        return volume->isVisible() && volume->height() >= 22;
      })) return 6;
  if (!args.at(6).isEmpty() &&
      !view.grabWindow().save(args.at(6) + "-devices.png")) return 6;
  if (!item->setProperty("activeTab", 1)) return 6;
  auto *console = findItem(item, "audioConsoleSection");
  auto *fader = findItem(item, "consoleStripFader_strip.hw.1");
  if (!console || !fader || !QTest::qWaitFor([console, fader] {
        return console->isVisible() && fader->isVisible() &&
               fader->width() > 0 && fader->height() > 0;
      })) return 6;
  if (!args.at(6).isEmpty() &&
      !view.grabWindow().save(args.at(6) + "-console.png")) return 6;
  item->setParentItem(nullptr);
  qInfo() << "Staged Audio components:" << types.size()
          << "full-page device/console construction:" << args.at(3);
  return 0;
}
