// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelltokenpublisher.h"
#include "shellappearancebridge.h"
#include "shellpreferencevalues.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QColor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_TaskListPlugin)

namespace {

constexpr auto HostedAppletSource = R"qml(
    import QtQuick
    import QindaQt.Shell.TaskList 1.0
    import QindaQt.Tokens 1.0

    Item {
        width: 480
        height: 64
        readonly property bool tokenReady: Tokens.ready
        readonly property color backgroundBase: Tokens.bg.base

        QtObject {
            id: taskAccess
            property string phaseText: "loading"
            property int entryCount: 0
            property int totalEntryCount: 0
            property var entryRows: []
            property int overflowCount: 0
            property string phaseReasonText: ""
            property bool feedbackPresent: false
            property string feedback: ""
        }

        TaskListApplet {
            id: hostedApplet
            anchors.fill: parent
            access: taskAccess
        }
    }
)qml";

class BridgeTransport final : public QindaQt::Services::SettingsClient::SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    { requests.append({token, owner}); }
    void commit(quint64, const QString &, const QString &, quint64, const QVariantList &) override {}
    void requestActivation() override {}
    struct Request { quint64 token; QString owner; };
    QList<Request> requests;
};

using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

QVariantMap preferenceSnapshot(const QVariantMap &values, QString epoch, quint64 revision)
{
    QVariantMap layers;
    for (const QString &key : values.keys()) {
        layers.insert(key, QStringLiteral("user-overrides"));
    }
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), std::move(epoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), layers},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap preferenceValues(const QString &profile, const QString &theme,
                             const QString &fontFamily, double pointSize,
                             double textScale, bool highContrast,
                             bool reducedMotion, bool reducedTransparency)
{
    return {{QStringLiteral("panels.layoutProfile"), profile},
            {QStringLiteral("appearance.theme"), theme},
            {QStringLiteral("appearance.colorScheme"),
             theme.contains(QStringLiteral("light")) ? QStringLiteral("light")
                                                       : QStringLiteral("dark")},
            {QStringLiteral("appearance.wallpaper"), QStringLiteral("qindaqt:jade-fold")},
            {QStringLiteral("appearance.wallpaperMode"), QStringLiteral("scaled")},
            {QStringLiteral("fonts.family"), fontFamily},
            {QStringLiteral("fonts.pointSize"), pointSize},
            {QStringLiteral("accessibility.highContrast"), highContrast},
            {QStringLiteral("accessibility.reducedMotion"), reducedMotion},
            {QStringLiteral("accessibility.reducedTransparency"), reducedTransparency},
            {QStringLiteral("accessibility.textScale"), textScale}};
}

QVariantMap preferenceValues()
{
    return preferenceValues(QStringLiteral("qindaqt"), QStringLiteral("qinda-light"),
                            QStringLiteral("Noto Serif"), 12.0, 2.0, true, true,
                            false);
}

// Counts the bridge's expected malformed-snapshot diagnostic while installed;
// see appearanceBridgeAppliesConfirmedSnapshots.
int g_malformedSnapshotWarnings = 0;

void swallowMalformedSnapshotWarnings(QtMsgType type,
                                      const QMessageLogContext &context,
                                      const QString &message)
{
    if (type == QtWarningMsg
        && message.contains(QStringLiteral("invalid preference snapshot"))) {
        ++g_malformedSnapshotWarnings;
        return;
    }
    qt_message_output(type, context, message);
}

QQuickItem *itemNamed(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = itemNamed(child, name)) {
            return match;
        }
    }
    return nullptr;
}

} // namespace

class ShellRuntimeTokenTests final : public QObject {
    Q_OBJECT

private slots:
    void productionPublisherPrecedesHostedApplet();
    void accessibilityInputsRepublishTokens();
    void appearanceBridgeAppliesConfirmedSnapshots();
    void appearanceBridgeCliThemeLockRetainsAccessibility();
};

void ShellRuntimeTokenTests::productionPublisherPrecedesHostedApplet()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    QVERIFY2(themes.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"), &error),
             qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("qinda-dark")));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_QML_BUILD_DIR));
    QindaQt::Shell::ShellTokenPublisher publisher(engine, themes);
    QVERIFY2(publisher.start(&error), qPrintable(error));
    QVERIFY(publisher.facade() != nullptr);
    QVERIFY(publisher.facade()->ready());
    QCOMPARE(publisher.facade()->sourceThemeId(), QStringLiteral("qinda-dark"));
    QCOMPARE(publisher.facade()->bg().value(QStringLiteral("base")).value<QColor>(),
             QColor(QStringLiteral("#111e2c")));

    QQmlComponent component(&engine);
    component.setData(HostedAppletSource,
                      QUrl(QStringLiteral("inline:hosted-task-list-token-row.qml")));
    QTRY_VERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(root != nullptr);
    QCOMPARE(root->property("tokenReady").toBool(), true);
    QCOMPARE(root->property("backgroundBase").value<QColor>(),
             QColor(QStringLiteral("#111e2c")));

    auto *hostedApplet = itemNamed(root, QStringLiteral("taskListApplet"));
    auto *loadingLabel = itemNamed(root, QStringLiteral("taskListLoadingLabel"));
    QVERIFY(hostedApplet != nullptr);
    QVERIFY(loadingLabel != nullptr);
    QCOMPARE(loadingLabel->property("color").value<QColor>(),
             publisher.facade()->fg()
                 .value(QStringLiteral("muted"))
                 .value<QColor>());

    const qulonglong priorGeneration = publisher.facade()->generation();
    QVERIFY(themes.selectById(QStringLiteral("qinda-light")));
    QCOMPARE(publisher.facade()->generation(), priorGeneration + 1);
    QCOMPARE(root->property("backgroundBase").value<QColor>(),
             QColor(QStringLiteral("#dde7e8")));
}

