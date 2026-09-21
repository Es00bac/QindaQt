// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Shared harness for the two Settings Audio page suites:
// tst_audio_page.cpp (the route surface: inventory, intents, focus) and
// tst_audio_console_page.cpp (the console grid: band alignment, the gain
// law, control dispatch). One engine setup and one page factory, so the two
// suites cannot drift into testing two different pages. Split by behavior
// per the repository's 600-line ceiling on hand-written sources.

#include "stub_audio_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/shell/icons/icon_runtime.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtCore/QCoreApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>

#include <memory>
#include <utility>

namespace QindaQt::Apps::SettingsAudio::TestSupport {

inline QQuickItem *findItem(QQuickItem *root, const QString &objectName) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == objectName) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (QQuickItem *match = findItem(child, objectName); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

inline void attach(QQuickView &view, QQuickItem &page, const QSize size) {
  view.resize(size);
  page.setParentItem(view.contentItem());
  page.setSize(size);
  view.show();
  QCoreApplication::processEvents();
}

// The engine the page runs under in both suites: the built QML import path,
// the token facade fed with the shipped qinda-dark theme, and the shipped
// icon runtime so icon buttons prove resolved glyphs. Returns false with
// `error` set on the first step that fails.
[[nodiscard]] inline bool prepareAudioPageEngine(QQuickView &view,
                                                 QString *error) {
  view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *view.engine(), error);
  if (facade == nullptr) {
    return false;
  }
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  if (!loaded.ok) {
    *error = loaded.error;
    return false;
  }
  if (!facade->publish(loaded.theme, {}, error)) {
    return false;
  }
  if (!QindaQt::Shell::Icons::IconRuntime::install(
          *view.engine(), {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
          {QStringLiteral("QindaQt")})) {
    *error = QStringLiteral("icon runtime install failed");
    return false;
  }
  return true;
}

// Instantiates the production AudioPage.qml against the duck-typed stub and
// attaches it to the view at `size`. The caller owns the model and reuses it
// for dispatch assertions. On failure the component's error string is printed
// and {nullptr, nullptr} returned.
[[nodiscard]] inline std::pair<std::unique_ptr<QObject>, QQuickItem *>
createAudioPage(QQuickView &view, StubAudioSettingsModel &model,
                const QSize size) {
  QQmlComponent component(view.engine());
  component.loadUrl(
      QUrl::fromLocalFile(QStringLiteral(QINDAQT_AUDIO_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("audioSettings"),
       QVariant::fromValue(static_cast<QObject *>(&model))},
  });
  if (object == nullptr) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  auto guard = std::unique_ptr<QObject>(object);
  auto *page = qobject_cast<QQuickItem *>(object);
  if (page == nullptr) {
    return {};
  }
  attach(view, *page, size);
  return {std::move(guard), page};
}

} // namespace QindaQt::Apps::SettingsAudio::TestSupport
