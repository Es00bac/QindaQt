// SPDX-License-Identifier: LGPL-3.0-or-later
#include "polkit_authority_probe.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>

#include <unistd.h>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusPendingReply>

namespace QindaQt::Apps::SettingsLoginScreen {

// AGENT-CONTRACT: the wire types of org.freedesktop.PolicyKit1.Authority
// .CheckAuthorization, whose full IN signature is
//   (sa{sv}) subject, s action_id, a{ss} details, u flags, s cancellation_id
// (see polkit's org.freedesktop.PolicyKit1.Authority.xml). The subject
// struct comes first; omitting it makes the daemon reject the call with a
// signature mismatch, which used to surface here as "the system does not
// know the login screen action" on a machine where everything was in fact
// installed. Verified live against polkitd on the reference Gentoo host.
// These two are deliberately NOT in an anonymous namespace: the QDBus
// metatype machinery finds their marshallers by ADL on the named namespace.
struct PolkitSubject {
  QString kind;
  QVariantMap details; // a{sv}
};

// CheckAuthorization's details argument is a{ss}; QtDBus has no registered
// map-of-string-to-string, so it gets an explicit metatype and marshaller.
using PolkitDetails = QMap<QString, QString>;

} // namespace QindaQt::Apps::SettingsLoginScreen

Q_DECLARE_METATYPE(QindaQt::Apps::SettingsLoginScreen::PolkitSubject)
Q_DECLARE_METATYPE(QindaQt::Apps::SettingsLoginScreen::PolkitDetails)

