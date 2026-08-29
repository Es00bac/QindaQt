// SPDX-License-Identifier: GPL-3.0-or-later

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

namespace {

const char *const BuildQmlImportPath = QINDAQT_QML_IMPORT_PATH;
const char *const DisplayPageQmlPath = QINDAQT_DISPLAY_PAGE_QML_PATH;

class StubDisplayModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY stateChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY stateChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY stateChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY stateChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY stateChanged)
  Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY stateChanged)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY stateChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY stateChanged)

  Q_PROPERTY(QVariantList outputs MEMBER outputs NOTIFY outputsChanged)
  Q_PROPERTY(QString selectedOutputId MEMBER selectedOutputId WRITE setSelectedOutputId NOTIFY selectedOutputIdChanged)
  Q_PROPERTY(QVariantMap selectedOutput MEMBER selectedOutput NOTIFY selectedOutputChanged)

  Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY draftChanged)
  Q_PROPERTY(bool draftValid MEMBER draftValid NOTIFY draftChanged)
  Q_PROPERTY(QString draftErrorMessage MEMBER draftErrorMessage NOTIFY draftChanged)
  Q_PROPERTY(QVariantMap fieldErrors MEMBER fieldErrors NOTIFY draftChanged)
  Q_PROPERTY(QVariantList warnings MEMBER warnings NOTIFY draftChanged)
  Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY stateChanged)

  Q_PROPERTY(bool inTransaction MEMBER inTransaction NOTIFY transactionChanged)
  Q_PROPERTY(bool awaitingConfirmation MEMBER awaitingConfirmation NOTIFY transactionChanged)
  Q_PROPERTY(int transactionRemainingSeconds MEMBER transactionRemainingSeconds NOTIFY transactionCountdownChanged)
  Q_PROPERTY(QString transactionStatusText MEMBER transactionStatusText NOTIFY transactionChanged)
  Q_PROPERTY(QString activeTransactionId MEMBER activeTransactionId NOTIFY transactionChanged)

