// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QtTypes>

#include <functional>
#include <optional>

namespace QindaQt::Compositor {

enum class ShellWindowAction {
    Activate,
    Minimize,
    Unminimize,
    Close,
    Raise,
};

enum class ShellWindowActionStatus {
    Admitted,
    Stale,
    UnknownWindow,
    Unauthorized,
    ControlDisabled,
};

struct ShellWindowGeneration final
{
    QString epoch;
    quint64 revision = 0;

    [[nodiscard]] bool isValid() const noexcept;
    friend bool operator==(const ShellWindowGeneration &,
                           const ShellWindowGeneration &) = default;
};

struct ShellWindowTarget final
{
    QString windowId;
    QString hybridContainerId;
};

struct ShellWindowActionRequest final
{
    QString callerUniqueName;
    ShellWindowAction action = ShellWindowAction::Activate;
    QString windowId;
    ShellWindowGeneration generation;
};

struct ShellWindowActionResult final
{
    ShellWindowActionStatus status = ShellWindowActionStatus::ControlDisabled;
    ShellWindowAction action = ShellWindowAction::Activate;
    QString windowId;
    ShellWindowGeneration generation;
    QString failureCode;
    QString message;

    [[nodiscard]] bool admitted() const noexcept;
};

class ShellWindowCredentialSource
{
public:
    virtual ~ShellWindowCredentialSource() = default;
    [[nodiscard]] virtual std::optional<qint64> processIdForUniqueName(
        const QString &uniqueName) const = 0;
};

class ShellPanelOwnerSource
{
public:
    virtual ~ShellPanelOwnerSource() = default;
    [[nodiscard]] virtual std::optional<qint64> shellPanelProcessId() const = 0;
};

class ShellWindowRegistry
{
public:
    virtual ~ShellWindowRegistry() = default;
    [[nodiscard]] virtual std::optional<ShellWindowGeneration>
    currentGeneration() const = 0;
    [[nodiscard]] virtual std::optional<ShellWindowTarget> target(
        const QString &windowId) const = 0;
};

class ShellWindowActionExecutor
{
public:
    virtual ~ShellWindowActionExecutor() = default;
    [[nodiscard]] virtual bool execute(ShellWindowAction action,
                                       const ShellWindowTarget &target,
                                       QString *error = nullptr) = 0;
};

struct ShellWindowActionLimits final
{
    qsizetype maximumRequestsPerInterval = 32;
    qint64 intervalMilliseconds = 1000;

    [[nodiscard]] bool isValid() const noexcept;
};

using ShellActionClock = std::function<qint64()>;

// Synchronous policy only. Every dependency is borrowed, must outlive the
// controller, and is called on its owning thread. The controller never queues
// or retries an action.
class ShellWindowActionController final
{
public:
    ShellWindowActionController(ShellWindowCredentialSource &credentials,
                                ShellPanelOwnerSource &panelOwner,
                                ShellWindowRegistry &registry,
                                ShellWindowActionExecutor &executor,
                                ShellWindowActionLimits limits = {},
                                ShellActionClock clock = {});

    [[nodiscard]] ShellWindowActionResult submit(
        const ShellWindowActionRequest &request);

private:
    struct RateWindow final
    {
        qint64 startMilliseconds = 0;
        qsizetype requestCount = 0;
    };

    [[nodiscard]] bool admitRate(const QString &uniqueName, qint64 now);

    ShellWindowCredentialSource &m_credentials;
    ShellPanelOwnerSource &m_panelOwner;
    ShellWindowRegistry &m_registry;
    ShellWindowActionExecutor &m_executor;
    ShellWindowActionLimits m_limits;
    ShellActionClock m_clock;
    QHash<QString, RateWindow> m_rateWindows;
};

[[nodiscard]] QString shellWindowActionName(ShellWindowAction action);
[[nodiscard]] QString shellWindowActionStatusName(ShellWindowActionStatus status);
[[nodiscard]] QByteArray encodeShellWindowActionResult(
    const ShellWindowActionResult &result);
[[nodiscard]] std::optional<ShellWindowActionResult>
decodeShellWindowActionResult(const QByteArray &payload, QString *error = nullptr);

} // namespace QindaQt::Compositor
