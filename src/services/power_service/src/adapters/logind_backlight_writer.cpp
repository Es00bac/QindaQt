// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/logind_backlight_writer.h>

#include <QtCore/QVariant>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusError>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>

#include <unistd.h>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kServiceName[] = "org.freedesktop.login1";
constexpr char kManagerPath[] = "/org/freedesktop/login1";
constexpr char kManagerInterface[] = "org.freedesktop.login1.Manager";
constexpr char kSessionInterface[] = "org.freedesktop.login1.Session";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";
constexpr char kAutoSessionPath[] = "/org/freedesktop/login1/session/auto";
constexpr char kBacklightSubsystem[] = "backlight";

// One bounded round trip. A resident service must never wait on logind
// indefinitely: a stalled write is reported as a failure, not as a hang.
constexpr int kCallTimeoutMs = 2000;
// ListSessions on a shared machine is short; this cap keeps a hostile or
// broken reply from turning resolution into an unbounded property sweep.
constexpr qsizetype kMaxExaminedSessions = 64;
// AGENT-GUARD: capping each call is not enough. One resolution can issue the
// `auto` seat probe, ListSessions, and one Active probe per candidate, so a
// logind that accepts connections and then stalls every reply for the full
// per-call timeout would block the resident service for kMaxExaminedSessions
// timeouts in a row. Resolution therefore also has a wall-clock budget: once
// it is spent, the attempt stops probing and either uses a seated candidate it
// already found or reports unavailable, and the negative-probe backoff takes
// over. The check runs *before* each call, so a call admitted just under the
// budget still runs to its own timeout: one resolution is bounded by the
// budget plus one call, and one request by that plus the write call.
constexpr qint64 kResolutionBudgetMs = 3000;
// A negative probe is retried at most this often, because the sysfs source
// calls available() on every rescan and rescan follows every write.
constexpr qint64 kNegativeProbeBackoffMs = 2000;

constexpr char kUnavailableToken[] = "logind-unavailable";
constexpr char kRefusedToken[] = "logind-refused";

bool errorIsStale(const QString &name)
{
    return name == QStringLiteral("org.freedesktop.DBus.Error.UnknownObject")
        || name == QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface")
        || name == QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown")
        || name == QStringLiteral("org.freedesktop.DBus.Error.NoSuchUnit")
        || name == QStringLiteral("org.freedesktop.DBus.Error.NoReply")
        || name == QStringLiteral("org.freedesktop.DBus.Error.Disconnected");
}

} // namespace

LogindBacklightWriter::LogindBacklightWriter(const QDBusConnection &systemBus)
    : m_bus(systemBus)
    , m_resolutionBudgetMs(kResolutionBudgetMs)
    , m_subjectUid(static_cast<quint32>(::getuid()))
{
    m_diagnostic = QString::fromLatin1(kUnavailableToken);
}

LogindBacklightWriter::~LogindBacklightWriter() = default;

void LogindBacklightWriter::setResolutionBudgetMs(const qint64 budgetMs)
{
    m_resolutionBudgetMs = budgetMs;
    m_resolved = false;
    m_sessionPath.clear();
    m_lastFailedProbe.invalidate();
}

void LogindBacklightWriter::setSubjectUid(const quint32 uid)
{
    m_subjectUid = uid;
    m_resolved = false;
    m_sessionPath.clear();
    m_lastFailedProbe.invalidate();
}

QString LogindBacklightWriter::resolvedSessionPath() const
{
    return m_resolved ? m_sessionPath : QString();
}

QString LogindBacklightWriter::unavailableDiagnostic() const
{
    return m_diagnostic;
}

bool LogindBacklightWriter::available()
{
    if (m_resolved) {
        return true;
    }
    if (m_lastFailedProbe.isValid()
        && m_lastFailedProbe.elapsed() < kNegativeProbeBackoffMs) {
        return false;
    }
    if (resolveSession()) {
        m_lastFailedProbe.invalidate();
        return true;
    }
    m_lastFailedProbe.start();
    return false;
}

bool LogindBacklightWriter::withinResolutionBudget(
    const QElapsedTimer &deadline) const
{
    return deadline.elapsed() < m_resolutionBudgetMs;
}

bool LogindBacklightWriter::resolveSession()
{
    m_resolved = false;
    m_sessionPath.clear();
    m_diagnostic = QString::fromLatin1(kUnavailableToken);
    if (!m_bus.isConnected()) {
        return false;
    }
    QElapsedTimer deadline;
    deadline.start();

    // `auto` is the caller's own session and the answer we want whenever this
    // process really runs inside the graphical session.
    if (sessionReportsSeat(QString::fromLatin1(kAutoSessionPath))) {
        m_sessionPath = QString::fromLatin1(kAutoSessionPath);
        m_resolved = true;
        return true;
    }
    if (!withinResolutionBudget(deadline)) {
        return false;
    }

    // Otherwise this process is outside a seat session (an ssh shell, a
    // system-scope unit). Resolve the subject's seat session explicitly so a
    // brightness request from such a context still reaches the panel the user
    // is looking at.
    const QStringList candidates = seatSessionPathsForSubject();
    QString firstSeated;
    for (const QString &candidate : candidates) {
        if (firstSeated.isEmpty()) {
            firstSeated = candidate;
        }
        if (!withinResolutionBudget(deadline)) {
            // Out of budget with a seated candidate in hand: prefer it over
            // reporting unavailable. An inactive seat session of this uid is
            // still the right panel far more often than no panel at all.
            break;
        }
        if (sessionIsActive(candidate)) {
            m_sessionPath = candidate;
            m_resolved = true;
            return true;
        }
    }
    if (!firstSeated.isEmpty()) {
        m_sessionPath = firstSeated;
        m_resolved = true;
        return true;
    }
    return false;
}

