// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_audio_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_audio/audio_settings_model.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtCore/QMetaObject>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;

namespace {

QQuickItem *findItem(QQuickItem *root, const QString &objectName) {
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

void attach(QQuickView &view, QQuickItem &page, const QSize size) {
  view.resize(size);
  page.setParentItem(view.contentItem());
  page.setSize(size);
  view.show();
  QCoreApplication::processEvents();
}

} // namespace

class AudioPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersInventoryAccessibly();
  void routesDefaultVolumeMuteAndRetryIntents();
  void showsStaleTruthLabeledAndOwnerLossEmpty();
  void keepsCompactFocusVisibleAndClosesTheCycle();
  void disabledDefaultFallsThroughToFirstAdmittedAction();
  void supportsDocumentPagingKeys();
  void stubMatchesRealModelSurface();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubAudioSettingsModel> m_model;

  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(const QSize size);
};

void AudioPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QString publishError;
  QVERIFY2(facade->publish(loaded.theme, {}, &publishError),
           qPrintable(publishError));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
AudioPageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubAudioSettingsModel>();
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_AUDIO_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("audioSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
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
  attach(*m_view, *page, size);
  return {std::move(guard), page};
}

void AudioPageTest::rendersInventoryAccessibly() {
  auto [guard, page] = createPage(QSize(900, 760));
  QVERIFY(page != nullptr);
  QVERIFY(findItem(page, QStringLiteral("audioPageHeading")) != nullptr);
  auto *state = findItem(page, QStringLiteral("audioServiceState"));
  auto *setDefault =
      findItem(page, QStringLiteral("audioOutputDefault_12"));
  auto *volume = findItem(page, QStringLiteral("audioOutputVolume_10"));
  auto *volumeText =
      findItem(page, QStringLiteral("audioOutputVolumeText_10"));
  auto *mute = findItem(page, QStringLiteral("audioInputMute_20"));
  auto *streamVolume =
      findItem(page, QStringLiteral("audioStreamVolume_30"));
  auto *streamMute =
      findItem(page, QStringLiteral("audioStreamMute_40"));
  QVERIFY(state != nullptr);
  QVERIFY(setDefault != nullptr);
  QVERIFY(volume != nullptr);
  QVERIFY(volumeText != nullptr);
  QVERIFY(mute != nullptr);
  QVERIFY(streamVolume != nullptr);
  QVERIFY(streamMute != nullptr);
  QVERIFY(setDefault->isEnabled());
  QVERIFY(volume->isEnabled());
  QVERIFY(mute->isEnabled());
  QCOMPARE(volumeText->property("text").toString(), QStringLiteral("50%"));

  // The already-default output keeps its inventory row but exposes no
  // visible second "set default" surface.
  auto *defaultTen =
      findItem(page, QStringLiteral("audioOutputDefault_10"));
  QVERIFY(defaultTen == nullptr || !defaultTen->isVisible());

  auto *setDefaultAccessible =
      QAccessible::queryAccessibleInterface(setDefault);
  auto *volumeAccessible = QAccessible::queryAccessibleInterface(volume);
  QVERIFY(setDefaultAccessible != nullptr);
  QVERIFY(volumeAccessible != nullptr);
  QCOMPARE(setDefaultAccessible->role(), QAccessible::Button);
  QCOMPARE(volumeAccessible->role(), QAccessible::Slider);
  QVERIFY(setDefaultAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("default")));
}

