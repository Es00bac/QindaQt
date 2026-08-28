// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtimeoptions.h"

#include <QtTest>

#include <limits>

using namespace QindaQt::Shell;

class RuntimeOptionsTests final : public QObject {
    Q_OBJECT

private slots:
    void acceptsStandaloneShellModes();
    void acceptsTheCompleteNotificationAuthorityBundle();
    void rejectsPartialNotificationAuthority();
    void rejectsMissingTrustValues();
    void rejectsInvalidCompositorProcessIds_data();
    void rejectsInvalidCompositorProcessIds();
    void rejectsRepeatedTrustOptions();
    void validatesDevelopmentEvidencePredecessor();
};

void RuntimeOptionsTests::acceptsStandaloneShellModes()
{
    for (const QStringList &arguments : {
             QStringList{QStringLiteral("qindaqt-shell")},
             QStringList{QStringLiteral("qindaqt-shell"), QStringLiteral("--list")},
         }) {
        const auto parsed = parseRuntimeOptions(arguments);
        QVERIFY2(parsed.options.has_value(), qPrintable(parsed.error));
        QCOMPARE(parsed.options->presentationTokenDescriptor, -1);
        QVERIFY(!parsed.options->compositorProcessId.has_value());
    }
}

void RuntimeOptionsTests::acceptsTheCompleteNotificationAuthorityBundle()
{
    const auto parsed = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--compositor-pid"), QStringLiteral("42424")});
    QVERIFY2(parsed.options.has_value(), qPrintable(parsed.error));
    QCOMPARE(parsed.options->presentationTokenDescriptor, 7);
    QCOMPARE(parsed.options->compositorProcessId, std::optional<qint64>(42'424));

    const qint64 maximumPid = std::numeric_limits<qint32>::max();
    const auto boundary = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--compositor-pid"), QString::number(maximumPid)});
    QVERIFY2(boundary.options.has_value(), qPrintable(boundary.error));
    QCOMPARE(boundary.options->compositorProcessId,
             std::optional<qint64>(maximumPid));
}

void RuntimeOptionsTests::rejectsPartialNotificationAuthority()
{
    const auto tokenOnly = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7")});
    QVERIFY(!tokenOnly.options.has_value());
    QVERIFY(tokenOnly.error.contains(QStringLiteral("supplied together")));

    const auto processOnly = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--compositor-pid"), QStringLiteral("42424")});
    QVERIFY(!processOnly.options.has_value());
    QVERIFY(processOnly.error.contains(QStringLiteral("supplied together")));
}

void RuntimeOptionsTests::rejectsMissingTrustValues()
{
    const auto missingProcessValue = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--compositor-pid")});
    QVERIFY(!missingProcessValue.options.has_value());
    QVERIFY(!missingProcessValue.error.isEmpty());

    const auto missingDescriptorValue = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd")});
    QVERIFY(!missingDescriptorValue.options.has_value());
    QVERIFY(!missingDescriptorValue.error.isEmpty());
}

void RuntimeOptionsTests::rejectsInvalidCompositorProcessIds_data()
{
    QTest::addColumn<QString>("value");
    QTest::newRow("empty") << QString();
    QTest::newRow("zero") << QStringLiteral("0");
    QTest::newRow("init") << QStringLiteral("1");
    QTest::newRow("negative") << QStringLiteral("-2");
    QTest::newRow("explicit-plus") << QStringLiteral("+2");
    QTest::newRow("suffix") << QStringLiteral("2x");
    QTest::newRow("qint32-overflow") << QStringLiteral("2147483648");
    QTest::newRow("qint64-overflow") << QStringLiteral("9223372036854775808");
}

void RuntimeOptionsTests::rejectsInvalidCompositorProcessIds()
{
    QFETCH(QString, value);
    const auto parsed = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--compositor-pid"), value});
    QVERIFY(!parsed.options.has_value());
    QVERIFY(!parsed.error.isEmpty());
}

void RuntimeOptionsTests::rejectsRepeatedTrustOptions()
{
    const auto repeatedProcess = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--compositor-pid"), QStringLiteral("42"),
         QStringLiteral("--compositor-pid"), QStringLiteral("43")});
    QVERIFY(!repeatedProcess.options.has_value());

    const auto repeatedDescriptor = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("8"),
         QStringLiteral("--compositor-pid"), QStringLiteral("42")});
    QVERIFY(!repeatedDescriptor.options.has_value());
}

void RuntimeOptionsTests::validatesDevelopmentEvidencePredecessor()
{
    const auto accepted = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--compositor-pid"), QStringLiteral("42424"),
         QStringLiteral("--development-evidence-predecessor-pid"),
         QStringLiteral("31337")});
    QVERIFY2(accepted.options.has_value(), qPrintable(accepted.error));
    QCOMPARE(accepted.options->developmentEvidencePredecessorProcessId,
             std::optional<qint64>(31'337));

    const auto missingBundle = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--development-evidence-predecessor-pid"),
         QStringLiteral("31337")});
    QVERIFY(!missingBundle.options.has_value());

    const auto invalid = parseRuntimeOptions(
        {QStringLiteral("qindaqt-shell"),
         QStringLiteral("--presentation-token-fd"), QStringLiteral("7"),
         QStringLiteral("--compositor-pid"), QStringLiteral("42424"),
         QStringLiteral("--development-evidence-predecessor-pid"),
         QStringLiteral("1")});
    QVERIFY(!invalid.options.has_value());
}

QTEST_GUILESS_MAIN(RuntimeOptionsTests)

#include "tst_runtimeoptions.moc"
