// SPDX-License-Identifier: GPL-3.0-or-later
#include "attachment_test_support.h"
using namespace AttachmentTestSupport;
class AttachmentTest final : public QObject {
  Q_OBJECT
private slots:
  void pinsActualOrdinaryPeerAndTransfersCloexecDescriptor();
  void initialAdvertisementDoesNotRevokeAndActualLossReattaches();
  void separateDaemonCannotReuseOwnerAndPid();
  void rejectsDeniedSelectionAndIncorrectIndependentPeer();
  void admissionLossIsImmediateAndOwnerLossFollowsTheWatcher();
  void rejectsUnsafePathsAndCanonicalNameViolations();
  void sameUidWrongProcessAndPathReplacementCannotAttach();
  void reentrantRevocationCannotReportSuccessfulAttachment();
  void kernelPeerDeathRevokesWithoutWaitingForOwnerWatcher();
  void kernelPeerDeathIsPublishedWithoutPolling();
  void selectedSessionOwnerLossRetiresTheSelection();
};
void AttachmentTest::pinsActualOrdinaryPeerAndTransfersCloexecDescriptor() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(compositor.isConnected() && session.isConnected());
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  Listener socket(runtime.filePath(
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0"));
  QVERIFY(socket.fd >= 0);
  struct stat socketInfo{};
  QVERIFY(::stat(QFile::encodeName(socket.path).constData(), &socketInfo) == 0);
  QCOMPARE(socketInfo.st_mode & 0777, mode_t(0755));
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QVERIFY(attachment.attach(
      session.baseService(), QFileInfo(socket.path).fileName(),
      PeerExpectation{compositor.baseService(), quint64(getpid())}));
  QVERIFY(attachment.live());
  QVERIFY(attachment.identity());
  QCOMPARE(attachment.identity()->compositorOwner, compositor.baseService());
  QCOMPARE(attachment.identity()->compositorPid, quint64(getpid()));
  const int transferred = attachment.openConnection();
  QVERIFY(transferred >= 3);
  QVERIFY((fcntl(transferred, F_GETFD) & FD_CLOEXEC) != 0);
  close(transferred);
  QSignalSpy revoked(&attachment, &CompositorAttachment::revoked);
  attachment.revoke();
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
  QCOMPARE(revoked.count(), 1);
}
void AttachmentTest::initialAdvertisementDoesNotRevokeAndActualLossReattaches() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session"), client = fixture.connect("client");
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  QVERIFY(socket.fd >= 0);
  CompositorAttachment attachment(
      client, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QTest::qWait(10);
  QSignalSpy revoked(&attachment, &CompositorAttachment::revoked);
  QSignalSpy attached(&attachment, &CompositorAttachment::attached);
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QVERIFY(attachment.attach(session.baseService(), name));
  QTest::qWait(150);
  QVERIFY(attachment.live());
  QVERIFY(revoked.isEmpty());
  QVERIFY(
      compositor.unregisterService(QString(QindaQt::CompositorNames::service)));
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  // Actual loss revokes even though the same owner reclaimed the name; the
  // retained selection then re-proves that owner (ADR-0379).
  QTRY_COMPARE(revoked.size(), 1);
  QTRY_COMPARE(attached.size(), 2);
  QVERIFY(attachment.live());
  QVERIFY(!attachment.recovering());
}
void AttachmentTest::separateDaemonCannotReuseOwnerAndPid() {
  Bus first, second;
  auto compositor = first.connect("compositor"),
       session = first.connect("session"), client = first.connect("client");
  auto other = second.connect("compositor"),
       otherSession = second.connect("session"),
       otherClient = second.connect("client");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QVERIFY(other.registerService(QString(QindaQt::CompositorNames::service)));
  QCOMPARE(compositor.baseService(), other.baseService());
  QCOMPARE(client.interface()->servicePid(compositor.baseService()).value(),
           otherClient.interface()->servicePid(other.baseService()).value());
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  QVERIFY(socket.fd >= 0);
  CompositorAttachment attachment(
      client, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QVERIFY(!attachment.sameBus(client));
  QVERIFY(attachment.attach(session.baseService(), name));
  QVERIFY(attachment.sameBus(client));
  QVERIFY(attachment.sameBus(session));
  QVERIFY(!attachment.sameBus(otherClient));
}
void AttachmentTest::rejectsDeniedSelectionAndIncorrectIndependentPeer() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session"), other = fixture.connect("other");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  QVERIFY(socket.fd >= 0);
  CompositorAttachment denied(compositor, runtime.path(),
                              [](const QString &) { return false; });
  QVERIFY(!denied.attach(session.baseService(), name));
  QVERIFY(!denied.identity());
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  QVERIFY(!attachment.attach(other.baseService(), name));
  QVERIFY(!attachment.attach(
      session.baseService(), name,
      PeerExpectation{other.baseService(), quint64(getpid())}));
  QVERIFY(!attachment.attach(
      session.baseService(), name,
      PeerExpectation{compositor.baseService(), quint64(getpid()) + 1}));
  QVERIFY(attachment.attach(session.baseService(), name));
}
void AttachmentTest::admissionLossIsImmediateAndOwnerLossFollowsTheWatcher() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  bool admitted = true;
  CompositorAttachment attachment(
      compositor, runtime.path(), [&](const QString &owner) {
        return admitted && owner == session.baseService();
      });
  QVERIFY(attachment.attach(session.baseService(), name));
  admitted = false;
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
  // Admission is consumer state: refusal hides the binding immediately but
  // is not compositor loss, so the attachment answers again once admitted.
  admitted = true;
  QVERIFY(attachment.live());
  QVERIFY(attachment.attach(session.baseService(), name));
  QVERIFY(
      compositor.unregisterService(QString(QindaQt::CompositorNames::service)));
  // Bus ownership is cached from NameOwnerChanged; loss is published when the
  // watcher delivers it, never by polling the daemon (ADR-0379).
  QTRY_VERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
  QVERIFY(attachment.recovering());
}
void AttachmentTest::rejectsUnsafePathsAndCanonicalNameViolations() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  for (const auto &invalid :
       {"../qindaqt-0", "qindaqt-00", "qindaqt-4096", "qindaqt-", "wayland-0"})
    QVERIFY(!attachment.attach(session.baseService(), invalid));
  const auto alias = runtime.filePath(
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "1");
  QVERIFY(QFile::link(socket.path, alias));
  QVERIFY(
      !attachment.attach(session.baseService(), QFileInfo(alias).fileName()));
  QVERIFY(::chmod(QFile::encodeName(socket.path).constData(), 0777) == 0);
  QVERIFY(!attachment.attach(session.baseService(), name));
  QVERIFY(::chmod(QFile::encodeName(socket.path).constData(), 0755) == 0);
  QVERIFY(QFile::setPermissions(
      runtime.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                          QFileDevice::ExeOwner | QFileDevice::ReadGroup));
  QVERIFY(!attachment.attach(session.baseService(), name));
  QVERIFY(QFile::setPermissions(runtime.path(), QFileDevice::ReadOwner |
                                                    QFileDevice::WriteOwner |
                                                    QFileDevice::ExeOwner));
  QVERIFY(attachment.attach(session.baseService(), name));
}
void AttachmentTest::sameUidWrongProcessAndPathReplacementCannotAttach() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  const auto path = runtime.filePath(name);
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  {
    OtherProcessListener wrong(path);
    QVERIFY(wrong.available);
    QVERIFY(!attachment.attach(session.baseService(), name));
  }
  QVERIFY(QFile::remove(path));
  Listener legitimate(path);
  QVERIFY(legitimate.fd >= 0);
  QVERIFY(attachment.attach(session.baseService(), name));
  QVERIFY(QFile::remove(path));
  OtherProcessListener replaced(path);
  QVERIFY(replaced.available);
  QCOMPARE(attachment.openConnection(), -1);
  QVERIFY(!attachment.identity());
}
void AttachmentTest::reentrantRevocationCannotReportSuccessfulAttachment() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == session.baseService(); });
  connect(&attachment, &CompositorAttachment::attached, &attachment,
          &CompositorAttachment::revoke);
  QVERIFY(!attachment.attach(session.baseService(), name));
  QVERIFY(!attachment.identity());
}
void AttachmentTest::selectedSessionOwnerLossRetiresTheSelection() {
  Bus fixture;
  auto compositor = fixture.connect("compositor"),
       session = fixture.connect("session");
  QVERIFY(
      compositor.registerService(QString(QindaQt::CompositorNames::service)));
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  Listener socket(runtime.filePath(name));
  const auto selected = session.baseService();
  CompositorAttachment attachment(
      compositor, runtime.path(),
      [&](const QString &owner) { return owner == selected; });
  QSignalSpy revoked(&attachment, &CompositorAttachment::revoked);
  QVERIFY(attachment.attach(selected, name));
  QDBusConnection::disconnectFromBus(session.name());
  session = QDBusConnection(QStringLiteral("disconnected-fixture"));
  // The selected unique owner can never return, so its loss retires the
  // selection when the watcher delivers it instead of scheduling recovery.
  QTRY_COMPARE(revoked.size(), 1);
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
  QVERIFY(!attachment.recovering());
}

