// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellwindowactions.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace QindaQt::Compositor {

inline constexpr qsizetype ShellTaskFactsMaximumPayloadBytes = 4 * 1024 * 1024;
inline constexpr qsizetype ShellTaskFactsMaximumWindows = 4096;
inline constexpr qsizetype ShellTaskFactsMaximumContainers = 2048;
inline constexpr qsizetype ShellTaskFactsMaximumOutputs = 64;
inline constexpr qsizetype ShellTaskFactsMaximumWorkspaces = 64;
inline constexpr qsizetype ShellTaskFactsMaximumTextCharacters = 512;

enum class ShellTaskWindowRole { Standalone, ContainerPrimary, ContainerMember };
enum class ShellTaskWindowType { Normal, NonNormal };
enum class ShellTaskWindowOwner { Application, BoundShell };
enum class ShellTaskContainerAuthority { ControlBridge, HybridProcess };
enum class ShellTaskFactsStatus { Ok, Unavailable, Unauthorized };

struct ShellTaskOutput final {
    QString id;
    friend bool operator==(const ShellTaskOutput &, const ShellTaskOutput &) = default;
};

struct ShellTaskWorkspace final {
    QString id;
    friend bool operator==(const ShellTaskWorkspace &,
                           const ShellTaskWorkspace &) = default;
};

struct ShellTaskContainer final {
    QString id;
    quint64 revision = 0;
    ShellTaskContainerAuthority authority = ShellTaskContainerAuthority::HybridProcess;
    friend bool operator==(const ShellTaskContainer &,
                           const ShellTaskContainer &) = default;
};

struct ShellTaskWindow final {
    QString windowId;
    QString applicationId;
    QString applicationName;
    QString title;
    ShellTaskWindowRole role = ShellTaskWindowRole::Standalone;
    ShellTaskWindowType type = ShellTaskWindowType::Normal;
    ShellTaskWindowOwner owner = ShellTaskWindowOwner::Application;
    bool active = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
    bool demandsAttention = false;
    QString outputId;
    QStringList workspaceIds;
    bool onAllWorkspaces = false;
    QString containerId;
    friend bool operator==(const ShellTaskWindow &,
                           const ShellTaskWindow &) = default;
};

struct ShellTaskFactsCandidate final {
    ShellWindowGeneration actionGeneration;
    QVector<ShellTaskOutput> outputs;
    QVector<ShellTaskWorkspace> workspaces;
    QVector<ShellTaskContainer> containers;
    QVector<ShellTaskWindow> windows;
    friend bool operator==(const ShellTaskFactsCandidate &,
                           const ShellTaskFactsCandidate &) = default;
};

struct ShellTaskFactsSnapshot final {
    ShellTaskFactsStatus status = ShellTaskFactsStatus::Unavailable;
    QString epoch;
    quint64 revision = 0;
    ShellTaskFactsCandidate facts;
    QString failureCode;
    QString message;

    [[nodiscard]] bool available() const noexcept;
    friend bool operator==(const ShellTaskFactsSnapshot &,
                           const ShellTaskFactsSnapshot &) = default;
};

enum class ShellTaskFactsPublishResult {
    Published,
    Unchanged,
    Rejected,
    RevisionExhausted,
};

// Value-only publication boundary. Callers serialize access on one thread;
// no KWin object, bus connection, timer, or global state enters this type.
class ShellTaskFactsStore final {
public:
    explicit ShellTaskFactsStore(QString epoch, quint64 revisionSeed = 0);

    [[nodiscard]] ShellTaskFactsPublishResult publish(
        const ShellTaskFactsCandidate &candidate, QString *error = nullptr);
    [[nodiscard]] bool markUnavailable(const QString &code,
                                       const QString &message);
    [[nodiscard]] const QByteArray &snapshotJson() const noexcept;
    [[nodiscard]] const QString &epoch() const noexcept;
    [[nodiscard]] quint64 revision() const noexcept;

private:
    QByteArray m_snapshotJson;
    QByteArray m_canonicalState;
    QString m_epoch;
    quint64 m_revision = 0;
    bool m_available = false;
};

class ShellTaskFactsSource {
public:
    virtual ~ShellTaskFactsSource() = default;
    // The borrowed payload remains source-owned and is copied at the D-Bus edge.
    [[nodiscard]] virtual const QByteArray &snapshotJson() = 0;
};

// Authenticates before consulting task facts. Collaborators are borrowed,
// thread-confined, and must outlive this controller.
class ShellTaskFactsController final {
public:
    ShellTaskFactsController(ShellWindowCredentialSource &credentials,
                             ShellPanelOwnerSource &panelOwner,
                             ShellTaskFactsSource &source);

    [[nodiscard]] bool authorized(const QString &callerUniqueName) const;
    [[nodiscard]] QByteArray snapshot(const QString &callerUniqueName);

private:
    ShellWindowCredentialSource &m_credentials;
    ShellPanelOwnerSource &m_panelOwner;
    ShellTaskFactsSource &m_source;
};

[[nodiscard]] bool validateShellTaskFactsCandidate(
    const ShellTaskFactsCandidate &candidate, QString *error = nullptr);
[[nodiscard]] std::optional<ShellTaskFactsSnapshot>
decodeShellTaskFactsSnapshot(const QByteArray &payload,
                             QString *error = nullptr);

} // namespace QindaQt::Compositor
