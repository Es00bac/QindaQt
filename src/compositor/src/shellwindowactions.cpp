// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/shellwindowactions.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <chrono>
#include <limits>
#include <utility>

namespace QindaQt::Compositor {
namespace {

constexpr qsizetype MaximumRateOwners = 8;

qint64 monotonicMilliseconds()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool canonicalWindowId(const QString &windowId)
{
    if (windowId.isEmpty()
        || windowId.size() > ShellWindowActionMaximumWindowIdCharacters) {
        return false;
    }
    const QUuid uuid(windowId);
    return !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces) == windowId;
}

ShellWindowActionResult reject(ShellWindowAction action,
                               const QString &windowId,
                               const ShellWindowGeneration &generation,
                               ShellWindowActionStatus status,
                               QString code, QString message)
{
    return {status, action, windowId, generation,
            std::move(code), std::move(message)};
}

ShellWindowActionResult rejectWithoutEcho(ShellWindowAction action,
                                          ShellWindowActionStatus status,
                                          QString code, QString message)
{
    return reject(action, {}, {}, status, std::move(code), std::move(message));
}

bool fieldsWithinEntryBounds(const ShellWindowActionRequest &request)
{
    // AGENT-GUARD: These QString length reads are the only caller-field access
    // permitted before the PID join. Do not scan, parse, normalize, or echo a
    // wire field until authentication succeeds; hostile peers otherwise run
    // unbounded work on KWin's owner thread.
    return request.windowId.size() <= ShellWindowActionMaximumWindowIdCharacters
        && request.epoch.size() <= ShellWindowActionMaximumEpochCharacters
        && request.revision.size() <= ShellWindowActionMaximumRevisionCharacters;
}

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

std::optional<ShellWindowAction> parseAction(const QString &name)
{
    if (name == QLatin1StringView("activate")) return ShellWindowAction::Activate;
    if (name == QLatin1StringView("minimize")) return ShellWindowAction::Minimize;
    if (name == QLatin1StringView("unminimize")) return ShellWindowAction::Unminimize;
    if (name == QLatin1StringView("close")) return ShellWindowAction::Close;
    if (name == QLatin1StringView("raise")) return ShellWindowAction::Raise;
    return std::nullopt;
}

std::optional<ShellWindowActionStatus> parseStatus(const QString &name)
{
    if (name == QLatin1StringView("admitted")) return ShellWindowActionStatus::Admitted;
    if (name == QLatin1StringView("stale")) return ShellWindowActionStatus::Stale;
    if (name == QLatin1StringView("unknown-window")) {
        return ShellWindowActionStatus::UnknownWindow;
    }
    if (name == QLatin1StringView("unauthorized")) {
        return ShellWindowActionStatus::Unauthorized;
    }
    if (name == QLatin1StringView("control-disabled")) {
        return ShellWindowActionStatus::ControlDisabled;
    }
    return std::nullopt;
}

std::optional<quint64> parseRevision(const QString &value)
{
    if (value.isEmpty() || (value.size() > 1 && value.startsWith(u'0'))) {
        return std::nullopt;
    }
    for (const QChar character : value) {
        if (character < u'0' || character > u'9') {
            return std::nullopt;
        }
    }
    bool ok = false;
    const quint64 revision = value.toULongLong(&ok, 10);
    return ok && revision > 0 ? std::optional<quint64>(revision) : std::nullopt;
}

} // namespace

bool ShellWindowGeneration::isValid() const noexcept
{
    return revision > 0 && !epoch.isEmpty()
        && epoch.size() <= ShellWindowActionMaximumEpochCharacters
        && epoch == epoch.trimmed();
}

bool ShellWindowActionResult::admitted() const noexcept
{
    return status == ShellWindowActionStatus::Admitted;
}

bool ShellWindowActionLimits::isValid() const noexcept
{
    return maximumRequestsPerInterval > 0
        && maximumRequestsPerInterval <= 4096
        && intervalMilliseconds > 0 && intervalMilliseconds <= 60'000;
}

