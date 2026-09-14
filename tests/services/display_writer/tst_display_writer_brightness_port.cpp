// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include "support/display_writer_test_support.h"

#include <QtTest/QTest>

#include <utility>

using namespace QindaQt::DisplayWriter;
using namespace QindaQt::DisplayWriter::TestSupport;
using QindaQt::DisplayService::BrightnessApplyOutcome;
using QindaQt::DisplayService::BrightnessApplyRequest;
using QindaQt::DisplayService::BrightnessSubmitStatus;
using QindaQt::DisplayService::DeviceBrightness;
using QindaQt::DisplayService::DeviceBrightnessFrame;
using QindaQt::DisplayTransaction::ApplyOutcome;

namespace
{

OutputDeviceState state(const quint32 value)
{
    return {.connectorName = QStringLiteral("DP-1"),
            .uuid = QStringLiteral("uuid-dp"),
            .enabled = true,
            .brightnessCapable = true,
            .brightnessObserved = true,
            .brightness = value};
}

BrightnessApplyRequest request(const quint64 requestId, const quint64 generation)
{
    return {.requestId = requestId,
            .ownerGeneration = generation,
            .connectorName = QStringLiteral("DP-1"),
            .runtimeUuid = QStringLiteral("uuid-dp"),
            .value = 2'500};
}

struct Harness {
    Harness()
    {
        auto fake = std::make_unique<FakeOutputManagementPort>();
        management = fake.get();
        writer = std::make_unique<WriterTransactionPort>(
            std::move(fake), std::make_unique<FakeJournalStore>(), 200);
        writer->setObserver(&observer);
    }

    [[nodiscard]] bool startAvailable(const quint64 generation = 4)
    {
        if (writer->start() != PortStartStatus::Started) {
            return false;
        }
        management->publishOwner(generation, true);
        return writer->isOutputManagementAvailable();
    }

    FakeOutputManagementPort *management = nullptr;
    RecordingObserver observer;
    std::unique_ptr<WriterTransactionPort> writer;
};

using CompletionRecord = std::pair<quint64, BrightnessApplyOutcome>;

} // namespace

class DisplayWriterBrightnessPortTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void forwardsOnlyCurrentOwnerFramesLater();
    void mapsSubmissionAndFencesCompletions();
    void excludesTopologyAppliesBothWays();
    void ownerChangeTimeoutAndStopAreUncertain();
    void replaysEstablishedFactsWhenTheObserverBindsLate();
    void stopClearsTheLateBindReplay();
};

