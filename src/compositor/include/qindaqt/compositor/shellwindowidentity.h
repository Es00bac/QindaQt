// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellwindowactions.h"

#include <QByteArray>
#include <QString>
#include <QtTypes>

#include <optional>

namespace QindaQt::Compositor {

inline constexpr qsizetype ShellWindowIdentityMaximumServiceBytes = 255;
inline constexpr qsizetype ShellWindowIdentityMaximumObjectPathBytes = 4096;

enum class ShellWindowIdentityStatus {
    Ok,
    Unavailable,
    Unauthorized,
};

struct ShellWindowIdentityFacts final
{
    QString windowId;
    std::optional<qint64> processId;
    std::optional<quint32> appMenuWindowId;
    std::optional<QString> appMenuServiceName;
    std::optional<QString> appMenuObjectPath;

    friend bool operator==(const ShellWindowIdentityFacts &,
                           const ShellWindowIdentityFacts &) = default;
};

// `revision` is the identity snapshot revision. `actionGeneration` is the
// exact independently retained window-action fence sampled with its facts.
struct ShellWindowIdentitySnapshot final
{
    ShellWindowIdentityStatus status = ShellWindowIdentityStatus::Unavailable;
    QString epoch;
    quint64 revision = 0;
    ShellWindowGeneration actionGeneration;
    std::optional<ShellWindowIdentityFacts> activeWindow;
    QString failureCode;
    QString message;

    [[nodiscard]] bool available() const noexcept;
    friend bool operator==(const ShellWindowIdentitySnapshot &,
                           const ShellWindowIdentitySnapshot &) = default;
};

struct ShellWindowIdentityCandidate final
{
    ShellWindowGeneration actionGeneration;
    std::optional<ShellWindowIdentityFacts> activeWindow;
};

enum class ShellWindowIdentityPublishResult {
    Published,
    Unchanged,
    Rejected,
    RevisionExhausted,
};

class ShellWindowIdentityStore final
{
public:
    explicit ShellWindowIdentityStore(QString epoch, quint64 revisionSeed = 0);

    [[nodiscard]] ShellWindowIdentityPublishResult publish(
        const ShellWindowIdentityCandidate &candidate, QString *error = nullptr);
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

class ShellWindowIdentitySource
{
public:
    virtual ~ShellWindowIdentitySource() = default;
    // Borrowed payload remains owned by the source and is copied at the IPC
    // boundary. Implementations refresh synchronously before returning.
    [[nodiscard]] virtual const QByteArray &snapshotJson() = 0;
};

// Authenticates before consulting the identity source. All collaborators are
// borrowed, thread-confined, and must outlive the controller.
class ShellWindowIdentityController final
{
public:
    ShellWindowIdentityController(ShellWindowCredentialSource &credentials,
                                  ShellPanelOwnerSource &panelOwner,
                                  ShellWindowIdentitySource &identitySource);

    [[nodiscard]] bool authorized(const QString &callerUniqueName) const;
    [[nodiscard]] QByteArray snapshot(const QString &callerUniqueName);

private:
    ShellWindowCredentialSource &m_credentials;
    ShellPanelOwnerSource &m_panelOwner;
    ShellWindowIdentitySource &m_identitySource;
};

[[nodiscard]] QByteArray encodeShellWindowIdentitySnapshot(
    const ShellWindowIdentitySnapshot &snapshot);
[[nodiscard]] std::optional<ShellWindowIdentitySnapshot>
decodeShellWindowIdentitySnapshot(const QByteArray &payload,
                                  QString *error = nullptr);

} // namespace QindaQt::Compositor