ShellWindowActionController::ShellWindowActionController(
    ShellWindowCredentialSource &credentials,
    ShellPanelOwnerSource &panelOwner,
    ShellWindowRegistry &registry,
    ShellWindowActionExecutor &executor,
    ShellWindowActionLimits limits,
    ShellActionClock clock)
    : m_credentials(credentials)
    , m_panelOwner(panelOwner)
    , m_registry(registry)
    , m_executor(executor)
    , m_limits(limits.isValid() ? limits : ShellWindowActionLimits{})
    , m_clock(clock ? std::move(clock) : ShellActionClock(monotonicMilliseconds))
{
}

ShellWindowActionResult ShellWindowActionController::submit(
    const ShellWindowActionRequest &request)
{
    const bool fieldsAreBounded = fieldsWithinEntryBounds(request);
    const auto panelPid = m_panelOwner.shellPanelProcessId();
    if (!panelPid || *panelPid <= 1) {
        return rejectWithoutEcho(
            request.action, ShellWindowActionStatus::ControlDisabled,
            QStringLiteral("shell-owner-unbound"),
            QStringLiteral("no single committed shell panel owner is bound"));
    }
    if (request.callerUniqueName.isEmpty()
        || !request.callerUniqueName.startsWith(u':')) {
        return rejectWithoutEcho(
            request.action, ShellWindowActionStatus::Unauthorized,
            QStringLiteral("caller-not-unique"),
            QStringLiteral("the caller has no unique D-Bus identity"));
    }
    const auto callerPid =
        m_credentials.processIdForUniqueName(request.callerUniqueName);
    if (!callerPid || *callerPid != *panelPid) {
        return rejectWithoutEcho(
            request.action, ShellWindowActionStatus::Unauthorized,
            QStringLiteral("caller-pid-mismatch"),
            QStringLiteral("the D-Bus caller does not own the shell panels"));
    }
    if (!admitRate(request.callerUniqueName, m_clock())) {
        return rejectWithoutEcho(
            request.action, ShellWindowActionStatus::ControlDisabled,
            QStringLiteral("rate-limited"),
            QStringLiteral("the bounded shell action rate was exceeded"));
    }
    if (!fieldsAreBounded) {
        return rejectWithoutEcho(
            request.action, ShellWindowActionStatus::ControlDisabled,
            QStringLiteral("request-fields-too-large"),
            QStringLiteral("the shell action fields exceed their entry bounds"));
    }
    const auto revision = parseRevision(request.revision);
    const ShellWindowGeneration generation{request.epoch,
                                            revision.value_or(0)};
    const auto currentGeneration = m_registry.currentGeneration();
    if (!generation.isValid() || !currentGeneration
        || generation != *currentGeneration) {
        return reject(request.action, request.windowId, generation,
                      ShellWindowActionStatus::Stale,
                      QStringLiteral("stale-generation"),
                      QStringLiteral("the observed compositor generation is no longer current"));
    }
    if (!canonicalWindowId(request.windowId)) {
        return reject(request.action, request.windowId, generation,
                      ShellWindowActionStatus::UnknownWindow,
                      QStringLiteral("unknown-window"),
                      QStringLiteral("the window UUID is not currently managed"));
    }
    const auto target = m_registry.target(request.windowId);
    if (!target || target->windowId != request.windowId) {
        return reject(request.action, request.windowId, generation,
                      ShellWindowActionStatus::UnknownWindow,
                      QStringLiteral("unknown-window"),
                      QStringLiteral("the window UUID is not currently managed"));
    }

    QString error;
    if (!m_executor.execute(request.action, *target, &error)) {
        return reject(request.action, request.windowId, generation,
                      ShellWindowActionStatus::ControlDisabled,
                      QStringLiteral("action-rejected"),
                      error.isEmpty() ? QStringLiteral("window policy rejected the action")
                                      : error);
    }
    return {ShellWindowActionStatus::Admitted, request.action, request.windowId,
            generation, {}, {}};
}