void ShellRuntimeTokenTests::accessibilityInputsRepublishTokens()
{
    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    QVERIFY2(themes.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"), &error),
             qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("qinda-light")));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_QML_BUILD_DIR));
    QindaQt::Shell::ShellTokenPublisher publisher(engine, themes);

    // Confirmed startup preferences land before the first publication.
    QindaQt::DesignTokens::AccessibilityInputs startup;
    startup.basePointSize = 12.0;
    startup.textScale = 2.0;
    publisher.setAccessibilityInputs(startup);
    QVERIFY2(publisher.start(&error), qPrintable(error));
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
             24.0);
    // qinda-light motionDuration is 150 ms, published unclamped.
    QCOMPARE(publisher.facade()->motion().value(QStringLiteral("base")).toInt(), 150);

    // A later confirmed change republishes in place, one generation per swap.
    const qulonglong generation = publisher.facade()->generation();
    QindaQt::DesignTokens::AccessibilityInputs reduced;
    reduced.reducedMotion = true;
    publisher.setAccessibilityInputs(reduced);
    QCOMPARE(publisher.facade()->generation(), generation + 1);
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
             10.0);
    QCOMPARE(publisher.facade()->motion().value(QStringLiteral("base")).toInt(), 80);

    // Re-applying the same confirmed values does not churn generations.
    publisher.setAccessibilityInputs(reduced);
    QCOMPARE(publisher.facade()->generation(), generation + 1);

    // A confirmed fonts.family preference overlays the selected theme's own
    // family without disturbing the mono family.
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("fontFamily")).toString(),
             QStringLiteral("Inter"));
    publisher.setFontFamilyOverride(QStringLiteral("Noto Serif"));
    QCOMPARE(publisher.fontFamilyOverride(), QStringLiteral("Noto Serif"));
    QCOMPARE(publisher.facade()->generation(), generation + 2);
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("fontFamily")).toString(),
             QStringLiteral("Noto Serif"));
    QCOMPARE(publisher.facade()->type()
                 .value(QStringLiteral("monoFontFamily"))
                 .toString(),
             QStringLiteral("JetBrains Mono"));
    publisher.setFontFamilyOverride(QStringLiteral("Noto Serif"));
    QCOMPARE(publisher.facade()->generation(), generation + 2);
}

