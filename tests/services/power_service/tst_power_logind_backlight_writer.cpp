// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/logind_backlight_writer.h>

#include "support/fake_logind_backlight_service.h"
#include "support/private_bus.h"

#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using QindaQt::Tests::FakeLogindBacklightService;
using QindaQt::Tests::PrivateBus;

namespace {

constexpr quint32 kSubjectUid = 4242;
constexpr quint32 kOtherUid = 9;
constexpr char kDevice[] = "amdgpu_bl0";
constexpr char kAutoPath[] = "/org/freedesktop/login1/session/auto";

FakeLogindBacklightService::SessionSpec session(const QString &id, quint32 uid,
                                                const QString &seat,
                                                const QString &path, bool active)
{
    return {.sessionId = id,
            .uid = uid,
            .userName = QStringLiteral("tester"),
            .seatId = seat,
            .path = path,
            .active = active};
}

} // namespace

// The writer is the only place QindaQt asks another service to change the
// panel brightness for it (ADR-0186). These rows pin the two things that
// actually broke on real hardware: `.../session/auto` answers for the caller's
// session, which has no seat outside the graphical session, and a cached
// session path goes stale across a logout.
class PowerLogindBacklightWriterTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void usesAutoSessionWhenItReportsASeat();
    void fallsBackToTheActiveSeatSessionOfTheSubject();
    void prefersAnySeatedSubjectSessionWhenNoneIsActive();
    void unavailableWhenTheSubjectHasNoSeatSession();
    void failedProbeIsNotRepeatedOnEveryCall();
    void refusalIsReportedAndKeepsTheResolvedSession();
    void staleSessionIsReResolvedOnceAndRetried();
    void rejectsAnEmptyDeviceName();
    void disconnectedBusIsUnavailable();

private:
    std::unique_ptr<PrivateBus> m_bus;
    std::unique_ptr<QindaQt::Tests::FakeLogindBacklightServiceThread> m_logindHost;
    FakeLogindBacklightService *m_logind = nullptr;
    std::unique_ptr<Upstream::LogindBacklightWriter> m_writer;
};

void PowerLogindBacklightWriterTests::init()
{
    m_bus = std::make_unique<PrivateBus>();
    QVERIFY(m_bus->start());
    // The fake serves on its own thread: the writer blocks by design, so a
    // same-thread fake would never get to answer.
    m_logindHost = std::make_unique<QindaQt::Tests::FakeLogindBacklightServiceThread>(
        m_bus->address);
    QVERIFY(m_logindHost->isReady());
    m_logind = m_logindHost->service();
    QVERIFY(m_logind != nullptr);
    m_writer = std::make_unique<Upstream::LogindBacklightWriter>(
        m_bus->openConnection(QStringLiteral("writer")));
    m_writer->setSubjectUid(kSubjectUid);
}

void PowerLogindBacklightWriterTests::cleanup()
{
    m_writer.reset();
    m_logind = nullptr;
    m_logindHost.reset();
    m_bus.reset();
}

void PowerLogindBacklightWriterTests::usesAutoSessionWhenItReportsASeat()
{
    m_logind->setAutoSession(QStringLiteral("seat0"), true);
    // A decoy the writer must not need: resolution stops at `auto`.
    m_logind->setSessions({session(QStringLiteral("31"), kSubjectUid,
                                   QStringLiteral("seat0"),
                                   QStringLiteral("/org/freedesktop/login1/session/_31"),
                                   true)});

    QVERIFY(m_writer->available());
    QCOMPARE(m_writer->resolvedSessionPath(), QString::fromLatin1(kAutoPath));
    const Upstream::BacklightWriteOutcome outcome =
        m_writer->write(QString::fromLatin1(kDevice), 32000);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Succeeded);
    QCOMPARE(outcome.reasonCode, QStringLiteral("applied"));

    const QList<FakeLogindBacklightService::BrightnessCall> calls =
        m_logind->brightnessCalls();
    QCOMPARE(calls.size(), 1);
    const FakeLogindBacklightService::BrightnessCall &call = calls.constFirst();
    QCOMPARE(call.sessionPath, QString::fromLatin1(kAutoPath));
    QCOMPARE(call.subsystem, QStringLiteral("backlight"));
    QCOMPARE(call.deviceName, QString::fromLatin1(kDevice));
    QCOMPARE(call.value, quint32(32000));
    // `auto` answered, so the manager was never asked for the session list.
    QCOMPARE(m_logind->listSessionsCalls(), 0);
}