public:
  bool loading = false;
  bool ready = true;
  bool busy = false;
  bool unavailable = false;
  bool degraded = false;
  bool canEdit = true;
  QString statusText;
  QString errorText;

  QVariantList outputs;
  QString selectedOutputId = QStringLiteral("edid:dp1");
  QVariantMap selectedOutput;

  bool draftDirty = false;
  bool draftValid = true;
  QString draftErrorMessage;
  QVariantMap fieldErrors;
  QVariantList warnings;
  bool applyAvailable = false;

  bool inTransaction = false;
  bool awaitingConfirmation = false;
  int transactionRemainingSeconds = 15;
  QString transactionStatusText;
  QString activeTransactionId;

  int appliedCount = 0;
  int canceledCount = 0;
  int confirmedCount = 0;
  int revertedCount = 0;
  int retriedCount = 0;

  explicit StubDisplayModel(QObject *parent = nullptr) : QObject(parent) {
    setupDefaultOutputs();
  }

  void setupDefaultOutputs() {
    QVariantMap mode1{
        {QStringLiteral("id"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("label"), QStringLiteral("3840 × 2160 @ 60 Hz")},
        {QStringLiteral("preferred"), true},
    };
    QVariantMap mode2{
        {QStringLiteral("id"), QStringLiteral("1920x1080@60")},
        {QStringLiteral("label"), QStringLiteral("1920 × 1080 @ 60 Hz")},
        {QStringLiteral("preferred"), false},
    };
    QVariantList modes{mode1, mode2};

    selectedOutput = {
        {QStringLiteral("stableId"), QStringLiteral("edid:dp1")},
        {QStringLiteral("connectorName"), QStringLiteral("DP-1")},
        {QStringLiteral("label"), QStringLiteral("Main Monitor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("primary"), true},
        {QStringLiteral("modeId"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("positionX"), 0},
        {QStringLiteral("positionY"), 0},
        {QStringLiteral("logicalWidth"), 1920},
        {QStringLiteral("logicalHeight"), 1080},
        {QStringLiteral("scale"), 2.0},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("modes"), modes},
    };

    outputs = {selectedOutput};
    outputsMap[QStringLiteral("edid:dp1")] = selectedOutput;
    baselineOutputsMap = outputsMap;
  }

  QMap<QString, QVariantMap> outputsMap;
  QMap<QString, QVariantMap> baselineOutputsMap;

  void setupTwoOutputs() {
    QVariantMap mode1{
        {QStringLiteral("id"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("label"), QStringLiteral("3840 × 2160 @ 60 Hz")},
        {QStringLiteral("preferred"), true},
    };
    QVariantList modes{mode1};

    QVariantMap out1{
        {QStringLiteral("stableId"), QStringLiteral("edid:dp1")},
        {QStringLiteral("connectorName"), QStringLiteral("DP-1")},
        {QStringLiteral("label"), QStringLiteral("Main Monitor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("primary"), true},
        {QStringLiteral("modeId"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("positionX"), 0},
        {QStringLiteral("positionY"), 0},
        {QStringLiteral("logicalWidth"), 1920},
        {QStringLiteral("logicalHeight"), 1080},
        {QStringLiteral("scale"), 2.0},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("modes"), modes},
    };

    QVariantMap out2{
        {QStringLiteral("stableId"), QStringLiteral("edid:hdmi1")},
        {QStringLiteral("connectorName"), QStringLiteral("HDMI-1")},
        {QStringLiteral("label"), QStringLiteral("Side Monitor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("primary"), false},
        {QStringLiteral("modeId"), QStringLiteral("1920x1080@60")},
        {QStringLiteral("positionX"), 1920},
        {QStringLiteral("positionY"), 0},
        {QStringLiteral("logicalWidth"), 1920},
        {QStringLiteral("logicalHeight"), 1080},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("modes"), modes},
    };

    outputsMap.clear();
    outputsMap[QStringLiteral("edid:dp1")] = out1;
    outputsMap[QStringLiteral("edid:hdmi1")] = out2;
    baselineOutputsMap = outputsMap;

    outputs = {out1, out2};
    selectedOutputId = QStringLiteral("edid:dp1");
    selectedOutput = out1;
  }

  void setSelectedOutputId(const QString &id) {
    selectedOutputId = id;
    if (outputsMap.contains(id)) {
      selectedOutput = outputsMap.value(id);
    }
    Q_EMIT selectedOutputIdChanged(id);
    Q_EMIT selectedOutputChanged();
  }

  Q_INVOKABLE bool setOutputEnabled(const QString &stableId, bool enabled) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("enabled")] = enabled;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputPrimary(const QString &stableId) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("primary")] = true;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputMode(const QString &stableId, const QString &modeId) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("modeId")] = modeId;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputScale(const QString &stableId, double scale) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("scale")] = scale;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputTransform(const QString &stableId, const QString &t) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("transform")] = t;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputPosition(const QString &stableId, int x, int y) {
    if (outputsMap.contains(stableId)) {
      auto map = outputsMap.value(stableId);
      map[QStringLiteral("positionX")] = x;
      map[QStringLiteral("positionY")] = y;
      outputsMap[stableId] = map;
      if (selectedOutputId == stableId) {
        selectedOutput = map;
      }
    } else {
      selectedOutput[QStringLiteral("positionX")] = x;
      selectedOutput[QStringLiteral("positionY")] = y;
    }
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool applyDraft() {
    ++appliedCount;
    return true;
  }

  Q_INVOKABLE bool cancelDraft() {
    ++canceledCount;
    draftDirty = false;
    applyAvailable = false;
    outputsMap = baselineOutputsMap;
    if (outputsMap.contains(selectedOutputId)) {
      selectedOutput = outputsMap.value(selectedOutputId);
    }
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool confirmTransaction() {
    ++confirmedCount;
    return true;
  }

  Q_INVOKABLE bool revertTransaction() {
    ++revertedCount;
    return true;
  }

  Q_INVOKABLE void retry() {
    ++retriedCount;
  }

Q_SIGNALS:
  void stateChanged();
  void outputsChanged();
  void selectedOutputIdChanged(const QString &selectedOutputId);
  void selectedOutputChanged();
  void draftChanged();
  void transactionChanged();
  void transactionCountdownChanged();
};

QQuickItem *findItemByObjectName(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems()) {
    if (auto *match = findItemByObjectName(child, name); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

} // namespace

class DisplayPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void testPageRenderingAndControls();
  void testScaleAndOrientationInteraction();
  void testArrangementPositionSynchronizationOnSwitchAndRevert();
  void testUnavailableNoticeAndRetry();
  void testPreviewBannerAndTransactionActions();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubDisplayModel> m_model;
};

void DisplayPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QString::fromUtf8(BuildQmlImportPath));

  QString facadeError;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));

  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QString pubError;
  QVERIFY2(facade->publish(loaded.theme, {}, &pubError), qPrintable(pubError));

  m_model = std::make_unique<StubDisplayModel>();
}

void DisplayPageTest::testPageRenderingAndControls() {
  m_model->setupDefaultOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *heading = findItemByObjectName(pageItem, QStringLiteral("displayPageHeading"));
  QVERIFY(heading != nullptr);

  auto *applyBtn = findItemByObjectName(pageItem, QStringLiteral("displayApplyButton"));
  QVERIFY(applyBtn != nullptr);
  QCOMPARE(applyBtn->property("available").toBool(), false);

  auto *closeBtn = findItemByObjectName(pageItem, QStringLiteral("displayCloseButton"));
  QVERIFY(closeBtn != nullptr);
}

