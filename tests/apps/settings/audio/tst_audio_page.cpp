// SPDX-License-Identifier: GPL-3.0-or-later

#include "stub_audio_settings_model.h"

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_audio/audio_settings_model.h>
#include <qindaqt/services/audio_protocol/audio_gain.h>
#include <qindaqt/shell/icons/icon_runtime.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtGui/QAccessibleInterface>
#include <QtCore/QMetaObject>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

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
  void rendersChannelStripsAndVirtualDevices();
  void routesDefaultVolumeMuteAndRetryIntents();
  void showsStaleTruthLabeledAndOwnerLossEmpty();
  void keepsCompactFocusVisibleWithoutAPageCloseAction();
  void disabledDefaultFallsThroughToFirstAdmittedAction();
  void supportsDocumentPagingKeys();
  void stubMatchesRealModelSurface();
  void consoleCardsShareOneGrid();
  void consoleFaderFollowsTheOneGainLaw();
  void consoleStripControlsDispatch();
  void consoleBusControlsDispatch();

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
  // The virtual-device section renders real iconography; the harness
  // resolves the shipped icon theme so the buttons prove resolved glyphs.
  QVERIFY2(QindaQt::Shell::Icons::IconRuntime::install(
               *m_view->engine(),
               {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
               {QStringLiteral("QindaQt")}),
           "icon runtime install");
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

// The console is read across a row like a real desk, so every card must put
// its meter, fader and pads at the same height as its neighbour's. This row
// exists because they did not: a virtual strip hid the device picker a
// hardware strip carries, which lifted its whole desk band, and buses put
// their picker at the bottom while strips put it at the top. The band heights
// in AudioConsoleStrip.qml and AudioConsoleBus.qml are shared verbatim; this
// fails if they ever drift apart again.
void AudioPageTest::consoleCardsShareOneGrid() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);

  struct CardUnderTest {
    const char *card;
    const char *fader;
    const char *meter;
  };
  // A hardware strip and a virtual strip (the pair that misaligned), then a
  // physical bus and a virtual bus.
  const CardUnderTest cards[] = {
      {"consoleStrip_strip.hw.1", "consoleStripFader_strip.hw.1",
       "consoleStripMeter_strip.hw.1"},
      {"consoleStrip_strip.virtual.1", "consoleStripFader_strip.virtual.1",
       "consoleStripMeter_strip.virtual.1"},
      {"consoleBus_bus.a1", "consoleBusFader_bus.a1", "consoleBusMeter_bus.a1"},
      {"consoleBus_bus.b1", "consoleBusFader_bus.b1", "consoleBusMeter_bus.b1"},
  };

  qreal sharedFaderOffset = -1.0;
  qreal sharedCardHeight = -1.0;
  for (const CardUnderTest &entry : cards) {
    QQuickItem *card = findItem(page, QString::fromLatin1(entry.card));
    QQuickItem *fader = findItem(page, QString::fromLatin1(entry.fader));
    QQuickItem *meter = findItem(page, QString::fromLatin1(entry.meter));
    QVERIFY2(card != nullptr, entry.card);
    QVERIFY2(fader != nullptr, entry.fader);
    QVERIFY2(meter != nullptr, entry.meter);

    const qreal faderOffset = fader->mapToItem(card, QPointF(0.0, 0.0)).y();
    const qreal meterOffset = meter->mapToItem(card, QPointF(0.0, 0.0)).y();
    QCOMPARE(meterOffset, faderOffset);
    // The desk band height is the one number both files state.
    QCOMPARE(fader->height(), 150.0);

    if (sharedFaderOffset < 0.0) {
      sharedFaderOffset = faderOffset;
      sharedCardHeight = card->height();
      continue;
    }
    QCOMPARE(faderOffset, sharedFaderOffset);
    QCOMPARE(card->height(), sharedCardHeight);
  }
  QVERIFY(sharedFaderOffset > 0.0);
}