void AudioPageTest::routesDefaultVolumeMuteAndRetryIntents() {
  auto [guard, page] = createPage(QSize(900, 760));
  QVERIFY(page != nullptr);
  auto *setDefault =
      findItem(page, QStringLiteral("audioOutputDefault_12"));
  auto *volume = findItem(page, QStringLiteral("audioOutputVolume_12"));
  auto *mute = findItem(page, QStringLiteral("audioInputMute_20"));
  auto *streamVolume =
      findItem(page, QStringLiteral("audioStreamVolume_30"));
  auto *streamMute =
      findItem(page, QStringLiteral("audioStreamMute_40"));
  auto *retry = findItem(page, QStringLiteral("audioRetryButton"));
  QVERIFY(setDefault != nullptr);
  QVERIFY(volume != nullptr);
  QVERIFY(mute != nullptr);
  QVERIFY(streamVolume != nullptr);
  QVERIFY(streamMute != nullptr);

  volume->setProperty("value", 0.6);
  QVERIFY(QMetaObject::invokeMethod(volume, "moved"));
  QCOMPARE(m_model->deviceVolumeSerial, qulonglong(12));
  QVERIFY(m_model->deviceVolumeLevel > 0.59);
  QVERIFY(m_model->deviceVolumeLevel < 0.61);

  QVERIFY(QMetaObject::invokeMethod(setDefault, "clicked"));
  QCOMPARE(m_model->defaultSerial, qulonglong(12));

  mute->setProperty("checked", true);
  QVERIFY(QMetaObject::invokeMethod(mute, "toggled"));
  QCOMPARE(m_model->deviceMuteSerial, qulonglong(20));
  QCOMPARE(m_model->deviceMuted, true);

  streamVolume->setProperty("value", 0.3);
  QVERIFY(QMetaObject::invokeMethod(streamVolume, "moved"));
  QCOMPARE(m_model->streamVolumeSerial, qulonglong(30));
  QVERIFY(m_model->streamVolumeLevel > 0.29);
  QVERIFY(m_model->streamVolumeLevel < 0.31);

  streamMute->setProperty("checked", false);
  QVERIFY(QMetaObject::invokeMethod(streamMute, "toggled"));
  QCOMPARE(m_model->streamMuteSerial, qulonglong(40));
  QCOMPARE(m_model->streamMuted, false);

  // The retry action appears exactly when truth is not ready.
  QVERIFY(retry == nullptr || !retry->isVisible());
  m_model->ready = false;
  m_model->unavailable = true;
  m_model->statusText = QStringLiteral("The audio service is unavailable.");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  retry = findItem(page, QStringLiteral("audioRetryButton"));
  QVERIFY(retry != nullptr);
  QVERIFY(retry->isVisible());
  QVERIFY(QMetaObject::invokeMethod(retry, "clicked"));
  QCOMPARE(m_model->reloadCount, 1);
}

void AudioPageTest::showsStaleTruthLabeledAndOwnerLossEmpty() {
  auto [guard, page] = createPage(QSize(900, 760));
  QVERIFY(page != nullptr);
  m_model->ready = false;
  m_model->stale = true;
  m_model->statusText =
      QStringLiteral("Audio information is stale while the service recovers.");
  auto *state = findItem(page, QStringLiteral("audioServiceState"));
  QVERIFY(state != nullptr);
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  QCOMPARE(state->property("status").toInt(), 2); // StateCard.Warning
  QVERIFY(findItem(page, QStringLiteral("audioOutputVolume_10")) != nullptr);

  m_model->stale = false;
  m_model->unavailable = true;
  m_model->serviceEpoch = 0;
  m_model->serviceRevision = 0;
  m_model->defaultOutputName = QString();
  m_model->defaultInputName = QString();
  m_model->outputDevices.clear();
  m_model->inputDevices.clear();
  m_model->streams.clear();
  m_model->statusText = QStringLiteral("The audio service is unavailable.");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  QCOMPARE(state->property("status").toInt(), 3); // StateCard.Error
  QVERIFY(findItem(page, QStringLiteral("audioOutputVolume_10")) == nullptr);
  QVERIFY(findItem(page, QStringLiteral("audioStreamVolume_30")) == nullptr);
}

void AudioPageTest::keepsCompactFocusVisibleAndClosesTheCycle() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *entry = findItem(page, QStringLiteral("audioOutputVolume_10"));
  auto *setDefault =
      findItem(page, QStringLiteral("audioOutputDefault_12"));
  auto *close = findItem(page, QStringLiteral("audioCloseButton"));
  auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
  QVERIFY(entry != nullptr);
  QVERIFY(setDefault != nullptr);
  QVERIFY(close != nullptr);
  QVERIFY(viewport != nullptr);
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), entry);

  entry->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), entry);
  setDefault->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), setDefault);
  QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
  close->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), close);
  QTest::keyClick(m_view.get(), Qt::Key_Tab);
  QTRY_COMPARE(m_view->activeFocusItem(), entry);

  // Reverse Tab from Close must reach the preceding enabled, admitted
  // control (the last stream row's volume) instead of looping on Close.
  auto *lastStreamVolume =
      findItem(page, QStringLiteral("audioStreamVolume_40"));
  QVERIFY(lastStreamVolume != nullptr);
  QVERIFY(lastStreamVolume->isEnabled());
  close->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), close);
  QTest::keyClick(m_view.get(), Qt::Key_Backtab);
  QTRY_COMPARE(m_view->activeFocusItem(), lastStreamVolume);

  // With Retry visible, reverse Tab from Close moves to Retry.
  m_model->ready = false;
  m_model->unavailable = true;
  m_model->statusText = QStringLiteral("The audio service is unavailable.");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *retry = findItem(page, QStringLiteral("audioRetryButton"));
  QVERIFY(retry != nullptr);
  QVERIFY(retry->isVisible());
  close->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), close);
  QTest::keyClick(m_view.get(), Qt::Key_Backtab);
  QTRY_COMPARE(m_view->activeFocusItem(), retry);
}

