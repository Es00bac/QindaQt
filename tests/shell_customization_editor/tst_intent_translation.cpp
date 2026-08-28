// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_test_fixtures.h"

#include "qindaqt/shell_customization/editing_commands.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"
#include "qindaqt/shell_customization_editor/intent_translator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include <limits>
#include <type_traits>

using namespace QindaQt;
using namespace QindaQt::ShellCustomizationEditor;
using namespace QindaQt::ShellCustomizationEditor::TestFixtures;

namespace {

bool sameKind(const QindaQt::ShellCustomization::EditingCommand &command,
              QindaQt::ShellCustomization::EditingCommandKind kind)
{
    return QindaQt::ShellCustomization::commandKind(command) == kind;
}

QByteArray commandBytes(const QindaQt::ShellCustomization::EditingCommand &command)
{
    using namespace QindaQt::ShellCustomization;
    QVariantMap payload{
        {QStringLiteral("kind"), static_cast<int>(commandKind(command))},
        {QStringLiteral("revision"), QVariant::fromValue(expectedRevision(command))},
    };
    const auto optionalValue = [](const std::optional<QString> &value) -> QVariant {
        return value.has_value() ? QVariant{*value} : QVariant{};
    };
    std::visit(
        [&](const auto &typed) {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, InsertAppletCommand>) {
                payload.insert(QStringLiteral("panel"), typed.panelId);
                payload.insert(QStringLiteral("instance"), typed.instanceId);
                payload.insert(QStringLiteral("plugin"), typed.pluginId);
                payload.insert(QStringLiteral("settings"), typed.initialSettings);
                payload.insert(QStringLiteral("before"), optionalValue(typed.beforeAppletId));
            } else if constexpr (std::is_same_v<T, MoveAppletCommand>) {
                payload.insert(QStringLiteral("source"), typed.sourcePanelId);
                payload.insert(QStringLiteral("applet"), typed.appletId);
                payload.insert(QStringLiteral("target"), typed.targetPanelId);
                payload.insert(QStringLiteral("before"), optionalValue(typed.beforeAppletId));
            } else if constexpr (std::is_same_v<T, RemoveAppletCommand>) {
                payload.insert(QStringLiteral("panel"), typed.panelId);
                payload.insert(QStringLiteral("applet"), typed.appletId);
            } else if constexpr (std::is_same_v<T, DuplicateAppletCommand>) {
                payload.insert(QStringLiteral("source"), typed.sourcePanelId);
                payload.insert(QStringLiteral("applet"), typed.appletId);
                payload.insert(QStringLiteral("target"), typed.targetPanelId);
                payload.insert(QStringLiteral("new"), typed.newAppletId);
                payload.insert(QStringLiteral("before"), optionalValue(typed.beforeAppletId));
            } else if constexpr (std::is_same_v<T, UpdateAppletSettingsCommand>) {
                payload.insert(QStringLiteral("panel"), typed.panelId);
                payload.insert(QStringLiteral("applet"), typed.appletId);
                payload.insert(QStringLiteral("settings"), typed.settings);
            } else if constexpr (std::is_same_v<T, ConfigurePanelCommand>) {
                payload.insert(QStringLiteral("panel"), typed.panelId);
                payload.insert(QStringLiteral("layer"), Profiles::toString(typed.layer));
                payload.insert(QStringLiteral("hide"), Profiles::toString(typed.hideMode));
                payload.insert(QStringLiteral("rows"), typed.rows);
                payload.insert(QStringLiteral("thickness"), typed.thickness);
                payload.insert(QStringLiteral("length"), typed.length);
            } else if constexpr (std::is_same_v<T, MovePanelCommand>) {
                payload.insert(QStringLiteral("panel"), typed.panelId);
                payload.insert(QStringLiteral("output"), typed.outputId);
                payload.insert(QStringLiteral("edge"), Profiles::toString(typed.edge));
                payload.insert(QStringLiteral("alignment"), Profiles::toString(typed.alignment));
                payload.insert(QStringLiteral("before"), optionalValue(typed.beforePanelId));
            }
        },
        command);
    return QJsonDocument{QJsonObject::fromVariantMap(payload)}.toJson(QJsonDocument::Compact);
}

} // namespace