// The console is operated in decibels, and ADR-0171 admits exactly one
// mapping between the fader's travel and dB: the model's. This row proves the
// QML never re-derives that mapping — the printed scale, the ticks and the
// readout all read the model's own conversion functions.
void AudioPageTest::consoleFaderFollowsTheOneGainLaw() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);
  auto *fader = findItem(page, QStringLiteral("consoleStripFader_strip.hw.1"));
  QVERIFY(fader != nullptr);
  QVERIFY(fader->isEnabled());

  // The readout must be the model's conversion of the live position, never a
  // number the QML computed on its own.
  const qreal live = fader->property("livePosition").toReal();
  const double gainDb =
      QindaQt::Audio::gainDbFromFaderPosition(live);
  const QString expectedReadout =
      (gainDb > 0 ? QStringLiteral("+") : QStringLiteral())
      + QString::number(gainDb, 'f', 1) + QStringLiteral(" dB");
  QCOMPARE(fader->property("readout").toString(), expectedReadout);
  auto *readout = findItem(page, QStringLiteral("consoleFaderReadout"));
  QVERIFY(readout != nullptr);
  QCOMPARE(readout->property("text").toString(), expectedReadout);

  // Every printed scale label sits where the model's gain law puts its gain.
  const qreal topY = fader->property("topY").toReal();
  const qreal travel = fader->property("travel").toReal();
  const struct {
    double db;
    const char *objectName;
  } labels[] = {
      {12.0, "consoleFaderScaleLabel_p12"}, {0.0, "consoleFaderScaleLabel_0"},
      {-12.0, "consoleFaderScaleLabel_m12"}, {-24.0, "consoleFaderScaleLabel_m24"},
      {-36.0, "consoleFaderScaleLabel_m36"}, {-48.0, "consoleFaderScaleLabel_m48"},
      {-60.0, "consoleFaderScaleLabel_m60"},
  };
  for (const auto &label : labels) {
    auto *item = findItem(page, QString::fromLatin1(label.objectName));
    QVERIFY2(item != nullptr, label.objectName);
    const qreal centre = item->property("y").toReal()
                         + item->property("height").toReal() / 2.0;
    const qreal expected = topY
        + (1.0 - QindaQt::Audio::faderPositionFromGainDb(label.db)) * travel;
    QVERIFY2(qAbs(centre - expected) < 0.51, label.objectName);
  }

  // Keyboard parity: the fader takes focus and steps, and Home returns to
  // unity — every step dispatched through the model as a POSITION.
  fader->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), fader);
  auto *faderAccessible = QAccessible::queryAccessibleInterface(fader);
  QVERIFY(faderAccessible != nullptr);
  QCOMPARE(faderAccessible->role(), QAccessible::Slider);

  QTest::keyClick(m_view.get(), Qt::Key_Up);
  QTRY_VERIFY(m_model->lastFaderPosition > 0.0);
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  QVERIFY(qAbs(m_model->lastFaderPosition - (live + 0.02)) < 1e-9);

  QTest::keyClick(m_view.get(), Qt::Key_Home);
  QTRY_VERIFY(qAbs(m_model->lastFaderPosition
                   - QindaQt::Audio::unityFaderPosition()) < 1e-9);
}

// Strip-face controls dispatch their console intents: the pan dial, the
// mute lamp, and a routing-bank send lamp. These existed in the projection
// but pan had no surface at all before the rebuild.
void AudioPageTest::consoleStripControlsDispatch() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);

  auto *pan = findItem(page, QStringLiteral("consoleStripPan_strip.hw.1"));
  QVERIFY(pan != nullptr);
  QVERIFY(pan->isEnabled());
  pan->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), pan);
  auto *panAccessible = QAccessible::queryAccessibleInterface(pan);
  QVERIFY(panAccessible != nullptr);
  QCOMPARE(panAccessible->role(), QAccessible::Slider);
  QTest::keyClick(m_view.get(), Qt::Key_Up);
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  // Pan runs -1..+1 in 40 steps (ADR-0177 balance).
  QVERIFY(qAbs(m_model->lastFaderPosition - 0.05) < 1e-9);

  auto *mute = findItem(page, QStringLiteral("consoleStripMute_strip.hw.1"));
  QVERIFY(mute != nullptr);
  QVERIFY(mute->isEnabled());
  mute->setProperty("checked", true);
  QVERIFY(QMetaObject::invokeMethod(mute, "toggled"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  QCOMPARE(m_model->lastConsoleFlag, true);

  // A routing-bank lamp toggles its send and keeps the send's dialled gain.
  auto *send = findItem(page, QStringLiteral("consoleSend_strip.hw.1_0"));
  QVERIFY(send != nullptr);
  QVERIFY(send->isEnabled());
  send->setProperty("checked", false);
  QVERIFY(QMetaObject::invokeMethod(send, "toggled"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("strip.hw.1"));
  QCOMPARE(m_model->lastBusIndex, 0);
  QCOMPARE(m_model->lastConsoleFlag, false);
}

// Bus-face controls dispatch their console intents: a channel-mode lamp, the
// mute lamp, and the bus fader's keyboard travel.
void AudioPageTest::consoleBusControlsDispatch() {
  auto [guard, page] = createPage(QSize(1100, 900));
  QVERIFY(page != nullptr);

  auto *swap = findItem(page, QStringLiteral("consoleBusMode_bus.a1_swap"));
  QVERIFY(swap != nullptr);
  QVERIFY(swap->isEnabled());
  QVERIFY(QMetaObject::invokeMethod(swap, "clicked"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("bus.a1"));
  QCOMPARE(m_model->lastProcessing.value(QStringLiteral("mode")).toString(),
           QStringLiteral("swap"));

  auto *mute = findItem(page, QStringLiteral("consoleBusMute_bus.a1"));
  QVERIFY(mute != nullptr);
  mute->setProperty("checked", true);
  QVERIFY(QMetaObject::invokeMethod(mute, "toggled"));
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("bus.a1"));
  QCOMPARE(m_model->lastConsoleFlag, true);

  auto *fader = findItem(page, QStringLiteral("consoleBusFader_bus.b1"));
  QVERIFY(fader != nullptr);
  fader->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), fader);
  QTest::keyClick(m_view.get(), Qt::Key_End);
  QTRY_VERIFY(m_model->lastFaderPosition >= 0.0);
  QCOMPARE(m_model->lastConsoleId, QStringLiteral("bus.b1"));
  QCOMPARE(m_model->lastFaderPosition, 0.0);
}

QTEST_MAIN(AudioPageTest)
#include "tst_audio_page.moc"
