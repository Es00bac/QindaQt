// SPDX-License-Identifier: GPL-3.0-or-later

// The Settings Audio route's surface tests: inventory presentation, intent
// dispatch, stale/owner-loss fail-closed behaviour, compact focus, and the
// stub's parity with the real model. The console grid's tests live in
// tst_audio_console_page.cpp; both suites share audio_page_test_support.h.

#include "audio_page_test_support.h"

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtCore/QMetaObject>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
using QindaQt::Apps::SettingsAudio::TestSupport::createAudioPage;
using QindaQt::Apps::SettingsAudio::TestSupport::findItem;
using QindaQt::Apps::SettingsAudio::TestSupport::prepareAudioPageEngine;

class AudioPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersInventoryAccessibly();
  void rendersChannelStripsAndVirtualDevices();
  void routesDefaultVolumeMuteAndRetryIntents();
  void showsStaleTruthLabeledAndOwnerLossEmpty();
  void keepsCompactFocusVisibleWithoutAPageCloseAction();
  void disabledDefaultFallsThroughToFirstAdmittedAction();
  void supportsDocumentPagingKeys();
  void compactTabsAndManualPeerKeyboardFlow();
  void stubMatchesRealModelSurface();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubAudioSettingsModel> m_model;

  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(const QSize size);
};

void AudioPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  QString error;
  QVERIFY2(prepareAudioPageEngine(*m_view, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
AudioPageTest::createPage(const QSize size) {
  m_model = std::make_unique<StubAudioSettingsModel>();
  return createAudioPage(*m_view, *m_model, size);
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

void AudioPageTest::rendersChannelStripsAndVirtualDevices() {
  auto [guard, page] = createPage(QSize(900, 760));
  QVERIFY(page != nullptr);

  // The channel disclosure exists for the six-channel fixture and stays
  // collapsed until opened; the strip then exposes one fader per channel.
  auto *channelsToggle =
      findItem(page, QStringLiteral("audioChannelsToggle_12"));
  QVERIFY(channelsToggle != nullptr);
  QVERIFY(channelsToggle->isVisible());
  QVERIFY(channelsToggle->isEnabled());
  QVERIFY(findItem(page, QStringLiteral("audioChannelVolume_12_2"))
          == nullptr);
  QVERIFY(QMetaObject::invokeMethod(channelsToggle, "clicked"));
  QCoreApplication::processEvents();
  auto *frontLeft = findItem(page, QStringLiteral("audioChannelVolume_12_0"));
  auto *center = findItem(page, QStringLiteral("audioChannelVolume_12_2"));
  auto *surroundText =
      findItem(page, QStringLiteral("audioChannelVolumeText_12_5"));
  QVERIFY(frontLeft != nullptr);
  QVERIFY(center != nullptr);
  QVERIFY(surroundText != nullptr);
  QVERIFY(center->isEnabled());
  QCOMPARE(surroundText->property("text").toString(), QStringLiteral("25%"));

  auto *centerAccessible = QAccessible::queryAccessibleInterface(center);
  QVERIFY(centerAccessible != nullptr);
  QCOMPARE(centerAccessible->role(), QAccessible::Slider);
  QVERIFY(centerAccessible->text(QAccessible::Description)
              .contains(QStringLiteral("FC channel volume")));

  center->setProperty("value", 0.4);
  QVERIFY(QMetaObject::invokeMethod(center, "moved"));
  QCOMPARE(m_model->channelSerial, qulonglong(12));
  QCOMPARE(m_model->channelIndex, 2);
  QVERIFY(m_model->channelLevel > 0.39);
  QVERIFY(m_model->channelLevel < 0.41);

  // The virtual-device section renders its managed inventory and never a
  // remove action for hardware rows.
  QVERIFY(findItem(page, QStringLiteral("audioVirtualAddOutput")) != nullptr);
  QVERIFY(findItem(page, QStringLiteral("audioVirtualAddInput")) != nullptr);
  QVERIFY(findItem(page, QStringLiteral("audioVirtualRemove_14")) != nullptr);
  QVERIFY(findItem(page, QStringLiteral("audioVirtualRemove_10"))
          == nullptr);
  QVERIFY(findItem(page, QStringLiteral("audioVirtualRemove_12"))
          == nullptr);

  auto *addOutput = findItem(page, QStringLiteral("audioVirtualAddOutput"));
  QVERIFY(addOutput->isEnabled());
  QVERIFY(QMetaObject::invokeMethod(addOutput, "clicked"));
  QCOMPARE(m_model->createCount, 1);
  QCOMPARE(m_model->createKindToken, QStringLiteral("output"));
  QCOMPARE(m_model->createDisplayName, QStringLiteral("Virtual output"));
  QCOMPARE(m_model->createChannels, 2);

  auto *addInput = findItem(page, QStringLiteral("audioVirtualAddInput"));
  QVERIFY(QMetaObject::invokeMethod(addInput, "clicked"));
  QCOMPARE(m_model->createCount, 2);
  QCOMPARE(m_model->createKindToken, QStringLiteral("input"));

  auto *removeBus = findItem(page, QStringLiteral("audioVirtualRemove_14"));
  QVERIFY(QMetaObject::invokeMethod(removeBus, "clicked"));
  QCOMPARE(m_model->removeVirtualSerial, qulonglong(14));

  // Without the capabilities the section's controls are disabled and the
  // channel disclosure disappears from every device row.
  m_model->canManageVirtualDevices = false;
  m_model->canSetChannelVolumes = false;
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  QVERIFY(!findItem(page, QStringLiteral("audioVirtualAddOutput"))
              ->isEnabled());
  QVERIFY(!findItem(page, QStringLiteral("audioVirtualAddInput"))
              ->isEnabled());
  QVERIFY(!findItem(page, QStringLiteral("audioVirtualRemove_14"))
              ->isEnabled());
  auto *hiddenToggle =
      findItem(page, QStringLiteral("audioChannelsToggle_12"));
  QVERIFY(hiddenToggle == nullptr || !hiddenToggle->isVisible());
  QVERIFY(!findItem(page, QStringLiteral("audioChannelVolume_12_2"))
              ->isEnabled());
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

void AudioPageTest::keepsCompactFocusVisibleWithoutAPageCloseAction() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *entry = findItem(page, QStringLiteral("audioOutputVolume_10"));
  auto *setDefault =
      findItem(page, QStringLiteral("audioOutputDefault_12"));
  auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
  QVERIFY(entry != nullptr);
  QVERIFY(setDefault != nullptr);
  QVERIFY(findItem(page, QStringLiteral("audioCloseButton")) == nullptr);
  QVERIFY(viewport != nullptr);
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(), entry);

  entry->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), entry);
  setDefault->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), setDefault);
  QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
  auto *lastStreamVolume =
      findItem(page, QStringLiteral("audioStreamVolume_40"));
  QVERIFY(lastStreamVolume != nullptr);
  QVERIFY(lastStreamVolume->isEnabled());
  m_model->ready = false;
  m_model->unavailable = true;
  m_model->statusText = QStringLiteral("The audio service is unavailable.");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *retry = findItem(page, QStringLiteral("audioRetryButton"));
  QVERIFY(retry != nullptr);
  QVERIFY(retry->isVisible());
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
  // This case is about default/volume fallback only; clear the default
  // row's channel strip so its disclosure cannot absorb the entry focus.
  firstRow[QStringLiteral("channelVolumes")] = QVariantList{};
  firstRow[QStringLiteral("channelVolumeAvailable")] = false;
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