bool LogindBacklightWriter::sessionReportsSeat(const QString &sessionPath) const
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), sessionPath,
        QString::fromLatin1(kPropertiesInterface), QStringLiteral("Get"));
    call.setArguments({QString::fromLatin1(kSessionInterface),
                       QStringLiteral("Seat")});
    const QDBusMessage reply = m_bus.call(call, QDBus::Block, kCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1) {
        return false;
    }
    // Seat is `(so)`: the seat id and its object path. An empty id means the
    // session has no seat, which is exactly the "no seat" SetBrightness
    // failure, detected before we try to write.
    QVariant value = reply.arguments().constFirst();
    if (value.canConvert<QDBusVariant>()) {
        value = value.value<QDBusVariant>().variant();
    }
    if (!value.canConvert<QDBusArgument>()) {
        return false;
    }
    const QDBusArgument structure = value.value<QDBusArgument>();
    if (structure.currentType() != QDBusArgument::StructureType) {
        return false;
    }
    QString seatId;
    QDBusObjectPath seatPath;
    structure.beginStructure();
    structure >> seatId >> seatPath;
    structure.endStructure();
    return !seatId.isEmpty();
}

bool LogindBacklightWriter::sessionIsActive(const QString &sessionPath) const
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), sessionPath,
        QString::fromLatin1(kPropertiesInterface), QStringLiteral("Get"));
    call.setArguments({QString::fromLatin1(kSessionInterface),
                       QStringLiteral("Active")});
    const QDBusMessage reply = m_bus.call(call, QDBus::Block, kCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1) {
        return false;
    }
    QVariant value = reply.arguments().constFirst();
    if (value.canConvert<QDBusVariant>()) {
        value = value.value<QDBusVariant>().variant();
    }
    return value.metaType() == QMetaType::fromType<bool>() && value.toBool();
}

QStringList LogindBacklightWriter::seatSessionPathsForSubject() const
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kManagerPath),
        QString::fromLatin1(kManagerInterface), QStringLiteral("ListSessions"));
    const QDBusMessage reply = m_bus.call(call, QDBus::Block, kCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1) {
        return {};
    }
    const QVariant value = reply.arguments().constFirst();
    if (!value.canConvert<QDBusArgument>()) {
        return {};
    }
    const QDBusArgument array = value.value<QDBusArgument>();
    if (array.currentType() != QDBusArgument::ArrayType) {
        return {};
    }
    // ListSessions is `a(susso)`: session id, uid, user name, seat id, path.
    // The seat id arrives in the same reply, so filtering costs no extra call.
    QStringList seated;
    qsizetype examined = 0;
    array.beginArray();
    while (!array.atEnd() && examined < kMaxExaminedSessions) {
        ++examined;
        QString sessionId;
        quint32 uid = 0;
        QString userName;
        QString seatId;
        QDBusObjectPath sessionPath;
        array.beginStructure();
        array >> sessionId >> uid >> userName >> seatId >> sessionPath;
        array.endStructure();
        if (uid != m_subjectUid || seatId.isEmpty() || sessionPath.path().isEmpty()) {
            continue;
        }
        seated.push_back(sessionPath.path());
    }
    array.endArray();
    return seated;
}

LogindBacklightWriter::CallResult
LogindBacklightWriter::callSetBrightness(const QString &deviceName,
                                         const quint32 value, QString &errorName,
                                         QString &errorText) const
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), m_sessionPath,
        QString::fromLatin1(kSessionInterface), QStringLiteral("SetBrightness"));
    call.setArguments({QString::fromLatin1(kBacklightSubsystem), deviceName,
                       QVariant(value)});
    const QDBusMessage reply = m_bus.call(call, QDBus::Block, kCallTimeoutMs);
    if (reply.type() == QDBusMessage::ReplyMessage) {
        return CallResult::Succeeded;
    }
    const QDBusError error = QDBusError(reply);
    errorName = error.name();
    errorText = error.message();
    return errorIsStale(errorName) ? CallResult::Stale : CallResult::Refused;
}

BacklightWriteOutcome LogindBacklightWriter::write(const QString &deviceName,
                                                   const quint32 value)
{
    if (deviceName.isEmpty()) {
        return {.status = BacklightWriteStatus::Rejected,
                .reasonCode = QStringLiteral("unknown-device"),
                .diagnostic = {}};
    }
    if (!available()) {
        return {.status = BacklightWriteStatus::Failed,
                .reasonCode = QString::fromLatin1(kUnavailableToken),
                .diagnostic = {}};
    }

    QString errorName;
    QString errorText;
    CallResult result = callSetBrightness(deviceName, value, errorName, errorText);
    if (result == CallResult::Stale) {
        // The cached session died (a logout, a seat switch). Re-resolve once
        // and retry, so the very next request after a session change works.
        m_resolved = false;
        m_sessionPath.clear();
        if (available()) {
            result = callSetBrightness(deviceName, value, errorName, errorText);
        } else {
            return {.status = BacklightWriteStatus::Failed,
                    .reasonCode = QString::fromLatin1(kUnavailableToken),
                    .diagnostic = errorText};
        }
    }
    if (result == CallResult::Succeeded) {
        return {.status = BacklightWriteStatus::Succeeded,
                .reasonCode = QStringLiteral("applied"),
                .diagnostic = {}};
    }
    // logind answered and refused. Keep the resolved session: the refusal is
    // about this request, not about the session's existence.
    return {.status = BacklightWriteStatus::Failed,
            .reasonCode = QString::fromLatin1(kRefusedToken),
            .diagnostic = errorText.isEmpty() ? errorName : errorText};
}

} // namespace QindaQt::Power::Upstream
