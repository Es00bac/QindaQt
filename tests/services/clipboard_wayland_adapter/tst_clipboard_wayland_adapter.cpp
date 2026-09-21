// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_data_control_server.h"

#include <qindaqt/services/clipboard_wayland_adapter/production_clipboard_wayland_adapter.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtTest/QTest>

using namespace QindaQt::Services;

class Observer final : public ClipboardWayland::CaptureObserver {
public:
    void captureAvailabilityChanged(bool value) override { available = value; }
    void captured(ClipboardWayland::SelectionKind value,
                  const ClipboardModel::ClipboardValue &clipboardValue) override
    { kind = value; payload = clipboardValue; ++captureCount; }
    void captureRefused(ClipboardWayland::SelectionKind,
                        ClipboardModel::ClipboardError error) override
    { refusal = error; ++refusalCount; }
    ClipboardModel::ClipboardValue payload;
    ClipboardModel::ClipboardError refusal = ClipboardModel::ClipboardError::None;
    ClipboardWayland::SelectionKind kind = ClipboardWayland::SelectionKind::Clipboard;
    int captureCount = 0;
    int refusalCount = 0;
    bool available = false;
};

class ClipboardWaylandAdapterTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init()
    {
        m_observer = Observer{};
        // AGENT-GUARD: a Wayland socket is an AF_UNIX path, and sun_path is
        // 108 bytes INCLUDING the terminator. This directory used to be
        // derived from the build root, so any build tree with a longish path
        // made the fake compositor refuse to bind with
        //   socket path "..." plus null terminator exceeds 108 bytes
        // and the whole row failed for a reason that had nothing to do with
        // the clipboard. Anchor it somewhere short instead: the session's own
        // XDG_RUNTIME_DIR is where Wayland sockets belong and is short by
        // construction, with /tmp as the fallback for a session that has
        // none. The pid keeps two concurrent runs apart.
        // AGENT-GUARD: read the session's XDG_RUNTIME_DIR exactly ONCE.
        // init() runs per row and sets that variable itself, so re-reading it
        // nests each row's directory inside the previous one
        // (/tmp/x/x/x/wayland-...) and walks straight back over the 108-byte
        // limit this is here to stay under.
        static const QString socketRoot = [] {
            const QString session = qEnvironmentVariable("XDG_RUNTIME_DIR");
            return (session.isEmpty() || session.size() > 64)
                       ? QStringLiteral("/tmp")
                       : session;
        }();
        const QString runtime =
            QDir(socketRoot)
                .filePath(QStringLiteral("qindaqt-wl-clipboard-%1")
                              .arg(QCoreApplication::applicationPid()));
        QDir(runtime).removeRecursively();
        m_runtime = runtime;
        QVERIFY(QDir().mkpath(runtime));
        QVERIFY(QFile::setPermissions(runtime, QFile::ReadOwner | QFile::WriteOwner
                                              | QFile::ExeOwner));
        qputenv("XDG_RUNTIME_DIR", runtime.toUtf8());
        m_server = std::make_unique<QindaQt::Tests::FakeDataControlServer>();
        QVERIFY(m_server->start());
        qputenv("WAYLAND_DISPLAY", m_server->socketName().toUtf8());
        m_adapter = ClipboardWayland::makeProductionClipboardWaylandAdapter();
        m_adapter->setObserver(&m_observer);
        QCOMPARE(m_adapter->start(), ClipboardWayland::StartStatus::Started);
        QTRY_VERIFY(m_server->hasDevice());
        QTRY_VERIFY(m_observer.available);
        m_adapter->setCaptureEnabled(true);
    }

    void cleanup()
    {
        m_adapter->stop();
        m_adapter.reset();
        m_server.reset();
        // The socket directory is outside the build tree now, so it is this
        // test's job to take it away again.
        if (!m_runtime.isEmpty()) {
            QDir(m_runtime).removeRecursively();
            m_runtime.clear();
        }
    }

    void capturesOnlyAdmittedMediaAndPrimary()
    {
        m_server->sendOffer({{QStringLiteral("application/octet-stream"), QByteArrayLiteral("ignored")},
                            {QStringLiteral("text/plain"), QByteArrayLiteral("accepted")}}, true);
        QTRY_COMPARE(m_observer.captureCount, 1);
        QCOMPARE(m_observer.kind, ClipboardWayland::SelectionKind::Primary);
        QCOMPARE(m_observer.payload.formats.size(), 1);
        QCOMPARE(m_observer.payload.formats.first().mediaType, QStringLiteral("text/plain"));
        QCOMPARE(m_server->receivedTypes(), QStringList{QStringLiteral("text/plain")});
    }

    void refusesSensitiveWithoutReadingPayload()
    {
        m_server->sendOffer({{QStringLiteral("x-kde-passwordmanagerhint"), QByteArrayLiteral("secret")},
                            {QStringLiteral("text/plain"), QByteArrayLiteral("ordinary")}}, false);
        QTRY_COMPARE(m_observer.refusalCount, 1);
        QCOMPARE(m_observer.refusal, ClipboardModel::ClipboardError::SensitiveRefused);
        QVERIFY(m_server->receivedTypes().isEmpty());
    }

    void rejectsOversizedTransfer()
    {
        m_server->sendOffer({{QStringLiteral("text/plain"),
                             QByteArray(ClipboardModel::kMaxItemPayloadBytes + 1, 'x')}}, false);
        QTRY_COMPARE_WITH_TIMEOUT(m_observer.refusalCount, 1, 10'000);
        QCOMPARE(m_observer.refusal, ClipboardModel::ClipboardError::OversizedValue);
        QCOMPARE(m_observer.captureCount, 0);
    }

    void rejectsOverboundedMediaAdvertisementsWithoutReading()
    {
        QHash<QString, QByteArray> tooMany;
        tooMany.insert(QStringLiteral("text/plain"), QByteArrayLiteral("ordinary"));
        for (qsizetype index = 0;
             index < ClipboardWayland::kMaxAdvertisedMediaTypesPerOffer; ++index) {
            tooMany.insert(QStringLiteral("application/x-fixture-%1").arg(index),
                           QByteArrayLiteral("ignored"));
        }
        m_server->sendOffer(tooMany, false);
        QTRY_COMPARE(m_observer.refusalCount, 1);
        QCOMPARE(m_observer.refusal, ClipboardModel::ClipboardError::TooManyFormats);
        QVERIFY(m_server->receivedTypes().isEmpty());

        m_observer = Observer{};
        const QString overlong(ClipboardModel::kMaxMediaTypeLength + 1, QLatin1Char('a'));
        m_server->sendOffer({{overlong, QByteArrayLiteral("ignored")},
                             {QStringLiteral("text/plain"), QByteArrayLiteral("ordinary")}},
                            false);
        QTRY_COMPARE(m_observer.refusalCount, 1);
        QCOMPARE(m_observer.refusal, ClipboardModel::ClipboardError::TooManyFormats);
        QVERIFY(m_server->receivedTypes().isEmpty());
    }

    void boundsPendingUnselectedOffersAndKeepsNewestUsable()
    {
        for (qsizetype index = 0; index <= ClipboardWayland::kMaxPendingOffers; ++index) {
            m_server->sendUnselectedOffer(
                {{QStringLiteral("text/plain"), QByteArray::number(index)}});
        }
        m_server->sendOffer(
            {{QStringLiteral("text/plain"), QByteArrayLiteral("newest")}}, false);
        QTRY_COMPARE(m_observer.captureCount, 1);
        QCOMPARE(m_observer.payload.formats.constFirst().payload,
                 QByteArrayLiteral("newest"));
    }

    void globalRemovalWithdrawsAvailability()
    {
        m_server->removeManagerGlobal();
        QTRY_VERIFY(!m_observer.available);
        QVERIFY(!m_adapter->isAvailable());
        m_server->sendOffer(
            {{QStringLiteral("text/plain"), QByteArrayLiteral("denied")}}, false);
        QTest::qWait(20);
        QCOMPARE(m_observer.captureCount, 0);
    }

    void compositorDisconnectWithdrawsAvailability()
    {
        m_server->disconnectClient();
        QTRY_VERIFY(!m_observer.available);
        QVERIFY(!m_adapter->isAvailable());
        QCOMPARE(m_observer.captureCount, 0);
    }

private:
    QString m_runtime;
    std::unique_ptr<QindaQt::Tests::FakeDataControlServer> m_server;
    Observer m_observer;
    std::unique_ptr<ClipboardWayland::ClipboardWaylandAdapter> m_adapter;
};

QTEST_GUILESS_MAIN(ClipboardWaylandAdapterTest)
#include "tst_clipboard_wayland_adapter.moc"