void DisplayWriterBrightnessPortTests::forwardsOnlyCurrentOwnerFramesLater()
{
    Harness h;
    QVERIFY(h.startAvailable());
    h.management->publishDevices(4, {state(6'000)});
    h.management->publishDevices(3, {state(1'000)});
    QVERIFY2(h.observer.frames.isEmpty(), "device frames must not reenter synchronously");
    QTRY_COMPARE(h.observer.frames.size(), 2);
    const DeviceBrightnessFrame current{
        .ownerGeneration = 4,
        .devices = {{.connectorName = QStringLiteral("DP-1"),
                     .runtimeUuid = QStringLiteral("uuid-dp"),
                     .enabled = true,
                     .capable = true,
                     .observed = true,
                     .value = 6'000}}};
    QCOMPARE(h.observer.frames.at(0), current);
    QCOMPARE(h.observer.frames.at(1), DeviceBrightnessFrame{});

    h.management->publishOwner(4, false);
    h.management->publishDevices(4, {state(6'000)});
    QTRY_COMPARE(h.observer.frames.size(), 3);
    QCOMPARE(h.observer.frames.at(2), DeviceBrightnessFrame{});
}

void DisplayWriterBrightnessPortTests::replaysEstablishedFactsWhenTheObserverBindsLate()
{
    // Real startup order: the outer runtime starts this port before the
    // resident service binds as observer (session-safety readiness gates that
    // bind). Facts published in between must replay on bind, queued — never
    // synchronously — or the authority never learns external brightness.
    auto fake = std::make_unique<FakeOutputManagementPort>();
    FakeOutputManagementPort *management = fake.get();
    WriterTransactionPort port(std::move(fake), std::make_unique<FakeJournalStore>(), 200);
    QCOMPARE(port.start(), PortStartStatus::Started);
    management->publishOwner(4, true);
    management->publishDevices(4, {state(6'000)});

    RecordingObserver observer;
    port.setObserver(&observer);
    QVERIFY2(observer.frames.isEmpty(), "binding must not reenter synchronously");
    QTRY_COMPARE(observer.frames.size(), 1);
    QCOMPARE(observer.frames.constFirst(),
             (DeviceBrightnessFrame{.ownerGeneration = 4,
                                    .devices = {{.connectorName = QStringLiteral("DP-1"),
                                                 .runtimeUuid = QStringLiteral("uuid-dp"),
                                                 .enabled = true,
                                                 .capable = true,
                                                 .observed = true,
                                                 .value = 6'000}}}));

    // A generation replacement before any bind still replays only current
    // truth, and the late observer must never see the superseded generation.
    RecordingObserver observer2;
    management->publishOwner(5, true);
    management->publishDevices(5, {state(3'000)});
    QTRY_COMPARE(observer.frames.size(), 2);
    port.setObserver(&observer2);
    QTRY_COMPARE(observer2.frames.size(), 1);
    QCOMPARE(observer2.frames.constFirst().ownerGeneration, 5);
    QCOMPARE(observer2.frames.constFirst().devices.constFirst().value, 3'000);
}

void DisplayWriterBrightnessPortTests::stopClearsTheLateBindReplay()
{
    auto fake = std::make_unique<FakeOutputManagementPort>();
    FakeOutputManagementPort *management = fake.get();
    WriterTransactionPort port(std::move(fake), std::make_unique<FakeJournalStore>(), 200);
    QCOMPARE(port.start(), PortStartStatus::Started);
    management->publishOwner(4, true);
    management->publishDevices(4, {state(6'000)});
    port.stop();
    // After stop, a fresh observer binds to no truth: no replay, ever.
    RecordingObserver observer;
    port.setObserver(&observer);
    QCoreApplication::processEvents();
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QVERIFY(observer.frames.isEmpty());
}

void DisplayWriterBrightnessPortTests::mapsSubmissionAndFencesCompletions()
{
    Harness h;
    QVERIFY(h.startAvailable());
    QCOMPARE(h.writer->requestBrightness(request(41, 4)), BrightnessSubmitStatus::Accepted);
    QCOMPARE(h.management->brightnessSubmissions.size(), 1);
    const BrightnessConfiguration submitted = h.management->brightnessSubmissions.constFirst();
    QVERIFY(submitted.requestId != 0);
    QCOMPARE(submitted,
             (BrightnessConfiguration{.requestId = submitted.requestId,
                                      .connectorName = QStringLiteral("DP-1"),
                                      .uuid = QStringLiteral("uuid-dp"),
                                      .brightness = 2'500}));
    QCOMPARE(h.writer->requestBrightness(request(42, 4)), BrightnessSubmitStatus::Busy);
    QCOMPARE(h.management->brightnessSubmissions.size(), 1);

    h.management->complete(3, submitted.requestId, CompletionOutcome::Applied);
    h.management->complete(4, submitted.requestId + 1, CompletionOutcome::Applied);
    QCoreApplication::processEvents();
    QVERIFY(h.observer.brightnessCompletions.isEmpty());
    h.management->complete(4, submitted.requestId, CompletionOutcome::Applied);
    QVERIFY2(h.observer.brightnessCompletions.isEmpty(), "completion must be queued");
    QTRY_COMPARE(h.observer.brightnessCompletions.size(), 1);
    QCOMPARE(h.observer.brightnessCompletions.at(0),
             (CompletionRecord{41, BrightnessApplyOutcome::Applied}));

    QCOMPARE(h.writer->requestBrightness(request(43, 5)), BrightnessSubmitStatus::Unavailable);
    const QList<std::pair<SubmitStatus, BrightnessSubmitStatus>> refusals{
        {SubmitStatus::Unavailable, BrightnessSubmitStatus::Unavailable},
        {SubmitStatus::Busy, BrightnessSubmitStatus::Busy},
        {SubmitStatus::Unsupported, BrightnessSubmitStatus::Unsupported},
        {SubmitStatus::Malformed, BrightnessSubmitStatus::Malformed}};
    quint64 requestId = 50;
    for (const auto &[port, expected] : refusals) {
        h.management->brightnessSubmitStatus = port;
        QCOMPARE(h.writer->requestBrightness(request(requestId++, 4)), expected);
    }
    h.management->brightnessSubmitStatus = SubmitStatus::Accepted;
    QCoreApplication::processEvents();
    QCOMPARE(h.observer.brightnessCompletions.size(), 1);

    for (const auto &[outcome, expected] :
         {std::pair{CompletionOutcome::Rejected, BrightnessApplyOutcome::Rejected},
          std::pair{CompletionOutcome::Malformed, BrightnessApplyOutcome::TransportUncertain},
          std::pair{CompletionOutcome::TransportUncertain,
                    BrightnessApplyOutcome::TransportUncertain}}) {
        const qsizetype before = h.observer.brightnessCompletions.size();
        QCOMPARE(h.writer->requestBrightness(request(requestId, 4)),
                 BrightnessSubmitStatus::Accepted);
        h.management->complete(4, h.management->brightnessSubmissions.constLast().requestId,
                               outcome);
        QTRY_COMPARE(h.observer.brightnessCompletions.size(), before + 1);
        QCOMPARE(h.observer.brightnessCompletions.constLast(),
                 (CompletionRecord{requestId, expected}));
        ++requestId;
    }
}

void DisplayWriterBrightnessPortTests::excludesTopologyAppliesBothWays()
{
    Harness h;
    QVERIFY(h.startAvailable());
    h.writer->beginMachineLineage(2);
    h.writer->requestApply(completeRequest(11));
    QCOMPARE(h.management->submissions.size(), 1);
    QCOMPARE(h.writer->requestBrightness(request(41, 4)), BrightnessSubmitStatus::Busy);
    QVERIFY(h.management->brightnessSubmissions.isEmpty());
    h.management->complete(4, h.management->submissions.constFirst().requestId,
                           CompletionOutcome::Applied);
    QTRY_COMPARE(h.observer.completions.size(), 1);

    QCOMPARE(h.writer->requestBrightness(request(42, 4)), BrightnessSubmitStatus::Accepted);
    h.writer->requestApply(completeRequest(12));
    QTRY_COMPARE(h.observer.completions.size(), 2);
    QCOMPARE(h.observer.completions.at(1), (Completion{2, 12, ApplyOutcome::Rejected}));
    QCOMPARE(h.management->submissions.size(), 1);
    QVERIFY(h.observer.brightnessCompletions.isEmpty());
}

void DisplayWriterBrightnessPortTests::ownerChangeTimeoutAndStopAreUncertain()
{
    Harness h;
    QVERIFY(h.startAvailable());
    QCOMPARE(h.writer->requestBrightness(request(41, 4)), BrightnessSubmitStatus::Accepted);
    const quint64 abandoned = h.management->brightnessSubmissions.constLast().requestId;
    h.management->publishOwner(5, true);
    QTRY_COMPARE(h.observer.brightnessCompletions.size(), 1);
    QCOMPARE(h.observer.brightnessCompletions.at(0),
             (CompletionRecord{41, BrightnessApplyOutcome::TransportUncertain}));
    h.management->complete(5, abandoned, CompletionOutcome::Applied);

    QCOMPARE(h.writer->requestBrightness(request(42, 5)), BrightnessSubmitStatus::Accepted);
    QTRY_COMPARE_WITH_TIMEOUT(h.observer.brightnessCompletions.size(), 2, 2'000);
    QCOMPARE(h.observer.brightnessCompletions.at(1),
             (CompletionRecord{42, BrightnessApplyOutcome::TransportUncertain}));

    QCOMPARE(h.writer->requestBrightness(request(43, 5)), BrightnessSubmitStatus::Accepted);
    h.writer->stop();
    QTRY_COMPARE(h.observer.brightnessCompletions.size(), 3);
    QCOMPARE(h.observer.brightnessCompletions.at(2),
             (CompletionRecord{43, BrightnessApplyOutcome::TransportUncertain}));
    QTRY_VERIFY(!h.observer.frames.isEmpty());
    QCOMPARE(h.observer.frames.constLast(), DeviceBrightnessFrame{});
}

QTEST_GUILESS_MAIN(DisplayWriterBrightnessPortTests)
#include "tst_display_writer_brightness_port.moc"
