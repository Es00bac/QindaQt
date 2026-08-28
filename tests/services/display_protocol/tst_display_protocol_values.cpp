// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include "support/display_protocol_test_data.h"

#include <QtTest>

#include <limits>

using namespace QindaQt::Display;

class DisplayProtocolValueTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void acceptsBoundedValues();
    void rejectsEveryCollectionBound();
    void rejectsEveryTextBound();
    void rejectsInvalidEnumsNumbersAndCoordinates();
    void rejectsLineageIdentityAndRelationshipErrors();
    void confirmationPolicyIsClosedAndFailSafe();
};

void DisplayProtocolValueTests::acceptsBoundedValues()
{
    QVERIFY(validateMode(Test::mode()).accepted);
    QVERIFY(validateOutput(Test::output()).accepted);
    QVERIFY(validateCandidate(Test::candidate()).accepted);
    QVERIFY(validateSnapshot(Test::snapshot()).accepted);

    const TransactionSummary summary{
        .transactionId = QStringLiteral("transaction"),
        .state = TransactionState::ResolvingUncertain,
        .reason = TransactionReason::TransportUncertain,
        .initiatingEpoch = QStringLiteral("display-epoch"),
        .baseRevision = 7,
        .observedRevision = 8,
        .deadlineMonotonicMilliseconds = 42,
        .revertAttempt = 0,
    };
    QVERIFY(validateTransactionSummary(summary).accepted);

    const OperationResult result{.kind = OperationKind::Confirm,
                                 .status = OperationStatus::Succeeded,
                                 .error = ErrorCode::None,
                                 .initiatingEpoch = QStringLiteral("display-epoch"),
                                 .initiatingRevision = 7,
                                 .observedRevision = 8,
                                 .transactionId = QStringLiteral("transaction"),
                                 .diagnostic = {}};
    QVERIFY(validateOperationResult(result).accepted);
}

void DisplayProtocolValueTests::rejectsEveryCollectionBound()
{
    Snapshot snapshot = Test::snapshot();
    snapshot.outputs.clear();
    for (qsizetype index = 0; index <= kMaxOutputs; ++index) {
        Output value = Test::output(QStringLiteral("conn:%1").arg(index),
                                    QStringLiteral("DP-%1").arg(index));
        value.primary = index == 0;
        value.priority = static_cast<quint32>(index + 1);
        snapshot.outputs.push_back(std::move(value));
    }
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-snapshot-count"));

    Output output = Test::output();
    output.modes.clear();
    for (qsizetype index = 0; index <= kMaxModesPerOutput; ++index) {
        output.modes.push_back(Test::mode(QString::number(index)));
    }
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("too-many-modes"));

    Candidate candidate = Test::candidate();
    candidate.outputs.clear();
    for (qsizetype index = 0; index <= kMaxCandidateOutputs; ++index) {
        CandidateOutput value = Test::candidate().outputs.first();
        value.stableId = QStringLiteral("conn:%1").arg(index);
        value.primary = index == 0;
        value.priority = static_cast<quint32>(index + 1);
        candidate.outputs.push_back(std::move(value));
    }
    QCOMPARE(validateCandidate(candidate).reasonCode,
             QStringLiteral("invalid-candidate-output-count"));

    snapshot = Test::snapshot();
    snapshot.transactions = {{.transactionId = QStringLiteral("a"),
                              .state = TransactionState::Staged,
                              .initiatingEpoch = snapshot.serviceEpoch,
                              .baseRevision = snapshot.revision},
                             {.transactionId = QStringLiteral("b"),
                              .state = TransactionState::Staged,
                              .initiatingEpoch = snapshot.serviceEpoch,
                              .baseRevision = snapshot.revision}};
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("invalid-snapshot-count"));
}