void AudioPageTest::disabledDefaultFallsThroughToFirstAdmittedAction() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);

  // Valid public Audio1 projection with an admitted action on a
  // non-default output while the default output admits none of its own
  // controls (canSetVolume/canSetMute false). Host entry must target the
  // first enabled, admitted control in traversal order, never the disabled
  // default-output slider.
  auto outputDevices = m_model->outputDevices;
  auto firstRow = outputDevices.at(0).toMap();
  firstRow[QStringLiteral("volumeAvailable")] = false;
  firstRow[QStringLiteral("muteAvailable")] = false;
  outputDevices[0] = firstRow;
  m_model->outputDevices = outputDevices;
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();

  auto *disabledVolume =
      findItem(page, QStringLiteral("audioOutputVolume_10"));
  auto *disabledMute = findItem(page, QStringLiteral("audioOutputMute_10"));
  auto *admittedDefault =
      findItem(page, QStringLiteral("audioOutputDefault_12"));
  QVERIFY(disabledVolume != nullptr);
  QVERIFY(disabledMute != nullptr);
  QVERIFY(admittedDefault != nullptr);
  QVERIFY(!disabledVolume->isEnabled());
  QVERIFY(!disabledMute->isEnabled());
  QVERIFY(admittedDefault->isEnabled());

  auto *target = page->property("firstFocusTarget").value<QQuickItem *>();
  QCOMPARE(target, admittedDefault);
  QVERIFY(target->isEnabled());
  target->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), target);

  // A later projection change that re-admits the default output's volume
  // must recompute the entry target back to traversal-first control.
  firstRow[QStringLiteral("volumeAvailable")] = true;
  outputDevices[0] = firstRow;
  m_model->outputDevices = outputDevices;
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  target = page->property("firstFocusTarget").value<QQuickItem *>();
  disabledVolume = findItem(page, QStringLiteral("audioOutputVolume_10"));
  QVERIFY(disabledVolume != nullptr);
  QCOMPARE(target, disabledVolume);
  QVERIFY(target->isEnabled());
}

void AudioPageTest::supportsDocumentPagingKeys() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *volume = findItem(page, QStringLiteral("audioOutputVolume_10"));
  auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
  QVERIFY(volume != nullptr);
  QVERIFY(viewport != nullptr);
  volume->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), volume);

  viewport->setProperty("contentY", 0.0);
  QTest::keyClick(m_view.get(), Qt::Key_PageDown);
  QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
  QTest::keyClick(m_view.get(), Qt::Key_End, Qt::ControlModifier);
  const qreal maximum = qMax(
      0.0, viewport->property("contentHeight").toReal()
               - viewport->property("height").toReal());
  QTRY_COMPARE(viewport->property("contentY").toReal(), maximum);
  QTest::keyClick(m_view.get(), Qt::Key_PageUp);
  QTRY_VERIFY(viewport->property("contentY").toReal() < maximum);
  QTest::keyClick(m_view.get(), Qt::Key_Home, Qt::ControlModifier);
  QTRY_COMPARE(viewport->property("contentY").toReal(), 0.0);
}

void AudioPageTest::stubMatchesRealModelSurface() {
  const auto propertySurface = [](const QMetaObject &meta) {
    QStringList surface;
    for (int index = meta.propertyOffset(); index < meta.propertyCount();
         ++index) {
      const QMetaProperty property = meta.property(index);
      surface.append(QString::fromLatin1(property.name()) + u':'
                     + QString::fromLatin1(property.typeName()));
    }
    surface.sort();
    return surface;
  };
  const auto invokableSurface = [](const QMetaObject &meta) {
    QStringList surface;
    for (int index = meta.methodOffset(); index < meta.methodCount(); ++index) {
      const QMetaMethod method = meta.method(index);
      if (method.methodType() == QMetaMethod::Method) {
        surface.append(QString::fromLatin1(method.methodSignature()));
      }
    }
    surface.sort();
    return surface;
  };

  QCOMPARE(propertySurface(
               QindaQt::Apps::SettingsAudio::AudioSettingsModel::staticMetaObject),
           propertySurface(StubAudioSettingsModel::staticMetaObject));
  QCOMPARE(invokableSurface(
               QindaQt::Apps::SettingsAudio::AudioSettingsModel::staticMetaObject),
           invokableSurface(StubAudioSettingsModel::staticMetaObject));
}

QTEST_MAIN(AudioPageTest)
#include "tst_audio_page.moc"