bool ShellWindowActionController::admitRate(const QString &uniqueName, qint64 now)
{
    if (m_rateWindows.size() >= MaximumRateOwners
        && !m_rateWindows.contains(uniqueName)) {
        m_rateWindows.clear();
    }
    auto &window = m_rateWindows[uniqueName];
    if (now < window.startMilliseconds
        || now - window.startMilliseconds >= m_limits.intervalMilliseconds) {
        window = {now, 0};
    }
    if (window.requestCount >= m_limits.maximumRequestsPerInterval) {
        return false;
    }
    ++window.requestCount;
    return true;
}

QString shellWindowActionName(ShellWindowAction action)
{
    switch (action) {
    case ShellWindowAction::Activate: return QStringLiteral("activate");
    case ShellWindowAction::Minimize: return QStringLiteral("minimize");
    case ShellWindowAction::Unminimize: return QStringLiteral("unminimize");
    case ShellWindowAction::Close: return QStringLiteral("close");
    case ShellWindowAction::Raise: return QStringLiteral("raise");
    }
    return {};
}

QString shellWindowActionStatusName(ShellWindowActionStatus status)
{
    switch (status) {
    case ShellWindowActionStatus::Admitted: return QStringLiteral("admitted");
    case ShellWindowActionStatus::Stale: return QStringLiteral("stale");
    case ShellWindowActionStatus::UnknownWindow: return QStringLiteral("unknown-window");
    case ShellWindowActionStatus::Unauthorized: return QStringLiteral("unauthorized");
    case ShellWindowActionStatus::ControlDisabled: return QStringLiteral("control-disabled");
    }
    return {};
}

QByteArray encodeShellWindowActionResult(const ShellWindowActionResult &result)
{
    QJsonObject object{{QStringLiteral("status"),
                        shellWindowActionStatusName(result.status)},
                       {QStringLiteral("action"), shellWindowActionName(result.action)}};
    if (!result.windowId.isEmpty() || result.generation.revision != 0
        || !result.generation.epoch.isEmpty()) {
        object.insert(QStringLiteral("windowId"), result.windowId);
        object.insert(QStringLiteral("epoch"), result.generation.epoch);
        object.insert(QStringLiteral("revision"),
                      QString::number(result.generation.revision));
    }
    if (!result.failureCode.isEmpty()) {
        object.insert(QStringLiteral("failure"),
                      QJsonObject{{QStringLiteral("code"), result.failureCode},
                                  {QStringLiteral("message"), result.message}});
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<ShellWindowActionResult> decodeShellWindowActionResult(
    const QByteArray &payload, QString *error)
{
    if (payload.isEmpty() || payload.size() > 64 * 1024) {
        setError(error, QStringLiteral("shell action reply size is invalid"));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("shell action reply is not a JSON object"));
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    const auto status = parseStatus(object.value(QStringLiteral("status")).toString());
    const auto action = parseAction(object.value(QStringLiteral("action")).toString());
    const QString windowId = object.value(QStringLiteral("windowId")).toString();
    const QString epoch = object.value(QStringLiteral("epoch")).toString();
    const auto revision = parseRevision(
        object.value(QStringLiteral("revision")).toString());
    if (!status || !action || !revision || !canonicalWindowId(windowId)) {
        setError(error, QStringLiteral("shell action reply has invalid required fields"));
        return std::nullopt;
    }
    const QJsonObject failure = object.value(QStringLiteral("failure")).toObject();
    ShellWindowActionResult result{*status, *action, windowId, {epoch, *revision},
                                   failure.value(QStringLiteral("code")).toString(),
                                   failure.value(QStringLiteral("message")).toString()};
    if (!result.generation.isValid()
        || (result.admitted() && !result.failureCode.isEmpty())
        || (!result.admitted() && result.failureCode.isEmpty())) {
        setError(error, QStringLiteral("shell action reply status is inconsistent"));
        return std::nullopt;
    }
    if (error) {
        error->clear();
    }
    return result;
}

} // namespace QindaQt::Compositor
