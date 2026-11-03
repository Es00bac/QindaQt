// SPDX-License-Identifier: GPL-3.0-or-later
// CallerPolicy decisions (fake process facts) and SendGuard (fake clock and presenter).
#include <qindaqt/services/link_service/caller_policy.h>
#include <qindaqt/services/link_service/send_guard.h>

#include <QtTest/QtTest>

using namespace QindaQt::Link;

namespace {

struct FakeInspector final : ProcessInspector {
    bool pidOf(const QString &name, uint *pid) override
    {
        if (!pids.contains(name)) return false;
        *pid = pids.value(name);
        return true;
    }
    QString executableOf(uint pid) override { return exes.value(pid); }
    qulonglong startTimeOf(uint pid) override
    {
        const qulonglong t = starts.value(pid);
        if (flipStart) return t + (++calls);  // a different incarnation on each read
        return t;
    }
    bool executableFileId(uint pid, quint64 *dev, quint64 *ino) override
    {
        if (!exeIds.contains(pid)) return false;
        *dev = 1;
        *ino = exeIds.value(pid);
        return true;
    }
    bool fileIdOfPath(const QString &path, quint64 *dev, quint64 *ino) override
    {
        if (!pathIds.contains(path)) return false;
        *dev = 1;
        *ino = pathIds.value(path);
        return true;
    }
    QString mountNamespaceOf(uint pid) override { return namespaces.value(pid, QStringLiteral("mnt:[1]")); }
    QString ownMountNamespace() override { return QStringLiteral("mnt:[1]"); }
    QHash<QString, uint> pids;
    QHash<uint, quint64> exeIds;                 // inode behind /proc/<pid>/exe
    QHash<QString, quint64> pathIds;             // inode of a path in the service namespace
    QHash<uint, QString> namespaces;
    QHash<uint, QString> exes;
    QHash<uint, qulonglong> starts;
    bool flipStart = false;
    qulonglong calls = 0;
};

class FakePresenter final : public NotificationPresenter {
public:
    quint32 present(const PresentedNotification &n, quint32) override
    {
        shown.append(n);
        return quint32(shown.size());
    }
    void close(quint32) override {}
    QList<PresentedNotification> shown;
};

CallerPolicy makePolicy(FakeInspector **out, const QStringList &trusted)
{
    auto inspector = std::make_unique<FakeInspector>();
    *out = inspector.get();
    return CallerPolicy(QDBusConnection(QStringLiteral("policy-test")), std::move(inspector), trusted);
}

} // namespace

class LinkCallerPolicyTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void trustedUntrustedDeletedAndUnknown()
    {
        FakeInspector *f = nullptr;
        CallerPolicy policy = makePolicy(&f, {QStringLiteral("/opt/qindaqt/bin/qindaqt-link")});
        f->pathIds = {{QStringLiteral("/opt/qindaqt/bin/qindaqt-link"), 10}};
        f->pids = {{QStringLiteral(":1.1"), 100}, {QStringLiteral(":1.2"), 200}, {QStringLiteral(":1.3"), 300}};
        f->exes = {{100, QStringLiteral("/opt/qindaqt/bin/qindaqt-link")},
                   {200, QStringLiteral("/usr/bin/evil")},
                   {300, QStringLiteral("/opt/qindaqt/bin/qindaqt-link (deleted)")}};
        f->exeIds = {{100, 10}, {200, 20}, {300, 10}};
        f->starts = {{100, 5}, {200, 6}, {300, 7}};
        QString name;
        QVERIFY(policy.isTrusted(QStringLiteral(":1.1"), &name));
        QCOMPARE(name, QStringLiteral("qindaqt-link"));
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.2")));
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.3")));     // replaced binary
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.99")));    // unknown peer
        QVERIFY(!policy.isTrusted(QString()));                  // no sender
    }

    void clientIsNotShell()
    {
        // ADR-0440: the shell role is a smaller set. An executable trusted as a client (the QindaLink app)
        // must be denied for Role::Shell; only the shell executable passes both.
        auto inspector = std::make_unique<FakeInspector>();
        FakeInspector *f = inspector.get();
        CallerPolicy policy(QDBusConnection(QStringLiteral("policy-test-shell")), std::move(inspector),
                            {QStringLiteral("/opt/qindaqt/bin/qindaqt-link"), QStringLiteral("/opt/qindaqt/bin/qindaqt-shell")},
                            {QStringLiteral("/opt/qindaqt/bin/qindaqt-shell")});
        f->pathIds = {{QStringLiteral("/opt/qindaqt/bin/qindaqt-link"), 10}, {QStringLiteral("/opt/qindaqt/bin/qindaqt-shell"), 11}};
        f->pids = {{QStringLiteral(":1.1"), 100}, {QStringLiteral(":1.2"), 200}, {QStringLiteral(":1.3"), 300}};
        f->exeIds = {{100, 10}, {200, 11}, {300, 99}};
        f->exes = {{100, QStringLiteral("/opt/qindaqt/bin/qindaqt-link")}, {200, QStringLiteral("/opt/qindaqt/bin/qindaqt-shell")},
                   {300, QStringLiteral("/usr/bin/other")}};
        f->starts = {{100, 1}, {200, 1}, {300, 1}};
        QVERIFY(policy.isTrusted(QStringLiteral(":1.1")));   // the app is a client...
        QVERIFY(!policy.isShell(QStringLiteral(":1.1")));    // ...but not the shell
        QVERIFY(policy.isTrusted(QStringLiteral(":1.2")));
        QVERIFY(policy.isShell(QStringLiteral(":1.2")));
        QVERIFY(!policy.isShell(QStringLiteral(":1.3")));
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.3")));
        // A policy built without a shell set trusts no shell at all.
        auto inspector2 = std::make_unique<FakeInspector>();
        FakeInspector *g = inspector2.get();
        CallerPolicy none(QDBusConnection(QStringLiteral("policy-test-noshell")), std::move(inspector2),
                          {QStringLiteral("/opt/qindaqt/bin/qindaqt-shell")});
        g->pathIds = f->pathIds; g->pids = f->pids; g->exeIds = f->exeIds; g->exes = f->exes; g->starts = f->starts;
        QVERIFY(none.isTrusted(QStringLiteral(":1.2")));
        QVERIFY(!none.isShell(QStringLiteral(":1.2")));
    }

    void bindMountedPathStringDoesNotMatter()
    {
        // unshare -Urm plus a bind mount makes readlink(/proc/pid/exe) read the trusted path while the inode
        // (and the mount namespace) differ. Identity decides, not the string.
        FakeInspector *f = nullptr;
        CallerPolicy policy = makePolicy(&f, {QStringLiteral("/usr/bin/qindaqt-link")});
        f->pathIds = {{QStringLiteral("/usr/bin/qindaqt-link"), 10}};
        f->pids = {{QStringLiteral(":1.1"), 100}, {QStringLiteral(":1.2"), 200}};
        f->exes = {{100, QStringLiteral("/usr/bin/qindaqt-link")}, {200, QStringLiteral("/usr/bin/qindaqt-link")}};
        f->starts = {{100, 5}, {200, 6}};
        f->exeIds = {{100, 666}, {200, 10}};          // 100 runs an attacker file; 200 has the right inode...
        f->namespaces = {{200, QStringLiteral("mnt:[999]")}};  // ...but from another mount namespace
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.1")));
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.2")));
        f->namespaces.clear();
        QVERIFY(policy.isTrusted(QStringLiteral(":1.2")));  // same inode, same namespace: trusted
    }

    void packageUpgradeIsPickedUpOnTheNextCheck()
    {
        FakeInspector *f = nullptr;
        CallerPolicy policy = makePolicy(&f, {QStringLiteral("/usr/bin/qindaqt-link")});
        f->pathIds = {{QStringLiteral("/usr/bin/qindaqt-link"), 10}};
        f->pids = {{QStringLiteral(":1.1"), 100}};
        f->exes = {{100, QStringLiteral("/usr/bin/qindaqt-link")}};
        f->exeIds = {{100, 10}};
        f->starts = {{100, 5}};
        QVERIFY(policy.isTrusted(QStringLiteral(":1.1")));
        f->pathIds[QStringLiteral("/usr/bin/qindaqt-link")] = 11;  // the file was replaced
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.1")));
        f->exeIds[100] = 11;
        QVERIFY(policy.isTrusted(QStringLiteral(":1.1")));
    }

    void unreadableProcessFailsClosed()
    {
        FakeInspector *f = nullptr;
        CallerPolicy policy = makePolicy(&f, {QStringLiteral("/opt/x/app")});
        f->pathIds = {{QStringLiteral("/opt/x/app"), 1}};
        f->exeIds = {{100, 1}};
        f->pids = {{QStringLiteral(":1.1"), 100}};
        f->exes = {{100, QStringLiteral("/opt/x/app")}};
        // No start time (process gone): refuse even though the exe matched.
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.1")));
        f->exes.clear();
        f->starts = {{100, 5}};
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.1")));
    }

    void pidReuseBetweenReadsFailsClosed()
    {
        FakeInspector *f = nullptr;
        CallerPolicy policy = makePolicy(&f, {QStringLiteral("/opt/x/app")});
        f->pathIds = {{QStringLiteral("/opt/x/app"), 1}};
        f->exeIds = {{100, 1}};
        f->pids = {{QStringLiteral(":1.1"), 100}};
        f->exes = {{100, QStringLiteral("/opt/x/app")}};
        f->starts = {{100, 5}};
        QVERIFY(policy.isTrusted(QStringLiteral(":1.1")));
        f->flipStart = true;
        QVERIFY(!policy.isTrusted(QStringLiteral(":1.1")));
    }

    void rateLimitPerMinuteAndHour()
    {
        qint64 now = 0;
        FakePresenter presenter;
        SendGuard guard(&presenter, [&] { return now; });
        for (int i = 0; i < SendGuard::kPerMinute; ++i) {
            QCOMPARE(guard.admit(QStringLiteral("dev"), 1, QStringLiteral("qindaqt-link")), QString());
            now += 1000;
        }
        QCOMPARE(guard.admit(QStringLiteral("dev"), 1, QStringLiteral("qindaqt-link")), QStringLiteral("rate-limited"));
        QCOMPARE(presenter.shown.size(), SendGuard::kPerMinute);  // a refused send announces nothing
        // Another device has its own budget.
        QCOMPARE(guard.admit(QStringLiteral("other"), 1, QStringLiteral("x")), QString());
        // After a minute the per-minute window reopens, but the hour budget (30) still binds.
        int accepted = SendGuard::kPerMinute;
        for (int minute = 1; minute < 20 && accepted < SendGuard::kPerHour; ++minute) {
            now += 61'000;
            for (int i = 0; i < SendGuard::kPerMinute && accepted < SendGuard::kPerHour; ++i, ++accepted) {
                QCOMPARE(guard.admit(QStringLiteral("dev"), 1, QStringLiteral("a")), QString());
            }
        }
        QCOMPARE(accepted, SendGuard::kPerHour);
        now += 61'000;
        QCOMPARE(guard.admit(QStringLiteral("dev"), 1, QStringLiteral("a")), QStringLiteral("rate-limited"));
        now += 3'600'000;
        QCOMPARE(guard.admit(QStringLiteral("dev"), 1, QStringLiteral("a")), QString());
    }

    void unknownDevicesAreRefusedAndDoNotGrowTheTable()
    {
        FakePresenter presenter;
        SendGuard guard(&presenter, [] { return qint64(0); }, [](const QString &id) { return id == QLatin1String("known"); });
        for (int i = 0; i < 1000; ++i) {
            QCOMPARE(guard.admit(QStringLiteral("bogus%1").arg(i), 1, QStringLiteral("x")), QStringLiteral("not-paired"));
        }
        QCOMPARE(guard.bucketCount(), 0);
        QCOMPARE(presenter.shown.size(), 0);
        QCOMPARE(guard.admit(QStringLiteral("known"), 1, QStringLiteral("x")), QString());
        QCOMPARE(guard.bucketCount(), 1);
    }

    void everyAcceptedSendIsAnnounced()
    {
        FakePresenter presenter;
        SendGuard guard(&presenter, [] { return qint64(0); });
        QCOMPARE(guard.admit(QStringLiteral("dev"), 3, QStringLiteral("evil<b>")), QString());
        QCOMPARE(presenter.shown.size(), 1);
        QCOMPARE(presenter.shown[0].summary, QStringLiteral("QindaLink sent a text to 3 recipient(s)"));
        QVERIFY(presenter.shown[0].body.contains(QLatin1String("Requested by evil&lt;b&gt;")));
        QCOMPARE(presenter.shown[0].appName, QStringLiteral("QindaLink"));
    }
};

QTEST_GUILESS_MAIN(LinkCallerPolicyTest)
#include "tst_link_caller_policy.moc"