void DisplayProtocolValueTests::rejectsEveryTextBound()
{
    Output output = Test::output();
    output.stableId = QString(kMaxStableIdUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));
    output = Test::output();
    output.connectorName = QString(kMaxConnectorNameUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));
    output = Test::output();
    output.runtimeCompositorUuid = QString(kMaxRuntimeUuidUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));
    output = Test::output();
    output.label = QString(kMaxLabelUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));
    output = Test::output();
    output.manufacturer = QString(kMaxManufacturerUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));
    output = Test::output();
    output.model = QString(kMaxModelUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));
    output = Test::output();
    output.modeId = QString(kMaxModeIdUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));
    output = Test::output();
    output.replicationSourceStableId = QString(kMaxStableIdUtf8Bytes + 1,
                                               QLatin1Char('x'));
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));

    Candidate candidate = Test::candidate();
    candidate.baseEpoch = QString(kMaxServiceEpochUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateCandidate(candidate).reasonCode,
             QStringLiteral("invalid-candidate-lineage"));
    candidate = Test::candidate();
    candidate.outputs[0].stableId = QString(kMaxStableIdUtf8Bytes + 1,
                                            QLatin1Char('x'));
    QCOMPARE(validateCandidate(candidate).reasonCode,
             QStringLiteral("invalid-candidate-text"));
    candidate = Test::candidate();
    candidate.outputs[0].modeId = QString(kMaxModeIdUtf8Bytes + 1,
                                          QLatin1Char('x'));
    QCOMPARE(validateCandidate(candidate).reasonCode,
             QStringLiteral("invalid-candidate-text"));
    candidate = Test::candidate();
    candidate.outputs[0].replicationSourceStableId = QString(
        kMaxStableIdUtf8Bytes + 1, QLatin1Char('x'));
    QCOMPARE(validateCandidate(candidate).reasonCode,
             QStringLiteral("invalid-candidate-text"));

    TransactionSummary summary{
        .transactionId = QString(kMaxTransactionIdUtf8Bytes + 1,
                                 QLatin1Char('x')),
        .state = TransactionState::Staged,
        .initiatingEpoch = QStringLiteral("epoch"),
        .baseRevision = 1,
    };
    QCOMPARE(validateTransactionSummary(summary).reasonCode,
             QStringLiteral("invalid-transaction-summary"));
    summary.transactionId = QStringLiteral("tx");
    summary.initiatingEpoch = QString(kMaxServiceEpochUtf8Bytes + 1,
                                      QLatin1Char('x'));
    QCOMPARE(validateTransactionSummary(summary).reasonCode,
             QStringLiteral("invalid-transaction-summary"));

    OperationResult result{
        .kind = OperationKind::Stage,
        .status = OperationStatus::Rejected,
        .error = ErrorCode::MalformedPayload,
        .initiatingEpoch = QStringLiteral("epoch"),
        .initiatingRevision = 1,
        .observedRevision = 0,
        .transactionId = {},
        .diagnostic = QString(kMaxDiagnosticUtf8Bytes + 1, QLatin1Char('x')),
        .wireValid = true,
    };
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("invalid-operation-result"));
}

