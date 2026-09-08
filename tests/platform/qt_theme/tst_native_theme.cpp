// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_palette.h"
#include <qindaqt/themes/theme_loader.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QQuickWindow>
#include <QFontDatabase>
#include <QWidget>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QStyle>
#include <QtTest>

using namespace QindaQt::Services::SettingsProtocol;
class AppearanceFixture final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Settings1")
public:
    QString theme = QStringLiteral("qinda-light");
    QString scheme = QStringLiteral("light");
    QString epoch = QStringLiteral("fixture-one");
    quint64 revision = 1;
    double pointSize = 17.0;
    int reads = 0;
    int writes = 0;
public slots:
    QVariantMap GetSnapshot(const QStringList &keys) {
        ++reads;
        const QVariantMap all{{"appearance.theme", theme}, {"appearance.colorScheme", scheme},
            {"fonts.family", QStringLiteral("DejaVu Sans")}, {"fonts.monospaceFamily", QStringLiteral("DejaVu Sans Mono")},
            {"fonts.pointSize", pointSize}, {"accessibility.textScale", 1.0},
            {"accessibility.highContrast", false}, {"accessibility.reducedMotion", false},
            {"accessibility.reducedTransparency", false}};
        QVariantMap values, sources;
        for (const auto &key : keys) { values.insert(key, all.value(key)); sources.insert(key, QStringLiteral("user-overrides")); }
        return {{WireContract::FieldStatus, quint32(SettingsWireStatus::Applied)},
                {WireContract::FieldWireSchemaVersion, WireContract::WireSchemaVersion},
                {WireContract::FieldSettingsSchemaVersion, quint32(2)},
                {WireContract::FieldEpoch, epoch}, {WireContract::FieldRevision, revision},
                {WireContract::FieldValues, values}, {WireContract::FieldSourceLayers, sources},
                {WireContract::FieldMessage, QString{}}};
    }
    QVariantMap CommitUserTransaction(const QString &, quint64, const QVariantList &) { ++writes; return {}; }
};

class NativeThemeTest final : public QObject {
    Q_OBJECT
    AppearanceFixture fixture;
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnection replacement = QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("replacement-owner"));
    QColor expected(const QString &theme, QPalette::ColorRole role) {
        const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_ROOT) + "/data/themes/" + theme + ".json");
        return QindaQt::QtTheme::nativeAppearance(loaded.theme, {})->palette.color(role);
    }
    void changed() {
        auto message = QDBusMessage::createSignal(WireContract::ObjectPath, WireContract::InterfaceName, WireContract::SettingsChangedSignal);
        message << fixture.epoch << ++fixture.revision << QStringList{QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme"), QStringLiteral("fonts.pointSize")};
        QVERIFY(bus.send(message));
    }
private slots:
    void initTestCase() {
        QVERIFY(bus.isConnected());
        QVERIFY(bus.registerObject(WireContract::ObjectPath, &fixture, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(WireContract::ServiceName));
        QTRY_VERIFY_WITH_TIMEOUT(fixture.reads > 0, 5000);
    }
    void nativeWidgetsAndQuickShareLiveAppearance() {
        QTRY_COMPARE(QApplication::palette().color(QPalette::Window), expected("qinda-light", QPalette::Window));
        QCOMPARE(QApplication::style()->objectName(), qEnvironmentVariable("QT_STYLE_OVERRIDE", "Fusion").toLower());
        QTRY_COMPARE(QApplication::font().pointSizeF(), 17.0);
        // QWidget's command-line style override also wins Qt Quick's style
        // selection on this Qt build. The widget-only variant still proves
        // user override preservation without requiring a Windows QML module.
        const bool widgetOnly = qEnvironmentVariable("QT_STYLE_OVERRIDE") == QStringLiteral("Windows");
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(widgetOnly
            ? "import QtQuick\nItem { property color observed: 'transparent' }"
            : "import QtQuick\nimport QtQuick.Controls\nApplicationWindow { property color observed: palette.window }", QUrl());
        QTRY_VERIFY(!component.isLoading());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window != nullptr, qPrintable(component.errorString()));
        if (!widgetOnly) QCOMPARE(window->property("observed").value<QColor>(), QApplication::palette().color(QPalette::Window));
        QWidget customWidget;
        QPalette customPalette = customWidget.palette();
        customPalette.setColor(QPalette::Window, QColor("#123456"));
        customWidget.setPalette(customPalette);
        fixture.theme = QStringLiteral("qinda-dark"); fixture.scheme = QStringLiteral("dark");
        fixture.pointSize = 19.0;
        changed();
        QTRY_COMPARE(QApplication::palette().color(QPalette::Window), expected("qinda-dark", QPalette::Window));
        if (!widgetOnly) QTRY_COMPARE(window->property("observed").value<QColor>(), QApplication::palette().color(QPalette::Window));
        QTRY_COMPARE(QApplication::font().pointSizeF(), 19.0);
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), expected("qinda-dark", QPalette::Highlight));
        QCOMPARE(QIcon::themeName(), QStringLiteral("QindaQt"));
        QCOMPARE(customWidget.palette().color(QPalette::Window), QColor("#123456"));
        QCOMPARE(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), QStringLiteral("DejaVu Sans Mono"));
    }
    void invalidThemeAndOwnerLossRetainPaletteWithoutWrites() {
        const auto palette = QApplication::palette();
        const int before = fixture.reads;
        fixture.theme = QStringLiteral("nonexistent-fixture-theme");
        fixture.scheme = QStringLiteral("not-a-scheme");
        changed();
        QTRY_VERIFY(fixture.reads > before);
        QTest::qWait(50);
        QCOMPARE(QApplication::palette(), palette);
        QVERIFY(bus.unregisterService(WireContract::ServiceName));
        QTest::qWait(100);
        QCOMPARE(QApplication::palette(), palette);
        QCOMPARE(fixture.writes, 0);
        fixture.epoch = QStringLiteral("fixture-two");
        fixture.theme = QStringLiteral("qinda-light"); fixture.scheme = QStringLiteral("light");
        fixture.revision = 1;
        QVERIFY(replacement.registerObject(WireContract::ObjectPath, &fixture, QDBusConnection::ExportAllSlots));
        QVERIFY(replacement.registerService(WireContract::ServiceName));
        QTRY_COMPARE(QApplication::palette().color(QPalette::Window), expected("qinda-light", QPalette::Window));
    }
    void projectionRejectsInvalidAndPreservesDisabledRoles() {
        QVERIFY(!QindaQt::QtTheme::nativeAppearance({}, {}));
        const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(QStringLiteral(QINDAQT_SOURCE_ROOT) + "/data/themes/qinda-high-contrast.json");
        const auto result = QindaQt::QtTheme::nativeAppearance(loaded.theme, {});
        QVERIFY(result.has_value());
        QVERIFY(result->highContrast);
        QCOMPARE(result->palette.color(QPalette::Highlight), result->palette.color(QPalette::Accent));
        QVERIFY(result->fixedFont.fixedPitch());
        for (int role = 0; role < QPalette::NColorRoles; ++role) {
            if (role == QPalette::NoRole) continue;
            for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
                QVERIFY(result->palette.isBrushSet(group, static_cast<QPalette::ColorRole>(role)));
        }
    }
};
QTEST_MAIN(NativeThemeTest)
#include "tst_native_theme.moc"
