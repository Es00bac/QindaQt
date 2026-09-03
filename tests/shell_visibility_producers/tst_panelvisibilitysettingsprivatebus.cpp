// SPDX-License-Identifier: GPL-3.0-or-later
#include "globalshortcutregistrar.h"
#include "panelvisibilityruntime.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QAction>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QtTest>

#include <functional>

using namespace QindaQt;

namespace {

using namespace Services::SettingsProtocol;

class CanonicalSettingsObject final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Settings1")
public Q_SLOTS:
    QVariantMap GetSnapshot(const QStringList &keys)
    {
        requestedKeysCorrect = keys
            == QStringList({QStringLiteral("accessibility.reducedMotion"),
                            QStringLiteral("panels.autoHideDelayMs")});
        const QVariantMap values{
            {QStringLiteral("accessibility.reducedMotion"), false},
            {QStringLiteral("panels.autoHideDelayMs"), qint64(400)},
        };
        const QVariantMap sources{
            {QStringLiteral("accessibility.reducedMotion"),
             QStringLiteral("user-overrides")},
            {QStringLiteral("panels.autoHideDelayMs"),
             QStringLiteral("user-overrides")},
        };
        return {
            {QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch),
             QStringLiteral("panel-settings-private-bus")},
            {QLatin1StringView(WireContract::FieldRevision), quint64(1)},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}},
        };
    }

public:
    bool requestedKeysCorrect = false;
};

class FakeRegistrar final : public Shell::GlobalShortcutRegistrar {
public:
    Shell::GlobalShortcutRegistration registerShortcut(
        QAction &, const QKeySequence &, QObject &,
        std::function<void(bool)>) override
    {
        return {true, true};
    }
};

} // namespace

class PanelVisibilitySettingsPrivateBusTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void appliesCanonicalSettings1Integer();
};

void PanelVisibilitySettingsPrivateBusTests::appliesCanonicalSettings1Integer()
{
    // AGENT-NOTE: P1-1 was a production-only type mismatch. This round trip
    // must traverse real Qt D-Bus decoding before the runtime sees qint64.
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    CanonicalSettingsObject service;
    QVERIFY(bus.registerService(QString::fromLatin1(WireContract::ServiceName)));
    QVERIFY(bus.registerObject(QString::fromLatin1(WireContract::ObjectPath),
                               &service, QDBusConnection::ExportAllSlots));

    Services::SettingsClient::QtSettingsTransport transport(bus);
    Services::SettingsClient::ClientTiming timing;
    timing.requestTimeoutMilliseconds = 500;
    timing.debounceMilliseconds = 0;
    timing.retryMilliseconds = {10};
    Services::SettingsClient::SettingsClient settings(
        transport, {QStringLiteral("accessibility.reducedMotion"),
                    QStringLiteral("panels.autoHideDelayMs")},
        timing);
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities(
        {{QStringLiteral("dock"), QStringLiteral("main")}}));
    FakeRegistrar registrar;
    Profiles::LayoutProfile profile;
    Profiles::PanelSpec panel;
    panel.id = QStringLiteral("dock");
    panel.hideMode = Profiles::HideMode::Always;
    profile.panels.append(panel);
    Shell::PanelVisibilityRuntime runtime(
        *qGuiApp, store, settings, registrar, profile, 320);

    QString error;
    QVERIFY2(settings.start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(settings.snapshot().has_value(), 2'000);
    QVERIFY(service.requestedKeysCorrect);
    QCOMPARE(settings.snapshot()->values
                 .value(QStringLiteral("panels.autoHideDelayMs"))
                 .metaType().id(),
             QMetaType::LongLong);
    QVERIFY(!runtime.reducedMotion());
    QCOMPARE(runtime.animationDurationMilliseconds(), 320);

    settings.stop();
    bus.unregisterObject(QString::fromLatin1(WireContract::ObjectPath));
    bus.unregisterService(QString::fromLatin1(WireContract::ServiceName));
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    PanelVisibilitySettingsPrivateBusTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_panelvisibilitysettingsprivatebus.moc"
