// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_streaming/streaming_settings_model.h>
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/services/screenshot_preferences/settings1_screenshot_preferences.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/themes/theme_loader.h>

#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt;

namespace {
class FakeTransport final : public Obs::ObsTransport {
    Q_OBJECT
public:
    void open(const QString &) override {}
    void close() override {}
    void sendText(const QString &) override {}
    [[nodiscard]] bool isOpen() const override { return false; }
};
class FakeSecrets final : public Obs::ObsSecretStore {
    Q_OBJECT
public:
    [[nodiscard]] std::optional<QString> password(QString *error) const override {
        if (error) error->clear();
        return std::nullopt;
    }
    [[nodiscard]] bool setPassword(const QString &, QString *) override { return false; }
};
class RejectingPreferences final : public Services::StreamingPreferences::StreamingPreferences {
    Q_OBJECT
public:
    int requests = 0;
    QString status;
    [[nodiscard]] bool isLoaded() const override { return true; }
    [[nodiscard]] int webSocketPort() const override { return 4455; }
    [[nodiscard]] bool autoConnect() const override { return false; }
    [[nodiscard]] bool startObsAtLogin() const override { return false; }
    [[nodiscard]] QString writeStatusText() const override { return status; }
    bool setWebSocketPort(int) override { return reject(); }
    bool setAutoConnect(bool) override { return reject(); }
    bool setStartObsAtLogin(bool) override { return reject(); }
private:
    bool reject() {
        ++requests;
        status = QStringLiteral("Settings rejected the change.");
        Q_EMIT writeStatusChanged();
        return false;
    }
};
// A Settings1 transport that answers one snapshot and records commits, so
// the capture section binds to real Settings1ScreenshotPreferences.
class FakeSettingsTransport final : public Services::SettingsClient::SettingsTransport {
    Q_OBJECT
public:
    struct Request { quint64 token; QString owner; };
    QList<Request> snapshots;
    int commits = 0;
    bool start(QString *error) override { if (error) error->clear(); return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override {
        snapshots.append({token, owner});
    }
    void commit(quint64, const QString &, const QString &, quint64, const QVariantList &) override {
        ++commits;
    }
    void requestActivation() override {}
};
QVariantMap captureSnapshot() {
    using Services::SettingsProtocol::WireContract;
    const QVariantMap values{
        {QStringLiteral("services.screenshotFolder"), QStringLiteral("/srv/shots")},
        {QStringLiteral("services.screenshotFileNamePattern"), QStringLiteral("Shot_{date}")},
        {QStringLiteral("services.screenshotDefaultMode"), QStringLiteral("active-window")},
        {QStringLiteral("services.screenshotDelaySeconds"), 5},
        {QStringLiteral("services.screenshotShowResult"), true},
        {QStringLiteral("services.screenshotRecordFinish"), QStringLiteral("quiet")}};
    QVariantMap sources;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        sources.insert(it.key(), QStringLiteral("user-overrides"));
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(Services::SettingsProtocol::SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
            {QLatin1StringView(WireContract::FieldRevision), quint64(1)},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}
QQuickItem *findItem(QQuickItem *root, const QString &name) {
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    for (QQuickItem *child : root->childItems())
        if (auto *found = findItem(child, name)) return found;
    return nullptr;
}
} // namespace

class StreamingPageTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void rejectedMouseSwitchesRestoreConfirmedState();
    void captureSectionShowsConfirmedScreenshotPreferences();
};

void StreamingPageTest::rejectedMouseSwitchesRestoreConfirmedState() {
    QQuickView view;
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = Apps::SettingsAppearance::ensureTokenFacade(*view.engine(), &error);
    QVERIFY2(facade != nullptr, qPrintable(error));
    const auto theme = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QVERIFY2(facade->publish(theme.theme, {}, &error), qPrintable(error));
    FakeTransport transport;
    Obs::ObsClient client(transport);
    FakeSecrets secrets;
    RejectingPreferences preferences;
    QTemporaryDir root;
    Apps::SettingsStreaming::StreamingSettingsModel model(client, secrets, preferences,
                                                           root.path());
    QQmlComponent component(view.engine());
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(QINDAQT_STREAMING_PAGE_QML_PATH)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> guard(component.createWithInitialProperties({
        {QStringLiteral("streamingSettings"), QVariant::fromValue(static_cast<QObject *>(&model))},
    }));
    QVERIFY2(guard != nullptr, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(guard.get());
    QVERIFY(page != nullptr);
    view.resize(900, 900);
    page->setParentItem(view.contentItem());
    page->setSize(QSizeF(900, 900));
    view.show();
    QCoreApplication::processEvents();
    for (const QString &name : {QStringLiteral("streamingAutoConnectSwitch"),
                                QStringLiteral("streamingStartAtLoginSwitch")}) {
        auto *toggle = findItem(page, name);
        QVERIFY2(toggle != nullptr, qPrintable(name));
        QVERIFY(!toggle->property("checked").toBool());
        const QPointF center = toggle->mapToScene(
            QPointF(toggle->width() / 2, toggle->height() / 2));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, center.toPoint());
        QTRY_VERIFY(!toggle->property("checked").toBool());
    }
    QCOMPARE(preferences.requests, 2);
    auto *status = findItem(page, QStringLiteral("streamingPreferenceStatus"));
    QVERIFY(status != nullptr);
    QCOMPARE(status->property("text").toString(),
             QStringLiteral("Settings rejected the change."));
}

void StreamingPageTest::captureSectionShowsConfirmedScreenshotPreferences() {
    // ADR-0289: the Screenshot tool's preferences live on this page. The
    // section shows confirmed Settings1 values, and a toggle only requests a
    // write; it keeps the confirmed value until readback.
    QQuickView view;
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = Apps::SettingsAppearance::ensureTokenFacade(*view.engine(), &error);
    QVERIFY2(facade != nullptr, qPrintable(error));
    const auto theme = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QVERIFY2(facade->publish(theme.theme, {}, &error), qPrintable(error));
    FakeTransport transport;
    Obs::ObsClient client(transport);
    FakeSecrets secrets;
    RejectingPreferences preferences;
    QTemporaryDir root;
    Apps::SettingsStreaming::StreamingSettingsModel model(client, secrets, preferences, root.path());
    FakeSettingsTransport settingsTransport;
    Services::SettingsClient::SettingsClient settingsClient(
        settingsTransport, Services::ScreenshotPreferences::Settings1ScreenshotPreferences::scopedKeys(),
        {.requestTimeoutMilliseconds = 100, .debounceMilliseconds = 0, .retryMilliseconds = {10}});
    Services::ScreenshotPreferences::Settings1ScreenshotPreferences capture(settingsClient);
    QVERIFY(settingsClient.start());
    Q_EMIT settingsTransport.ownerChanged(QStringLiteral(":1.40"));
    QTRY_COMPARE(settingsTransport.snapshots.size(), 1);
    const auto request = settingsTransport.snapshots.takeFirst();
    Q_EMIT settingsTransport.snapshotReceived(request.token, request.owner, captureSnapshot());
    QTRY_VERIFY(capture.isLoaded());

    QQmlComponent component(view.engine());
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(QINDAQT_STREAMING_PAGE_QML_PATH)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> guard(component.createWithInitialProperties({
        {QStringLiteral("streamingSettings"), QVariant::fromValue(static_cast<QObject *>(&model))},
        {QStringLiteral("captureSettings"), QVariant::fromValue(static_cast<QObject *>(&capture))},
    }));
    QVERIFY2(guard != nullptr, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(guard.get());
    QVERIFY(page != nullptr);
    view.resize(900, 1400);
    page->setParentItem(view.contentItem());
    page->setSize(QSizeF(900, 1400));
    view.show();
    QTRY_VERIFY(findItem(page, QStringLiteral("streamingCaptureSection")) != nullptr);
    QCOMPARE(findItem(page, QStringLiteral("captureFolderField"))->property("text").toString(),
             QStringLiteral("/srv/shots"));
    QCOMPARE(findItem(page, QStringLiteral("captureModeBox"))->property("currentIndex").toInt(), 3);
    QCOMPARE(findItem(page, QStringLiteral("captureDelayBox"))->property("currentIndex").toInt(), 2);
    QCOMPARE(findItem(page, QStringLiteral("captureRecordFinishBox"))->property("currentIndex").toInt(), 2);
    auto *showResult = findItem(page, QStringLiteral("captureShowResultSwitch"));
    QVERIFY(showResult != nullptr && showResult->property("checked").toBool());
    auto *flick = findItem(page, QStringLiteral("streamingFormViewport"));
    QVERIFY(flick != nullptr);
    flick->setProperty("contentY", qMax(0.0, showResult->mapToItem(flick->childItems().first(), QPointF()).y() - 100));
    QCoreApplication::processEvents();
    const QPointF center = showResult->mapToScene(QPointF(showResult->width() / 2, showResult->height() / 2));
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, center.toPoint());
    QTRY_COMPARE(settingsTransport.commits, 1);
    QVERIFY(capture.writePending());
    // Not published before the service reads it back.
    QVERIFY(showResult->property("checked").toBool());
}

QTEST_MAIN(StreamingPageTest)
#include "tst_streaming_page.moc"