class IntentTranslationTest final : public QObject
{
    Q_OBJECT

private slots:
    void zoneVocabularyIsLimitedToSchemaV1Zones() const
    {
        QVERIFY(isValidEditorZone(QStringLiteral("start")));
        QVERIFY(isValidEditorZone(QStringLiteral("center")));
        QVERIFY(isValidEditorZone(QStringLiteral("end")));
        // The editor must not offer zones the v1 runtime cannot honor
        // (panel-fill needs schema v2; desktop needs its own slice).
        QVERIFY(!isValidEditorZone(QStringLiteral("fill")));
        QVERIFY(!isValidEditorZone(QStringLiteral("desktop")));
        QVERIFY(!isValidEditorZone(QString()));
    }

    void paletteInsertTranslatesToSingleInsertCommand() const
    {
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("start"),
                                QStringLiteral("launcher-instance")};
        TranslationContext context;
        context.expectedRevision = 7;
        context.newInstanceAppletId = QStringLiteral("clock-instance-9");

        const auto commands =
            translateIntent(paletteInsertIntent(palettePayload(QStringLiteral("clock"))), target, context);

        QCOMPARE(commands.size(), 1);
        QVERIFY(sameKind(commands.first(),
                         QindaQt::ShellCustomization::EditingCommandKind::InsertApplet));
        const auto &insert =
            std::get<QindaQt::ShellCustomization::InsertAppletCommand>(commands.first());
        QCOMPARE(insert.expectedRevision, quint64{7});
        QCOMPARE(insert.panelId, QStringLiteral("bar"));
        QCOMPARE(insert.instanceId, QStringLiteral("clock-instance-9"));
        QCOMPARE(insert.pluginId, QStringLiteral("clock"));
        QCOMPARE(insert.beforeAppletId.value_or(QString()), QStringLiteral("launcher-instance"));
        QCOMPARE(insert.initialSettings.value(QStringLiteral("zone")).toString(),
                 QStringLiteral("start"));
    }

    void sameZoneMoveTranslatesToSingleMoveCommand() const
    {
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("end"),
                                QStringLiteral("clock-instance")};
        TranslationContext context;
        context.expectedRevision = 3;
        context.sourceSettings = {{QStringLiteral("zone"), QStringLiteral("end")}};

        const auto commands = translateIntent(
            instanceMoveIntent(instancePayload(QStringLiteral("bar"), QStringLiteral("launcher-instance"))),
            target, context);

        QCOMPARE(commands.size(), 1);
        QVERIFY(sameKind(commands.first(),
                         QindaQt::ShellCustomization::EditingCommandKind::MoveApplet));
    }

    void zoneCrossingMoveAppendsSettingsCompanion() const
    {
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("start"),
                                QStringLiteral("launcher-instance")};
        TranslationContext context;
        context.expectedRevision = 3;
        context.sourceSettings = {{QStringLiteral("zone"), QStringLiteral("end")}};

        const auto commands = translateIntent(
            instanceMoveIntent(instancePayload(QStringLiteral("bar"), QStringLiteral("clock-instance"))),
            target, context);

        QCOMPARE(commands.size(), 2);
        QVERIFY(sameKind(commands.first(),
                         QindaQt::ShellCustomization::EditingCommandKind::MoveApplet));
        QVERIFY(sameKind(commands.last(),
                         QindaQt::ShellCustomization::EditingCommandKind::UpdateAppletSettings));
        const auto &update =
            std::get<QindaQt::ShellCustomization::UpdateAppletSettingsCommand>(commands.last());
        QCOMPARE(update.panelId, QStringLiteral("bar"));
        QCOMPARE(update.appletId, QStringLiteral("clock-instance"));
        QCOMPARE(update.settings.value(QStringLiteral("zone")).toString(),
                 QStringLiteral("start"));
    }

    void companionPreservesUnrelatedSettings() const
    {
        const DropTarget target{QStringLiteral("dock"), QStringLiteral("center"), {}};
        TranslationContext context;
        context.expectedRevision = 1;
        context.sourceSettings = {{QStringLiteral("zone"), QStringLiteral("start")},
                                  {QStringLiteral("custom"), QStringLiteral("value")}};

        const auto commands = translateIntent(
            instanceMoveIntent(instancePayload(QStringLiteral("bar"), QStringLiteral("launcher-instance"))),
            target, context);

        QCOMPARE(commands.size(), 2);
        const auto &update =
            std::get<QindaQt::ShellCustomization::UpdateAppletSettingsCommand>(commands.last());
        QCOMPARE(update.settings.size(), 2);
        QCOMPARE(update.settings.value(QStringLiteral("custom")).toString(),
                 QStringLiteral("value"));
        QCOMPARE(update.settings.value(QStringLiteral("zone")).toString(),
                 QStringLiteral("center"));
    }

    void configurePanelSendsTheCompleteTuple() const
    {
        // AGENT-GUARD check: ConfigurePanelCommand replaces every field, so a
        // partial tuple would silently reset the others.
        const DropTarget target;
        TranslationContext context;
        context.expectedRevision = 2;

        PanelConfiguration configuration;
        configuration.layer = Profiles::Layer::Overlay;
        configuration.hideMode = Profiles::HideMode::Intelligent;
        configuration.rows = 1;
        configuration.thickness = 64;
        configuration.length = 0.75;
        const auto commands = translateIntent(
            configureIntent(QStringLiteral("dock"), configuration), target, context);

        QCOMPARE(commands.size(), 1);
        const auto &configure =
            std::get<QindaQt::ShellCustomization::ConfigurePanelCommand>(commands.first());
        QCOMPARE(configure.panelId, QStringLiteral("dock"));
        QCOMPARE(static_cast<int>(configure.layer),
                 static_cast<int>(Profiles::Layer::Overlay));
        QCOMPARE(static_cast<int>(configure.hideMode),
                 static_cast<int>(Profiles::HideMode::Intelligent));
        QCOMPARE(configure.rows, 1);
        QCOMPARE(configure.thickness, 64);
        QCOMPARE(configure.length, 0.75);
    }

    void gestureSequenceWrapsMutationsInPreviewBracket() const
    {
        const DropTarget target{QStringLiteral("bar"), QStringLiteral("end"),
                                QStringLiteral("clock-instance")};
        TranslationContext context;
        context.expectedRevision = 5;
        context.newInstanceAppletId = QStringLiteral("clock-instance-1");

        const auto sequence = gestureSequence(
            paletteInsertIntent(palettePayload(QStringLiteral("clock"))), target, context);

        QCOMPARE(sequence.size(), 3);
        QVERIFY(sameKind(sequence.first(),
                         QindaQt::ShellCustomization::EditingCommandKind::BeginPreview));
        QVERIFY(sameKind(sequence.at(1),
                         QindaQt::ShellCustomization::EditingCommandKind::InsertApplet));
        QVERIFY(sameKind(sequence.last(),
                         QindaQt::ShellCustomization::EditingCommandKind::CommitPreview));
    }

    void pointerAndKeyboardTranslationPayloadsAreByteIdenticalForEveryIntent() const
    {
        // Determinism is the contract the pointer/keyboard parity invariant
        // relies on: both input paths call this same translator.
        const DropTarget target{QStringLiteral("dock"), QStringLiteral("center"), {}};
        TranslationContext context;
        context.expectedRevision = 4;
        context.sourceSettings = {{QStringLiteral("zone"), QStringLiteral("start")}};

        context.newInstanceAppletId = QStringLiteral("new-instance");
        const auto comparePaths = [&](const CustomizationIntent &pointerIntent,
                                      const CustomizationIntent &keyboardIntent,
                                      const DropTarget &intentTarget) {
            const auto pointer = translateIntent(pointerIntent, intentTarget, context);
            const auto keyboard = translateIntent(keyboardIntent, intentTarget, context);
            QCOMPARE(pointer.size(), keyboard.size());
            for (qsizetype index = 0; index < pointer.size(); ++index) {
                QCOMPARE(commandBytes(pointer.at(index)), commandBytes(keyboard.at(index)));
            }
        };

        comparePaths(paletteInsertIntent(palettePayload(QStringLiteral("clock"))),
                     paletteInsertIntent(palettePayload(QStringLiteral("clock"))), target);
        comparePaths(instanceMoveIntent(instancePayload(QStringLiteral("bar"),
                                                        QStringLiteral("launcher-instance"))),
                     instanceMoveIntent(instancePayload(QStringLiteral("bar"),
                                                        QStringLiteral("launcher-instance"))), target);
        comparePaths(removeIntent(QStringLiteral("bar"), QStringLiteral("launcher-instance")),
                     removeIntent(QStringLiteral("bar"), QStringLiteral("launcher-instance")), target);
        comparePaths(duplicateIntent(QStringLiteral("bar"),
                                     QStringLiteral("launcher-instance"),
                                     QStringLiteral("new-instance")),
                     duplicateIntent(QStringLiteral("bar"),
                                     QStringLiteral("launcher-instance"),
                                     QStringLiteral("new-instance")), target);
        PanelConfiguration configuration;
        configuration.layer = Profiles::Layer::Overlay;
        configuration.hideMode = Profiles::HideMode::DodgeAll;
        configuration.rows = 2;
        configuration.thickness = 64;
        configuration.length = 0.5;
        comparePaths(configureIntent(QStringLiteral("bar"), configuration),
                     configureIntent(QStringLiteral("bar"), configuration), target);
        comparePaths(movePanelIntent(QStringLiteral("bar"), QStringLiteral("primary"),
                                     Profiles::Edge::Bottom,
                                     Profiles::Alignment::Center, std::nullopt),
                     movePanelIntent(QStringLiteral("bar"), QStringLiteral("primary"),
                                     Profiles::Edge::Bottom,
                                     Profiles::Alignment::Center, std::nullopt), target);
    }

    void structuralValidationRejectsMalformedIntents() const
    {
        const DropTarget validTarget{QStringLiteral("bar"), QStringLiteral("start"), {}};

        const auto blankPlugin =
            validateIntent(paletteInsertIntent(palettePayload(QString())), validTarget);
        QVERIFY(!blankPlugin.ok());
        QCOMPARE(blankPlugin.code, IntentErrorCode::EmptyPluginId);

        const auto selfAnchor = validateIntent(
            instanceMoveIntent(instancePayload(QStringLiteral("bar"), QStringLiteral("clock-instance"))),
            DropTarget{QStringLiteral("bar"), QStringLiteral("end"),
                       QStringLiteral("clock-instance")});
        QVERIFY(!selfAnchor.ok());
        QCOMPARE(selfAnchor.code, IntentErrorCode::AnchorSelfReference);

        const auto forbiddenZone =
            validateIntent(paletteInsertIntent(palettePayload(QStringLiteral("clock"))),
                           DropTarget{QStringLiteral("bar"), QStringLiteral("fill"), {}});
        QVERIFY(!forbiddenZone.ok());
        QCOMPARE(forbiddenZone.code, IntentErrorCode::InvalidZone);

        const auto duplicateWithoutIdentity =
            validateIntent(duplicateIntent(QStringLiteral("bar"),
                                           QStringLiteral("clock-instance"), QString()),
                           validTarget);
        QVERIFY(!duplicateWithoutIdentity.ok());
        QCOMPARE(duplicateWithoutIdentity.code, IntentErrorCode::EmptyNewAppletId);

        PanelConfiguration outOfBounds;
        outOfBounds.length = 1.5;
        const auto badConfiguration =
            validateIntent(configureIntent(QStringLiteral("bar"), outOfBounds), validTarget);
        QVERIFY(!badConfiguration.ok());
        QCOMPARE(badConfiguration.code, IntentErrorCode::InvalidConfiguration);

        const auto rejectsConfiguration = [&](const PanelConfiguration &configuration) {
            const auto result =
                validateIntent(configureIntent(QStringLiteral("bar"), configuration), validTarget);
            QVERIFY(!result.ok());
            QCOMPARE(result.code, IntentErrorCode::InvalidConfiguration);
        };
        for (const int rows : {0, 5}) {
            PanelConfiguration invalid;
            invalid.rows = rows;
            rejectsConfiguration(invalid);
        }
        for (const int thickness : {19, 193}) {
            PanelConfiguration invalid;
            invalid.thickness = thickness;
            rejectsConfiguration(invalid);
        }
        for (const double length : {0.09,
                                    std::numeric_limits<double>::infinity(),
                                    std::numeric_limits<double>::quiet_NaN()}) {
            PanelConfiguration invalid;
            invalid.length = length;
            rejectsConfiguration(invalid);
        }
        PanelConfiguration hiddenForever;
        hiddenForever.hideMode = Profiles::HideMode::Always;
        rejectsConfiguration(hiddenForever);
        PanelConfiguration unknownLayer;
        unknownLayer.layer = static_cast<Profiles::Layer>(999);
        rejectsConfiguration(unknownLayer);

        const auto invalidMovePanel = validateIntent(
            movePanelIntent(QStringLiteral("bar"), QStringLiteral("primary"),
                            static_cast<Profiles::Edge>(999),
                            Profiles::Alignment::Center, std::nullopt),
            validTarget);
        QVERIFY(!invalidMovePanel.ok());
        QCOMPARE(invalidMovePanel.code, IntentErrorCode::InvalidConfiguration);

        const auto valid = validateIntent(removeIntent(QStringLiteral("bar"),
                                                       QStringLiteral("clock-instance")),
                                          validTarget);
        QVERIFY(valid.ok());
    }
};

QTEST_MAIN(IntentTranslationTest)
#include "tst_intent_translation.moc"
