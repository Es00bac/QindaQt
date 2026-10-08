// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../../src/services/bluetooth_radio_helper/src/radio_operation_p.h"
#include <QtTest/QTest>
#include <functional>

using namespace QindaQt::BluetoothRadio;
namespace {
Request request() {
    return {QString(32, QLatin1Char('a')), QStringLiteral(":1.20"),
        QStringLiteral("/org/bluez/hci0"), QStringLiteral("12:34:56:78:9A:BC"),
        QStringLiteral(":1.30"), 2100};
}
struct Authority final : RadioAuthority {
    int calls = 0;
    QString owner = QStringLiteral(":1.10");
    std::function<bool(int)> check = [](int) { return true; };
    bool current(const QString &sender, const Request &) override { return sender == owner && check(++calls); }
};
struct State {
    RadioObservation observed{true, true, false};
    RadioWrite write = RadioWrite::Attempted;
    int writes = 0, observations = 0, selections = 0;
    bool selected = true;
    QString refusal = QStringLiteral("radio-observation-unavailable");
    std::function<void()> afterWrite;
};
class Lease final : public RadioLease {
public:
    explicit Lease(State &state) : s(state) {}
    RadioObservation observe() override { ++s.observations; return s.observed; }
    RadioWrite unblock(const std::function<bool()> &current) override {
        if (!current()) return RadioWrite::Denied;
        ++s.writes;
        if (s.write == RadioWrite::Attempted) s.observed.softBlocked = false;
        if (s.afterWrite) s.afterWrite();
        return s.write;
    }
private:
    State &s;
};
struct Platform final : RadioPlatform {
    State s;
    RadioSelection select(const QString &) override {
        ++s.selections;
        return s.selected ? RadioSelection{std::make_unique<Lease>(s), {}}
                          : RadioSelection{{}, s.refusal};
    }
};
struct Fixture {
    Authority authority;
    Platform platform;
    quint64 now = 100;
    RadioOperation operation{authority, platform, [this] { return now; }};
    Result run(const Request &value = request()) {
        return operation.execute(QStringLiteral(":1.10"), value);
    }
};
}
class RadioOperationTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void explicitSelectedWriteOnce() {
        Fixture f;
        QCOMPARE(f.run().disposition, Disposition::VerifiedUnblocked);
        QCOMPARE(f.platform.s.writes, 1);
        QCOMPARE(f.run().reasonCode, QStringLiteral("radio-request-rejected"));
        QCOMPARE(f.platform.s.writes, 1);
    }
    void unexpiredOwnerNonceSurvivesABA() {
        Fixture f;
        const auto original = request();
        QCOMPARE(f.run(original).disposition, Disposition::VerifiedUnblocked);
        f.authority.owner = QStringLiteral(":1.11");
        auto other = original; other.nonce = QString(32, QLatin1Char('b'));
        f.platform.s.observed.softBlocked = true;
        QCOMPARE(f.operation.execute(f.authority.owner, other).disposition,
                 Disposition::VerifiedUnblocked);
        f.authority.owner = QStringLiteral(":1.10");
        f.platform.s.observed.softBlocked = true;
        const auto replay = f.run(original);
        QCOMPARE(replay.disposition, Disposition::Refused);
        QCOMPARE(replay.reasonCode, QStringLiteral("radio-request-rejected"));
        QCOMPARE(f.platform.s.selections, 2);
        QCOMPARE(f.platform.s.writes, 2);
    }
    void sameNonceForDifferentOwnersRetainsBothEntries() {
        Fixture f;
        QCOMPARE(f.run().disposition, Disposition::VerifiedUnblocked);
        f.authority.owner = QStringLiteral(":1.11");
        f.platform.s.observed.softBlocked = true;
        QCOMPARE(f.operation.execute(f.authority.owner, request()).disposition,
                 Disposition::VerifiedUnblocked);
        f.authority.owner = QStringLiteral(":1.10");
        QCOMPARE(f.run().reasonCode, QStringLiteral("radio-request-rejected"));
        QCOMPARE(f.platform.s.writes, 2);
    }
    void globalBoundRefusesWithoutEvictingUnexpiredOwner() {
        Fixture f; f.platform.s.selected = false;
        for (int index = 0; index < 512; ++index) {
            auto next = request();
            next.nonce = QString::number(index, 16).rightJustified(32, QLatin1Char('0'));
            QCOMPARE(f.run(next).disposition, Disposition::NoWriteUnavailable);
        }
        f.authority.owner = QStringLiteral(":1.11");
        QCOMPARE(f.operation.execute(f.authority.owner, request()).reasonCode,
                 QStringLiteral("radio-busy"));
        QCOMPARE(f.platform.s.selections, 512);
        f.now = 2100;
        auto fresh = request(); fresh.deadlineBoottimeMs = 4100;
        QCOMPARE(f.operation.execute(f.authority.owner, fresh).disposition,
                 Disposition::NoWriteUnavailable);
        QCOMPARE(f.platform.s.selections, 513);
        f.authority.owner = QStringLiteral(":1.10");
        QCOMPARE(f.run().disposition, Disposition::Refused);
        QCOMPARE(f.platform.s.selections, 513);
    }
    void alreadyUnblockedDoesNotWrite() {
        Fixture f; f.platform.s.observed.softBlocked = false;
        QCOMPARE(f.run().disposition, Disposition::VerifiedUnblocked);
        QCOMPARE(f.platform.s.writes, 0);
    }
    void hardwareBlockDoesNotWrite() {
        Fixture f; f.platform.s.observed.hardBlocked = true;
        QCOMPARE(f.run().reasonCode, QStringLiteral("radio-hardware-blocked"));
        QCOMPARE(f.platform.s.writes, 0);
    }
    void missingObservationIsDefinitiveNoWrite() {
        Fixture f; f.platform.s.selected = false;
        QCOMPARE(f.run().disposition, Disposition::NoWriteUnavailable);
        QCOMPARE(f.platform.s.writes, 0);
    }
    void staleSelectionIsNotFallback() {
        Fixture f; f.platform.s.selected = false;
        f.platform.s.refusal = QStringLiteral("radio-stale-target");
        QCOMPARE(f.run().disposition, Disposition::Refused);
        QCOMPARE(f.platform.s.writes, 0);
    }
    void authorityLostBeforeWrite_data() {
        QTest::addColumn<int>("at");
        QTest::newRow("initial") << 1;
        QTest::newRow("after-selection") << 2;
        QTest::newRow("before-write") << 3;
        QTest::newRow("opened-writer") << 4;
    }
    void authorityLostBeforeWrite() {
        QFETCH(int, at); Fixture f;
        f.authority.check = [at](int call) { return call != at; };
        QCOMPARE(f.run().disposition, Disposition::Refused);
        QCOMPARE(f.platform.s.writes, 0);
    }
    void authorityLostAfterWriteIsUncertain() {
        Fixture f;
        f.platform.s.afterWrite = [&f] { f.authority.check = [](int) { return false; }; };
        QCOMPARE(f.run().disposition, Disposition::Uncertain);
        QCOMPARE(f.platform.s.writes, 1);
    }
    void replacementAfterWriteIsUncertain() {
        Fixture f;
        f.platform.s.afterWrite = [&f] { f.platform.s.observed.current = false; };
        QCOMPARE(f.run().disposition, Disposition::Uncertain);
        QCOMPARE(f.platform.s.writes, 1);
    }
    void stillBlockedAfterWriteIsUncertain() {
        Fixture f;
        f.platform.s.afterWrite = [&f] { f.platform.s.observed.softBlocked = true; };
        QCOMPARE(f.run().disposition, Disposition::Uncertain);
    }
    void refusedOpenIsNoRetry() {
        Fixture f; f.platform.s.write = RadioWrite::Denied;
        QCOMPARE(f.run().reasonCode, QStringLiteral("radio-not-authorized"));
        QCOMPARE(f.platform.s.writes, 1); // one injected attempt, no second attempt
    }
    void expiryDuringAdmissionDoesNotSelect() {
        Fixture f;
        f.authority.check = [&f](int) { f.now = 2100; return true; };
        QCOMPARE(f.run().disposition, Disposition::Refused);
        QCOMPARE(f.platform.s.selections, 0);
    }
    void futureExpiryAndMalformedTargetRejected() {
        Fixture f; auto value = request(); value.deadlineBoottimeMs = 99999;
        QCOMPARE(f.run(value).disposition, Disposition::Refused);
        value = request(); value.adapterPath = QStringLiteral("/org/bluez/hci0/../hci1");
        QCOMPARE(f.run(value).disposition, Disposition::Refused);
        QCOMPARE(f.platform.s.selections, 0);
    }
    void expiredReplayDoesNotSelect() {
        Fixture f; QCOMPARE(f.run().disposition, Disposition::VerifiedUnblocked);
        f.now = 2100;
        QCOMPARE(f.run().disposition, Disposition::Refused);
        QCOMPARE(f.platform.s.selections, 1);
    }
};
QTEST_GUILESS_MAIN(RadioOperationTest)
#include "tst_radio_operation.moc"