void ShellRuntimeTokenTests::appearanceBridgeAppliesConfirmedSnapshots()
{
    using namespace QindaQt::Services::SettingsClient;

    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    QVERIFY2(themes.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"), &error),
             qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("qinda-dark")));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_QML_BUILD_DIR));
    QindaQt::Shell::ShellTokenPublisher publisher(engine, themes);
    QVERIFY2(publisher.start(&error), qPrintable(error));
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
             10.0);

    BridgeTransport transport;
    SettingsClient client(transport, QindaQt::Shell::ShellPreferenceValues::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QindaQt::Shell::ShellAppearanceBridge bridge(client, themes, publisher,
                                                 /*themeLockedByCli=*/false);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.40"));
    QTRY_VERIFY_WITH_TIMEOUT(!transport.requests.isEmpty(), 2'000);
    auto first = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(
        first.token, first.owner,
        preferenceSnapshot(preferenceValues(), QStringLiteral("epoch-a"), 0));
    QTRY_COMPARE_WITH_TIMEOUT(
        themes.current().value(QStringLiteral("id")).toString(),
        QStringLiteral("qinda-light"), 2'000);
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
             24.0);
    QCOMPARE(publisher.facade()->type()
                 .value(QStringLiteral("fontFamily"))
                 .toString(),
             QStringLiteral("Noto Serif"));
    QCOMPARE(publisher.facade()->motion().value(QStringLiteral("base")).toInt(), 80);
    QVERIFY(bridge.lastConfirmed().has_value());
    QCOMPARE(bridge.lastConfirmed()->fontFamily, QStringLiteral("Noto Serif"));

    // Owner loss and malformed snapshots retain the last confirmed safe state.
    Q_EMIT transport.ownerChanged(QString{});
    QCOMPARE(themes.current().value(QStringLiteral("id")).toString(),
             QStringLiteral("qinda-light"));
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
             24.0);
    QCOMPARE(publisher.facade()->type()
                 .value(QStringLiteral("fontFamily"))
                 .toString(),
             QStringLiteral("Noto Serif"));

    Q_EMIT transport.ownerChanged(QStringLiteral(":1.41"));
    QTRY_VERIFY_WITH_TIMEOUT(!transport.requests.isEmpty(), 2'000);
    auto replacement = transport.requests.takeFirst();
    QVariantMap malformed = preferenceValues();
    malformed.insert(QStringLiteral("accessibility.highContrast"),
                     QStringLiteral("yes"));
    // The bridge must diagnose the dropped snapshot, but this test target runs
    // with QT_FATAL_WARNINGS=1, which QMessageLogger enforces regardless of
    // any installed handler. Suspend it for exactly this expected warning and
    // assert through the scoped handler that the diagnostic fired.
    g_malformedSnapshotWarnings = 0;
    const QtMessageHandler priorHandler =
        qInstallMessageHandler(swallowMalformedSnapshotWarnings);
    const QByteArray fatalWarnings = qgetenv("QT_FATAL_WARNINGS");
    qputenv("QT_FATAL_WARNINGS", "0");
    Q_EMIT transport.snapshotReceived(
        replacement.token, replacement.owner,
        preferenceSnapshot(malformed, QStringLiteral("epoch-b"), 1));
    QTest::qWait(50);
    if (fatalWarnings.isEmpty()) {
        qunsetenv("QT_FATAL_WARNINGS");
    } else {
        qputenv("QT_FATAL_WARNINGS", fatalWarnings);
    }
    qInstallMessageHandler(priorHandler);
    QCOMPARE(g_malformedSnapshotWarnings, 1);
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
             24.0);
    QCOMPARE(bridge.lastConfirmed()->themeId, QStringLiteral("qinda-light"));

    // A replacement owner's later confirmed snapshot applies normally.
    Q_EMIT transport.ownerChanged(QString{});
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.43"));
    QTRY_VERIFY_WITH_TIMEOUT(!transport.requests.isEmpty(), 2'000);
    auto next = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(
        next.token, next.owner,
        preferenceSnapshot(preferenceValues(QStringLiteral("qindaqt"),
                                            QStringLiteral("qinda-dark"),
                                            QStringLiteral("Inter"),
                                            10.0, 1.0, false, false, false),
                           QStringLiteral("epoch-c"), 2));
    QTRY_COMPARE_WITH_TIMEOUT(
        themes.current().value(QStringLiteral("id")).toString(),
        QStringLiteral("qinda-dark"), 2'000);
    QCOMPARE(publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
             10.0);
    QCOMPARE(publisher.facade()->type()
                 .value(QStringLiteral("fontFamily"))
                 .toString(),
             QStringLiteral("Inter"));
    QCOMPARE(publisher.facade()->motion().value(QStringLiteral("base")).toInt(), 160);
}

void ShellRuntimeTokenTests::appearanceBridgeCliThemeLockRetainsAccessibility()
{
    using namespace QindaQt::Services::SettingsClient;

    QindaQt::Themes::ThemeCatalog themes;
    QString error;
    QVERIFY2(themes.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"), &error),
             qPrintable(error));
    QVERIFY(themes.selectById(QStringLiteral("qinda-dark")));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_QML_BUILD_DIR));
    QindaQt::Shell::ShellTokenPublisher publisher(engine, themes);
    QVERIFY2(publisher.start(&error), qPrintable(error));

    BridgeTransport transport;
    SettingsClient client(transport, QindaQt::Shell::ShellPreferenceValues::scopedKeys(),
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}});
    QindaQt::Shell::ShellAppearanceBridge bridge(client, themes, publisher,
                                                 /*themeLockedByCli=*/true);
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.42"));
    QTRY_VERIFY_WITH_TIMEOUT(!transport.requests.isEmpty(), 2'000);
    auto request = transport.requests.takeFirst();
    Q_EMIT transport.snapshotReceived(
        request.token, request.owner,
        preferenceSnapshot(preferenceValues(), QStringLiteral("epoch-c"), 0));
    // An explicit --theme outranks the preference for the process lifetime;
    // accessibility preferences still apply.
    QTest::qWait(50);
    QCOMPARE(themes.current().value(QStringLiteral("id")).toString(),
             QStringLiteral("qinda-dark"));
    QTRY_COMPARE_WITH_TIMEOUT(
        publisher.facade()->type().value(QStringLiteral("body")).toDouble(),
        24.0, 2'000);
}

QTEST_MAIN(ShellRuntimeTokenTests)
#include "tst_shellruntime_tokens.moc"
