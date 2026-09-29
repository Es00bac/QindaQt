// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/secure_buffer.h>
#include <QTest>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <type_traits>
#include <algorithm>

using qindaqt::keyring::SecureBuffer;
static_assert(!std::is_copy_constructible_v<SecureBuffer>);
static_assert(!std::is_copy_assignable_v<SecureBuffer>);
class SecureBufferTest : public QObject {
    Q_OBJECT
private slots:
    void wipeAndReuse() {
        SecureBuffer buffer(31);
        std::fill(buffer.bytes().begin(), buffer.bytes().end(), 0xa5);
        buffer.wipe();
        QVERIFY(std::all_of(buffer.bytes().begin(), buffer.bytes().end(), [](auto b) { return b == 0; }));
        buffer.clear();
        QCOMPARE(buffer.size(), std::size_t(0));
        QVERIFY(buffer.bytes().empty());
        buffer.clear();
    }
    void moveTransfersOwnership() {
        SecureBuffer source(17);
        source.bytes()[0] = 0x42;
        const auto *address = source.bytes().data();
        SecureBuffer moved(std::move(source));
        QVERIFY(source.bytes().empty());
        QCOMPARE(moved.bytes().data(), address);
        SecureBuffer destination(7);
        destination = std::move(moved);
        QVERIFY(moved.bytes().empty());
        QCOMPARE(destination.bytes().data(), address);
        QCOMPARE(destination.bytes()[0], static_cast<unsigned char>(0x42));
        auto *same = &destination;
        destination = std::move(*same);
        QCOMPARE(destination.bytes().data(), address);
    }
    void forkDoesNotInheritSecretPages() {
        SecureBuffer parent(17);
        parent.bytes()[0] = 0x42;
        const auto pid = fork();
        QVERIFY(pid >= 0);
        if (pid == 0) {
            if (!parent.bytes().empty() || parent.size() != 0) _exit(2);
            parent.clear(); // Must not touch an absent MADV_DONTFORK mapping.
            _exit(0);
        }
        int status = 0;
        QCOMPARE(waitpid(pid, &status, 0), pid);
        QVERIFY(WIFEXITED(status));
        QCOMPARE(WEXITSTATUS(status), 0);
        QCOMPARE(parent.bytes()[0], static_cast<unsigned char>(0x42));
    }
    void refusesUnlockedMemory() {
        const auto pid = fork();
        QVERIFY(pid >= 0);
        if (pid == 0) {
            struct rlimit limit{0,0};
            if (setrlimit(RLIMIT_MEMLOCK, &limit) != 0) _exit(2);
            try { SecureBuffer forbidden(16); _exit(3); }
            catch (const std::runtime_error &) { _exit(0); }
        }
        int status = 0;
        QCOMPARE(waitpid(pid, &status, 0), pid);
        QVERIFY(WIFEXITED(status));
        QCOMPARE(WEXITSTATUS(status), 0);
    }
    void pagesAreLockedAndExcludedFromDump() {
        SecureBuffer buffer(17);
        QFile smaps("/proc/self/smaps");
        QVERIFY(smaps.open(QIODevice::ReadOnly));
        const auto lines = smaps.readAll().split('\n');
        const auto address = reinterpret_cast<quintptr>(buffer.bytes().data());
        bool inMapping = false, checked = false;
        for (const auto &line : lines) {
            if (line.contains('-') && line.contains(" rw")) {
                const auto range = line.split(' ').front().split('-');
                if (range.size() == 2)
                    inMapping = address >= range[0].toULongLong(nullptr, 16)
                        && address < range[1].toULongLong(nullptr, 16);
            } else if (inMapping && line.startsWith("VmFlags:")) {
                QVERIFY(line.split(' ').contains("lo"));
                QVERIFY(line.split(' ').contains("dd"));
                checked = true; break;
            }
        }
        QVERIFY(checked);
    }
};
QTEST_GUILESS_MAIN(SecureBufferTest)
#include "tst_secure_buffer.moc"