namespace QindaQt::Apps::SettingsLoginScreen {

QDBusArgument &operator<<(QDBusArgument &argument, const PolkitSubject &subject) {
  argument.beginStructure();
  argument << subject.kind << subject.details;
  argument.endStructure();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                PolkitSubject &subject) {
  argument.beginStructure();
  argument >> subject.kind >> subject.details;
  argument.endStructure();
  return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PolkitDetails &details) {
  argument.beginMap(QMetaType::fromType<QString>(),
                    QMetaType::fromType<QString>());
  for (auto it = details.constBegin(); it != details.constEnd(); ++it) {
    argument.beginMapEntry();
    argument << it.key() << it.value();
    argument.endMapEntry();
  }
  argument.endMap();
  return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                PolkitDetails &details) {
  argument.beginMap();
  while (!argument.atEnd()) {
    argument.beginMapEntry();
    QString key;
    QString value;
    argument >> key >> value;
    details.insert(key, value);
    argument.endMapEntry();
  }
  argument.endMap();
  return argument;
}

namespace {

void registerPolkitWireTypes() {
  static const bool registered = [] {
    qDBusRegisterMetaType<PolkitSubject>();
    qDBusRegisterMetaType<PolkitDetails>();
    return true;
  }();
  Q_UNUSED(registered);
}

// This process's start time exactly as polkit wants it in a unix-process
// subject: the raw field 22 of /proc/self/stat, clock ticks since boot,
// with NO conversion to seconds and no boot-time offset. Converting (as an
// earlier draft did) makes the value disagree with the daemon's own lookup
// and the answer comes back "process ... has been replaced". Returns 0 on
// any failure; callers treat 0 as "cannot build a subject polkit accepts".
[[nodiscard]] quint64 processStartTimeTicks() {
  QFile statFile(QStringLiteral("/proc/self/stat"));
  if (!statFile.open(QIODevice::ReadOnly)) {
    return 0;
  }
  const QByteArray stat = statFile.readAll();
  // comm (field 2) may contain spaces and parentheses; everything after the
  // LAST ')' starts at field 3 (state), so starttime (field 22) is index 19.
  const qsizetype closeParen = stat.lastIndexOf(')');
  if (closeParen < 0 || closeParen + 2 >= stat.size()) {
    return 0;
  }
  const QList<QByteArray> fields = stat.mid(closeParen + 2).split(' ');
  if (fields.size() <= 19) {
    return 0;
  }
  bool ok = false;
  const quint64 startTicks = fields.at(19).toULongLong(&ok);
  if (!ok) {
    return 0;
  }
  return startTicks;
}

} // namespace

PolkitAuthorityProbe::PolkitAuthorityProbe(QDBusConnection systemBus,
                                           QString actionId,
                                           QString pkexecPath,
                                           QString helperPath,
                                           QObject *parent)
    : LoginScreenAuthorityProbe(parent),
      m_bus(std::move(systemBus)),
      m_actionId(std::move(actionId)),
      m_pkexecPath(std::move(pkexecPath)),
      m_helperPath(std::move(helperPath)) {}

void PolkitAuthorityProbe::probe() {
  if (m_pkexecPath.isEmpty()) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "pkexec is not available on this system, so the login screen can "
        "only be viewed here, not changed."));
    return;
  }
  if (m_helperPath.isEmpty() || !QFile::exists(m_helperPath)) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The privileged helper for login screen settings is not installed, "
        "so this page can only show the current configuration."));
    return;
  }
  if (!m_bus.isConnected()) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The system authorization service cannot be reached, so the login "
        "screen can only be viewed here, not changed."));
    return;
  }

  registerPolkitWireTypes();
  QDBusInterface authority(QStringLiteral("org.freedesktop.PolicyKit1"),
                           QStringLiteral("/org/freedesktop/PolicyKit1/Authority"),
                           QStringLiteral("org.freedesktop.PolicyKit1.Authority"),
                           m_bus, this);
  // The subject is this process itself -- the question is "may the Settings
  // app use this action". Current polkit REQUIRES start-time (raw clock
  // ticks since boot, see processStartTimeTicks) and uid alongside pid in a
  // unix-process subject (they are how the daemon defends against PID reuse
  // and subject spoofing); omitting either is a hard "Didn't find value for
  // key ..." / "does not have uid set" error, verified live against polkitd
  // on the reference Gentoo host.
  const quint64 startTime = processStartTimeTicks();
  if (startTime == 0) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "This process's start time could not be read, so the system "
        "authorization service cannot be asked about the login screen. "
        "The page can only show the current configuration."));
    return;
  }
  // Flags 0: no user interaction from the probe itself. The answer's
  // challenge bit tells us a prompt WOULD succeed, which keeps the
  // controls writable -- the prompt then happens at save time, exactly
  // once, where the user expects it.
  const PolkitSubject subject{
      QStringLiteral("unix-process"),
      {{QStringLiteral("pid"),
        QVariant::fromValue<quint32>(
            static_cast<quint32>(QCoreApplication::applicationPid()))},
       {QStringLiteral("start-time"),
        QVariant::fromValue<quint64>(startTime)},
       {QStringLiteral("uid"), QVariant::fromValue<qint32>(
                                   static_cast<qint32>(getuid()))}}};
  auto *watcher = new QDBusPendingCallWatcher(
      authority.asyncCall(QStringLiteral("CheckAuthorization"),
                          QVariant::fromValue(subject), m_actionId,
                          QVariant::fromValue(PolkitDetails{}), uint(0),
                          QString()),
      this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          &PolkitAuthorityProbe::onReply);
}

void PolkitAuthorityProbe::onReply(QDBusPendingCallWatcher *watcher) {
  const QDBusMessage reply = watcher->reply();
  watcher->deleteLater();
  if (reply.type() == QDBusMessage::ErrorMessage) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The system does not know the login screen action (%1). The "
        "QindaQt polkit policy may not be installed, so this page can only "
        "show the current configuration.")
        .arg(reply.errorMessage()));
    return;
  }
  const QList<QVariant> arguments = reply.arguments();
  if (arguments.isEmpty()) {
    Q_EMIT probeFinished(false, QStringLiteral(
        "The authorization service answered unexpectedly; treating the "
        "login screen as read-only."));
    return;
  }
  // OUT signature is (bb a{sv}): is_authorized, is_challenge, details. Only
  // the two booleans matter here; the trailing map is left unread.
  const QDBusArgument result = arguments.first().value<QDBusArgument>();
  bool authorized = false;
  bool challenge = false;
  result.beginStructure();
  result >> authorized >> challenge;
  result.endStructure();
  if (authorized || challenge) {
    Q_EMIT probeFinished(true, QString());
    return;
  }
  Q_EMIT probeFinished(false, QStringLiteral(
      "You are not allowed to administer the login screen on this machine, "
      "so this page can only show the current configuration."));
}

} // namespace QindaQt::Apps::SettingsLoginScreen