void AudioPageTest::compactTabsAndManualPeerKeyboardFlow() {
  auto [guard, page] = createPage(QSize(420, 320));
  QVERIFY(page != nullptr);
  auto *tabs = findItem(page, QStringLiteral("audioDestinationTabs"));
  auto *lastTab = findItem(tabs, QStringLiteral("tab_2"));
  auto *output = findItem(page, QStringLiteral("audioOutputDefault_12"));
  auto *sendName = findItem(page, QStringLiteral("audioPeerSendName"));
  auto *sendHost = findItem(page, QStringLiteral("audioPeerSendHost"));
  auto *sendSave = findItem(page, QStringLiteral("audioPeerSendSave"));
  auto *receiveName = findItem(page, QStringLiteral("audioPeerReceiveName"));
  auto *receiveSource = findItem(page, QStringLiteral("audioPeerReceiveSource"));
  auto *receiveOutput = findItem(page, QStringLiteral("audioPeerReceiveOutput"));
  auto *receiveSave = findItem(page, QStringLiteral("audioPeerReceiveSave"));
  QVERIFY(tabs && lastTab && output && sendName && sendHost && sendSave);
  QVERIFY(receiveName && receiveSource && receiveOutput && receiveSave);
  QVERIFY(lastTab->mapToItem(page, QPointF(lastTab->width(), 0)).x() <= page->width());
  QCOMPARE(page->property("activeTab").toInt(), 0);
  QVERIFY(output->isVisible());
  tabs->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), tabs);
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QTRY_COMPARE(page->property("activeTab").toInt(), 1);
  QVERIFY(!output->isVisible());
  QVERIFY(findItem(page, QStringLiteral("consoleStrip_strip.hw.1"))->isVisible());
  QTest::keyClick(m_view.get(), Qt::Key_Right);
  QTRY_COMPARE(page->property("activeTab").toInt(), 2);
  QVERIFY(sendName->isVisible());
  QCOMPARE(page->property("firstFocusTarget").value<QObject *>(),
           static_cast<QObject *>(findItem(page, QStringLiteral("audioPeerCodeSender"))));
  sendName->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), sendName);
  sendName->setProperty("text", QStringLiteral("Desk"));
  sendHost->setProperty("text", QStringLiteral("192.0.2.20"));
  QCoreApplication::processEvents();
  QVERIFY(sendSave->property("available").toBool());
  QVERIFY(QMetaObject::invokeMethod(sendSave, "clicked"));
  QCOMPARE(m_model->savedPeerName, QStringLiteral("Desk"));
  QCOMPARE(m_model->savedPeerBus, QStringLiteral("bus.a2"));
  QCOMPARE(m_model->savedPeerHost, QStringLiteral("192.0.2.20"));
  QCOMPARE(m_model->savedPeerPort, 6980);
  QVERIFY(m_model->savedPeerOutgoing);
  receiveName->setProperty("text", QStringLiteral("DeskRx"));
  receiveSource->setProperty("text", QStringLiteral("192.0.2.10"));
  receiveOutput->setProperty("currentIndex", 0);
  QCoreApplication::processEvents();
  QVERIFY(receiveSave->property("available").toBool());
  receiveSave->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), receiveSave);
  auto *viewport = findItem(page, QStringLiteral("audioFormViewport"));
  QVERIFY(viewport != nullptr);
  QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
  QVERIFY(QMetaObject::invokeMethod(receiveSave, "clicked"));
  QCOMPARE(m_model->savedPeerName, QStringLiteral("DeskRx"));
  QCOMPARE(m_model->savedPeerHost, QStringLiteral("192.0.2.10"));
  QCOMPARE(m_model->savedPeerOutput, QStringLiteral("alsa_output.desk"));
  QVERIFY(!m_model->savedPeerOutgoing);

  // A fresh authoritative row repopulates the editor and exposes the
  // local-graph-only state. It remains reachable in the compact viewport.
  m_model->consoleVban = {QVariantMap{
      {QStringLiteral("name"), QStringLiteral("DeskRx")},
      {QStringLiteral("outgoing"), false},
      {QStringLiteral("busId"), QString()},
      {QStringLiteral("host"), QStringLiteral("192.0.2.10")},
      {QStringLiteral("port"), 6980},
      {QStringLiteral("enabled"), true},
      {QStringLiteral("active"), false},
      {QStringLiteral("outputNodeName"), QStringLiteral("alsa_output.desk")}}};
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  auto *edit = findItem(page, QStringLiteral("audioPeerEdit_DeskRx"));
  auto *enable = findItem(page, QStringLiteral("audioPeerEnable_DeskRx"));
  auto *peerState = findItem(page, QStringLiteral("audioPeerState_DeskRx"));
  QVERIFY(edit && enable && peerState);
  QVERIFY(peerState->property("text").toString().contains(QStringLiteral("waiting")));
  receiveName->setProperty("text", QString());
  QVERIFY(QMetaObject::invokeMethod(edit, "clicked"));
  QCOMPARE(receiveName->property("text").toString(), QStringLiteral("DeskRx"));
  QCOMPARE(receiveSource->property("text").toString(), QStringLiteral("192.0.2.10"));
  QCOMPARE(receiveOutput->property("currentIndex").toInt(), 0);
  QVERIFY(QMetaObject::invokeMethod(enable, "clicked"));
  QCOMPARE(m_model->lastVbanName, QStringLiteral("DeskRx"));
  QVERIFY(!m_model->lastVbanEnabled);
  m_model->consoleVban[0] = QVariantMap{
      {QStringLiteral("name"), QStringLiteral("DeskRx")},
      {QStringLiteral("outgoing"), false},
      {QStringLiteral("busId"), QString()},
      {QStringLiteral("host"), QStringLiteral("192.0.2.10")},
      {QStringLiteral("port"), 6980},
      {QStringLiteral("enabled"), true},
      {QStringLiteral("active"), true},
      {QStringLiteral("outputNodeName"), QStringLiteral("alsa_output.desk")}};
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  QVERIFY(peerState->property("text").toString().contains(QStringLiteral("unconfirmed")));

  m_model->canManagePeerStreams = false;
  m_model->errorText = QStringLiteral("Receiver output unavailable");
  Q_EMIT m_model->viewChanged();
  QCoreApplication::processEvents();
  QVERIFY(findItem(page, QStringLiteral("audioPeerUnavailable"))->isVisible());
  QVERIFY(!receiveSave->property("available").toBool());
  QVERIFY(!enable->property("available").toBool());
  QCOMPARE(findItem(page, QStringLiteral("audioError"))->property("text").toString(),
           QStringLiteral("Receiver output unavailable"));
  tabs->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(m_view.get(), Qt::Key_Left);
  QTRY_COMPARE(page->property("activeTab").toInt(), 1);
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