void DisplayPageTest::testScaleAndOrientationInteraction() {
  m_model->setupDefaultOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  // Find 100% scale button and click it
  auto *scaleBtn100 = findItemByObjectName(pageItem, QStringLiteral("displayScaleButton_100"));
  QVERIFY(scaleBtn100 != nullptr);
  QMetaObject::invokeMethod(scaleBtn100, "clicked");

  QCOMPARE(m_model->selectedOutput.value(QStringLiteral("scale")).toDouble(), 1.0);
  QVERIFY(m_model->draftDirty);

  // Check apply button is now available
  auto *applyBtn = findItemByObjectName(pageItem, QStringLiteral("displayApplyButton"));
  QVERIFY(applyBtn != nullptr);
  QCOMPARE(applyBtn->property("available").toBool(), true);

  QMetaObject::invokeMethod(applyBtn, "clicked");
  QCOMPARE(m_model->appliedCount, 1);
}

void DisplayPageTest::testArrangementPositionSynchronizationOnSwitchAndRevert() {
  m_model->setupTwoOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = false;

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *posXField = findItemByObjectName(pageItem, QStringLiteral("displayPosXField"));
  auto *posYField = findItemByObjectName(pageItem, QStringLiteral("displayPosYField"));
  QVERIFY(posXField != nullptr);
  QVERIFY(posYField != nullptr);

  // 1. Initial output DP-1 position is (0, 0)
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("0"));
  QCOMPARE(posYField->property("text").toString(), QStringLiteral("0"));

  // 2. Simulate user typing a new X position into posXField
  posXField->setProperty("text", QStringLiteral("500"));
  m_model->setOutputPosition(QStringLiteral("edid:dp1"), 500, 0);
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("500"));

  // 3. Switch selected output to HDMI-1 (position (1920, 0))
  // The focus-safe synchronization must immediately refresh posXField text to "1920"
  m_model->setSelectedOutputId(QStringLiteral("edid:hdmi1"));
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("1920"));
  QCOMPARE(posYField->property("text").toString(), QStringLiteral("0"));

  // 4. Switch back to DP-1 (which has drafted position 500)
  m_model->setSelectedOutputId(QStringLiteral("edid:dp1"));
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("500"));

  // 5. Cancel / revert draft - restores baseline position (0, 0)
  m_model->cancelDraft();
  QCOMPARE(posXField->property("text").toString(), QStringLiteral("0"));
  QCOMPARE(posYField->property("text").toString(), QStringLiteral("0"));
}

void DisplayPageTest::testUnavailableNoticeAndRetry() {
  m_model->unavailable = true;
  m_model->statusText = QStringLiteral("Service unavailable");
  m_model->outputs.clear();
  m_model->selectedOutput.clear();
  Q_EMIT m_model->stateChanged();

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *notice = findItemByObjectName(pageItem, QStringLiteral("displayUnavailableNotice"));
  QVERIFY(notice != nullptr);
  QVERIFY(notice->isVisible());

  auto *retryBtn = findItemByObjectName(pageItem, QStringLiteral("displayRetryButton"));
  QVERIFY(retryBtn != nullptr);
  QVERIFY(retryBtn->isVisible());

  QMetaObject::invokeMethod(retryBtn, "clicked");
  QCOMPARE(m_model->retriedCount, 1);
}

void DisplayPageTest::testPreviewBannerAndTransactionActions() {
  m_model->setupDefaultOutputs();
  m_model->unavailable = false;
  m_model->inTransaction = true;
  m_model->awaitingConfirmation = true;
  m_model->transactionStatusText = QStringLiteral("Preview active. Reverting in 15s");
  Q_EMIT m_model->transactionChanged();

  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(DisplayPageQmlPath)));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));

  QObject *pageObj = component.createWithInitialProperties({
      {QStringLiteral("displaySettings"),
       QVariant::fromValue(static_cast<QObject *>(m_model.get()))},
  });
  QVERIFY(pageObj != nullptr);
  std::unique_ptr<QObject> pageGuard(pageObj);

  auto *pageItem = qobject_cast<QQuickItem *>(pageObj);
  QVERIFY(pageItem != nullptr);

  auto *previewBanner = findItemByObjectName(pageItem, QStringLiteral("displayPreviewBanner"));
  QVERIFY(previewBanner != nullptr);
  QVERIFY(previewBanner->isVisible());

  auto *keepBtn = findItemByObjectName(pageItem, QStringLiteral("displayPreviewKeepButton"));
  QVERIFY(keepBtn != nullptr);
  QMetaObject::invokeMethod(keepBtn, "clicked");
  QCOMPARE(m_model->confirmedCount, 1);

  auto *revertBtn = findItemByObjectName(pageItem, QStringLiteral("displayPreviewRevertButton"));
  QVERIFY(revertBtn != nullptr);
  QMetaObject::invokeMethod(revertBtn, "clicked");
  QCOMPARE(m_model->revertedCount, 1);
}

QTEST_MAIN(DisplayPageTest)
#include "tst_display_page.moc"
