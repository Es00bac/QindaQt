// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_streaming/streaming_settings_model.h>
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
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

QTEST_MAIN(StreamingPageTest)
#include "tst_streaming_page.moc"