void PowerLogindBacklightWriterTests::fallsBackToTheActiveSeatSessionOfTheSubject()
{
    // Exactly the laptop-over-ssh shape: the caller's own session has no seat.
    m_logind->setAutoSession(QString(), false);
    m_logind->setSessions({
        session(QStringLiteral("5"), kOtherUid, QStringLiteral("seat0"),
                QStringLiteral("/org/freedesktop/login1/session/_5"), true),
        session(QStringLiteral("60"), kSubjectUid, QString(),
                QStringLiteral("/org/freedesktop/login1/session/_60"), true),
        session(QStringLiteral("100"), kSubjectUid, QStringLiteral("seat0"),
                QStringLiteral("/org/freedesktop/login1/session/_100"), false),
        session(QStringLiteral("120"), kSubjectUid, QStringLiteral("seat0"),
                QStringLiteral("/org/freedesktop/login1/session/_120"), true),
    });

    QVERIFY(m_writer->available());
    QCOMPARE(m_writer->resolvedSessionPath(),
             QStringLiteral("/org/freedesktop/login1/session/_120"));
    QCOMPARE(m_writer->write(QString::fromLatin1(kDevice), 1).status,
             Upstream::BacklightWriteStatus::Succeeded);
    QCOMPARE(m_logind->brightnessCalls().size(), 1);
    QCOMPARE(m_logind->brightnessCalls().constFirst().sessionPath,
             QStringLiteral("/org/freedesktop/login1/session/_120"));
    // Resolution is cached: a second write does not re-list.
    QCOMPARE(m_logind->listSessionsCalls(), 1);
    QCOMPARE(m_writer->write(QString::fromLatin1(kDevice), 2).status,
             Upstream::BacklightWriteStatus::Succeeded);
    QCOMPARE(m_logind->listSessionsCalls(), 1);
}

void PowerLogindBacklightWriterTests::prefersAnySeatedSubjectSessionWhenNoneIsActive()
{
    m_logind->setAutoSession(QString(), false);
    m_logind->setSessions({
        session(QStringLiteral("100"), kSubjectUid, QStringLiteral("seat0"),
                QStringLiteral("/org/freedesktop/login1/session/_100"), false),
        session(QStringLiteral("101"), kSubjectUid, QStringLiteral("seat0"),
                QStringLiteral("/org/freedesktop/login1/session/_101"), false),
    });

    QVERIFY(m_writer->available());
    QCOMPARE(m_writer->resolvedSessionPath(),
             QStringLiteral("/org/freedesktop/login1/session/_100"));
}

void PowerLogindBacklightWriterTests::unavailableWhenTheSubjectHasNoSeatSession()
{
    m_logind->setAutoSession(QString(), false);
    m_logind->setSessions({
        session(QStringLiteral("5"), kOtherUid, QStringLiteral("seat0"),
                QStringLiteral("/org/freedesktop/login1/session/_5"), true),
        session(QStringLiteral("60"), kSubjectUid, QString(),
                QStringLiteral("/org/freedesktop/login1/session/_60"), true),
    });

    QVERIFY(!m_writer->available());
    QVERIFY(m_writer->resolvedSessionPath().isEmpty());
    QCOMPARE(m_writer->unavailableDiagnostic(), QStringLiteral("logind-unavailable"));
    const Upstream::BacklightWriteOutcome outcome =
        m_writer->write(QString::fromLatin1(kDevice), 10);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Failed);
    QCOMPARE(outcome.reasonCode, QStringLiteral("logind-unavailable"));
    // Nothing was written anywhere.
    QVERIFY(m_logind->brightnessCalls().isEmpty());
}