void AttachmentTest::kernelPeerDeathRevokesWithoutWaitingForOwnerWatcher() {
  Bus fixture;
  auto client = fixture.connect("client"), session = fixture.connect("session");
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  QProcess peer;
  peer.start(QCoreApplication::applicationFilePath(),
             {"--peer", fixture.address, runtime.filePath(name)});
  QVERIFY(peer.waitForStarted());
  QVERIFY(peer.waitForReadyRead());
  const auto owner = QString::fromUtf8(peer.readLine()).trimmed();
  QVERIFY(owner.startsWith(':'));
  CompositorAttachment attachment(client, runtime.path(),
                                  [&](const QString &selected) {
                                    return selected == session.baseService();
                                  });
  QVERIFY(attachment.attach(session.baseService(), name,
                            PeerExpectation{owner, quint64(peer.processId())}));
  QVERIFY(attachment.live());
  peer.terminate();
  QVERIFY(peer.waitForFinished(5000));
  QVERIFY(!attachment.live());
  QVERIFY(!attachment.identity());
  QCOMPARE(attachment.openConnection(), -1);
}
void AttachmentTest::kernelPeerDeathIsPublishedWithoutPolling() {
  Bus fixture;
  auto client = fixture.connect("client"), session = fixture.connect("session");
  QTemporaryDir runtime;
  const auto name =
      QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
  QProcess peer;
  peer.start(QCoreApplication::applicationFilePath(),
             {"--peer", fixture.address, runtime.filePath(name)});
  QVERIFY(peer.waitForStarted());
  QVERIFY(peer.waitForReadyRead());
  const auto owner = QString::fromUtf8(peer.readLine()).trimmed();
  CompositorAttachment attachment(client, runtime.path(),
                                  [&](const QString &selected) {
                                    return selected == session.baseService();
                                  });
  QSignalSpy revoked(&attachment, &CompositorAttachment::revoked);
  QVERIFY(attachment.attach(session.baseService(), name,
                            PeerExpectation{owner, quint64(peer.processId())}));
  // Freeze the daemon so its NameOwnerChanged cannot announce the death: only
  // the kernel PIDFD/probe notifiers can publish it, and no getter runs.
  FrozenDaemon frozen(fixture);
  QVERIFY(frozen.frozen);
  peer.kill();
  QVERIFY(peer.waitForFinished(5000));
  QTRY_COMPARE(revoked.size(), 1);
  // The independently expected peer is dead, so the selection retires.
  QVERIFY(!attachment.recovering());
  QVERIFY(!attachment.live());
  frozen.thaw();
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  if (app.arguments().size() == 4 &&
      app.arguments().at(1) == QStringLiteral("--peer")) {
    auto bus = QDBusConnection::connectToBus(app.arguments().at(2), "peer");
    if (!bus.isConnected() ||
        !bus.registerService(QString(QindaQt::CompositorNames::service)))
      return 1;
    Listener socket(app.arguments().at(3));
    if (socket.fd < 0)
      return 2;
    const auto owner = bus.baseService().toUtf8();
    std::puts(owner.constData());
    std::fflush(stdout);
    return app.exec();
  }
  AttachmentTest tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "tst_compositor_attachment.moc"
