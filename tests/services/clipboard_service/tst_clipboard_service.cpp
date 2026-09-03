// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_wayland_adapter.h"

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>
#include <qindaqt/services/clipboard_service/clipboard_host.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Services;

namespace {
ClipboardModel::ClipboardValue textValue(const QByteArray &payload)
{
    return {{{QStringLiteral("text/plain"), payload}}};
}
}

class ClipboardServiceTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void optInAndLockGateCapture()
    {
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 55);
        adapter.offer(textValue("denied"));
        QVERIFY(ClipboardModel::decodeDescriptorList(host.snapshot().descriptorList).descriptors.isEmpty());
        host.setHistoryOptIn(true);
        QVERIFY(!adapter.captureEnabled);
        host.setUnlocked(true);
        QVERIFY(adapter.captureEnabled);
        adapter.offer(textValue("accepted"));
        QCOMPARE(ClipboardModel::decodeDescriptorList(host.snapshot().descriptorList).descriptors.size(), 1);
        host.setUnlocked(false);
        QVERIFY(ClipboardModel::decodeDescriptorList(host.snapshot().descriptorList).descriptors.isEmpty());
        QVERIFY(!adapter.captureEnabled);
    }

    void copyAndStaleLineage()
    {
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 77);
        host.setHistoryOptIn(true);
        host.setUnlocked(true);
        adapter.offer(textValue("synthetic payload"));
        const Clipboard::Snapshot current = host.snapshot();
        const auto entries = ClipboardModel::decodeDescriptorList(current.descriptorList).descriptors;
        QCOMPARE(entries.size(), 1);
        Clipboard::OperationRequest copy{Clipboard::OperationKind::Copy, 1, 77,
            current.generation, current.revision, entries.first().id, false};
        QCOMPARE(host.submit(copy).status, Clipboard::OperationStatus::Succeeded);
        QCOMPARE(adapter.lastPublished, textValue("synthetic payload"));
        Clipboard::OperationRequest stale{Clipboard::OperationKind::Delete, 2, 77,
            current.generation, current.revision, entries.first().id, false};
        QCOMPARE(host.submit(stale).reasonCode, QStringLiteral("stale-lineage"));
    }

    void disablingPurges()
    {
        FakeWaylandAdapter adapter;
        Clipboard::ClipboardHost host(&adapter, 12);
        host.setHistoryOptIn(true);
        host.setUnlocked(true);
        adapter.offer(textValue("ephemeral"));
        const quint32 generation = host.snapshot().generation;
        host.setHistoryOptIn(false);
        QCOMPARE(host.snapshot().generation, generation + 1);
        QVERIFY(host.snapshot().descriptorList.isEmpty()
                || ClipboardModel::decodeDescriptorList(host.snapshot().descriptorList).descriptors.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ClipboardServiceTest)
#include "tst_clipboard_service.moc"