void DisplayProtocolValueTests::rejectsInvalidEnumsNumbersAndCoordinates()
{
    Output output = Test::output();
    output.scale = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-value"));
    output = Test::output();
    output.scale = std::numeric_limits<double>::infinity();
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-value"));
    output = Test::output();
    output.scale = kMinimumScale - 0.01;
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-value"));
    output = Test::output();
    output.scale = kMaximumScale + 0.01;
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-value"));
    output = Test::output();
    output.transform = static_cast<Transform>(99);
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-value"));
    output = Test::output();
    output.position.setX(kCoordinateBound + 1);
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-value"));
    output = Test::output();
    output.modes[0].pixelSize.setWidth(kMaxPixelDimension + 1);
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-mode-value"));

    Candidate candidate = Test::candidate();
    candidate.outputs[0].scale = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(validateCandidate(candidate).reasonCode,
             QStringLiteral("invalid-candidate-value"));
    candidate = Test::candidate();
    candidate.outputs[0].transform = static_cast<Transform>(99);
    QCOMPARE(validateCandidate(candidate).reasonCode,
             QStringLiteral("invalid-candidate-value"));

    TransactionSummary summary{
        .transactionId = QStringLiteral("tx"),
        .state = static_cast<TransactionState>(99),
        .initiatingEpoch = QStringLiteral("epoch"),
        .baseRevision = 1,
    };
    QCOMPARE(validateTransactionSummary(summary).reasonCode,
             QStringLiteral("invalid-transaction-summary"));
    summary.state = TransactionState::Staged;
    summary.reason = static_cast<TransactionReason>(99);
    QCOMPARE(validateTransactionSummary(summary).reasonCode,
             QStringLiteral("invalid-transaction-summary"));

    OperationResult result{.kind = static_cast<OperationKind>(99),
                           .status = OperationStatus::Rejected,
                           .error = ErrorCode::MalformedPayload,
                           .initiatingEpoch = QStringLiteral("epoch"),
                           .initiatingRevision = 1,
                           .observedRevision = 0,
                           .transactionId = {},
                           .diagnostic = {},
                           .wireValid = true};
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("invalid-operation-result"));
    result.kind = OperationKind::Stage;
    result.status = static_cast<OperationStatus>(99);
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("invalid-operation-result"));
    result.status = OperationStatus::Rejected;
    result.error = static_cast<ErrorCode>(99);
    QCOMPARE(validateOperationResult(result).reasonCode,
             QStringLiteral("invalid-operation-result"));
}

void DisplayProtocolValueTests::rejectsLineageIdentityAndRelationshipErrors()
{
    Snapshot snapshot = Test::snapshot();
    snapshot.protocolVersion = 2;
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("unsupported-version"));
    snapshot = Test::snapshot();
    snapshot.liveFingerprint.clear();
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("invalid-snapshot-lineage"));
    snapshot = Test::snapshot();
    Output duplicate = snapshot.outputs.first();
    duplicate.connectorName = QStringLiteral("DP-2");
    duplicate.primary = false;
    duplicate.priority = 2;
    snapshot.outputs.push_back(duplicate);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("duplicate-output-id"));
    snapshot = Test::snapshot();
    snapshot.outputs[0].replicationSourceStableId = QStringLiteral("unknown");
    QCOMPARE(validateSnapshot(snapshot).reasonCode,
             QStringLiteral("unknown-replication-source"));
    snapshot = Test::snapshot();
    snapshot.outputs[0].enabled = false;
    snapshot.outputs[0].primary = false;
    snapshot.outputs[0].priority = 0;
    snapshot.outputs[0].position = QPoint(10, 10);
    QCOMPARE(validateSnapshot(snapshot).reasonCode, QStringLiteral("inconsistent-output"));

    Output output = Test::output();
    output.label = QStringLiteral("safe\u202Ehidden");
    QCOMPARE(validateOutput(output).reasonCode, QStringLiteral("invalid-output-text"));

    Candidate candidate = Test::candidate();
    candidate.wireValid = false;
    QCOMPARE(validateCandidate(candidate).reasonCode, QStringLiteral("malformed-payload"));
}

void DisplayProtocolValueTests::confirmationPolicyIsClosedAndFailSafe()
{
    QCOMPARE(confirmationRequirement(ChangeClass::Topology),
             ConfirmationRequirement::Required);
    for (quint32 value = static_cast<quint32>(ChangeClass::Brightness);
         value <= static_cast<quint32>(ChangeClass::CustomModeDefinition); ++value) {
        QCOMPARE(confirmationRequirement(static_cast<ChangeClass>(value)),
                 ConfirmationRequirement::BypassedForClosedPolicy);
    }
    QCOMPARE(confirmationRequirement(static_cast<ChangeClass>(999)),
             ConfirmationRequirement::Required);
}

QTEST_GUILESS_MAIN(DisplayProtocolValueTests)
#include "tst_display_protocol_values.moc"