void PowerLogindBacklightWriterTests::failedProbeIsNotRepeatedOnEveryCall()
{
    m_logind->setAutoSession(QString(), false);
    m_logind->setSessions({});

    QVERIFY(!m_writer->available());
    QCOMPARE(m_logind->listSessionsCalls(), 1);
    // AGENT-GUARD: SysfsBacklightSource::rescan() calls available() on every
    // rescan, and rescan follows every write. A negative answer must not turn
    // into a bus round trip per rescan.
    for (int attempt = 0; attempt < 5; ++attempt) {
        QVERIFY(!m_writer->available());
    }
    QCOMPARE(m_logind->listSessionsCalls(), 1);
}

void PowerLogindBacklightWriterTests::refusalIsReportedAndKeepsTheResolvedSession()
{
    m_logind->setAutoSession(QStringLiteral("seat0"), true);
    m_logind->setBrightnessError(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                                 QStringLiteral("Fake refusal"));

    const Upstream::BacklightWriteOutcome outcome =
        m_writer->write(QString::fromLatin1(kDevice), 5);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Failed);
    QCOMPARE(outcome.reasonCode, QStringLiteral("logind-refused"));
    QVERIFY(!outcome.diagnostic.isEmpty());
    // A refusal is about the request, not about the session, so nothing is
    // re-resolved and the call is not retried.
    QCOMPARE(m_writer->resolvedSessionPath(), QString::fromLatin1(kAutoPath));
    QCOMPARE(m_logind->brightnessCalls().size(), 1);
}

void PowerLogindBacklightWriterTests::staleSessionIsReResolvedOnceAndRetried()
{
    m_logind->setAutoSession(QString(), false);
    m_logind->setSessions({session(QStringLiteral("31"), kSubjectUid,
                                   QStringLiteral("seat0"),
                                   QStringLiteral("/org/freedesktop/login1/session/_31"),
                                   true)});
    QCOMPARE(m_writer->write(QString::fromLatin1(kDevice), 1).status,
             Upstream::BacklightWriteStatus::Succeeded);
    QCOMPARE(m_writer->resolvedSessionPath(),
             QStringLiteral("/org/freedesktop/login1/session/_31"));

    // The user logged out and back in: the cached object is gone and a new
    // session owns the seat.
    m_logind->setUnknownSessionPath(QStringLiteral("/org/freedesktop/login1/session/_31"));
    m_logind->setSessions({session(QStringLiteral("77"), kSubjectUid,
                                   QStringLiteral("seat0"),
                                   QStringLiteral("/org/freedesktop/login1/session/_77"),
                                   true)});

    QCOMPARE(m_writer->write(QString::fromLatin1(kDevice), 7).status,
             Upstream::BacklightWriteStatus::Succeeded);
    QCOMPARE(m_writer->resolvedSessionPath(),
             QStringLiteral("/org/freedesktop/login1/session/_77"));
    const QList<FakeLogindBacklightService::BrightnessCall> calls =
        m_logind->brightnessCalls();
    QCOMPARE(calls.size(), 2);
    const FakeLogindBacklightService::BrightnessCall &retry = calls.constLast();
    QCOMPARE(retry.sessionPath, QStringLiteral("/org/freedesktop/login1/session/_77"));
    QCOMPARE(retry.value, quint32(7));
}

void PowerLogindBacklightWriterTests::rejectsAnEmptyDeviceName()
{
    m_logind->setAutoSession(QStringLiteral("seat0"), true);
    const Upstream::BacklightWriteOutcome outcome = m_writer->write(QString(), 5);
    QCOMPARE(outcome.status, Upstream::BacklightWriteStatus::Rejected);
    QCOMPARE(outcome.reasonCode, QStringLiteral("unknown-device"));
    QVERIFY(m_logind->brightnessCalls().isEmpty());
}

void PowerLogindBacklightWriterTests::disconnectedBusIsUnavailable()
{
    Upstream::LogindBacklightWriter orphan(
        QDBusConnection(QStringLiteral("power1-logind-writer-unused")));
    QVERIFY(!orphan.available());
    QCOMPARE(orphan.unavailableDiagnostic(), QStringLiteral("logind-unavailable"));
    QCOMPARE(orphan.write(QString::fromLatin1(kDevice), 1).reasonCode,
             QStringLiteral("logind-unavailable"));
}

QTEST_MAIN(PowerLogindBacklightWriterTests)
#include "tst_power_logind_backlight_writer.moc"
