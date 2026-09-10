// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMap>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

enum class MemberFocusMode {
    Maximized,
    Fullscreen,
};

enum class MemberRestoreActivation {
    RestoreBaseline,
    PreserveCurrent,
};

struct MemberLayoutBaseline final
{
    QString windowId;
    QRectF frame;
    bool minimized = false;
    bool hidden = false;
    bool active = false;
    bool activePage = false;

    friend bool operator==(const MemberLayoutBaseline &,
                           const MemberLayoutBaseline &) = default;
};

struct MemberGroupBaseline final
{
    QString containerId;
    QRectF outerFrame;
    QVector<MemberLayoutBaseline> members;

    [[nodiscard]] const MemberLayoutBaseline *member(const QString &windowId) const;
    [[nodiscard]] bool isValid(QString *error = nullptr) const;

    friend bool operator==(const MemberGroupBaseline &,
                           const MemberGroupBaseline &) = default;
};

struct MemberFocusState final
{
    QString containerId;
    QString windowId;
    MemberFocusMode mode = MemberFocusMode::Maximized;

    friend bool operator==(const MemberFocusState &,
                           const MemberFocusState &) = default;
};

// Atomic platform seam. Implementations preflight all named live windows before
// mutation; an ordinary false leaves presentation and topology unchanged, while
// restoreRejectedPresentation may unwind a native request KWin already applied
// without changing the accepted focus presentation. The controller serializes
// calls and ignores re-entrant state signals raised by an operation.
class HybridMemberPolicyPlatform
{
public:
    virtual ~HybridMemberPolicyPlatform() = default;

    [[nodiscard]] virtual bool detachMember(const QString &containerId,
                                            const QString &windowId,
                                            const MemberGroupBaseline *focusBaseline,
                                            QString *error = nullptr) = 0;
    [[nodiscard]] virtual bool enterFocus(const MemberGroupBaseline &baseline,
                                          const QString &windowId,
                                          MemberFocusMode mode,
                                          QString *error = nullptr) = 0;
    // KWin emits maximize/fullscreen notifications after applying a native
    // request. When another member of the same container already owns
    // temporary focus presentation, the adapter must unwind only that
    // rejected member's native state against this committed baseline and
    // return focus to `focusOwnerWindowId`.
    // AGENT-CONTRACT: This is a single rejected-request correction, not an
    // active-window observer: ordinary Alt-Tab and outside-window focus remain
    // compositor-owned after the correction completes.
    [[nodiscard]] virtual bool restoreRejectedPresentation(
        const MemberGroupBaseline &baseline,
        const QString &windowId,
        const QString &focusOwnerWindowId,
        MemberFocusMode mode,
        QString *error = nullptr) = 0;
    [[nodiscard]] virtual bool restoreGroup(const MemberGroupBaseline &baseline,
                                            const QString &minimizeWindowId,
                                            const QSet<QString> &missingWindowIds,
                                            MemberRestoreActivation activation,
                                            QString *error = nullptr) = 0;
};

// Toolkit-neutral policy for native member-decoration actions. Synchronization
// copies committed layouts; no topology, KWin, or QObject references escape.
//
// AGENT-CONTRACT: Focus presentation is owned per container. Each container
// may present at most one member alone, and a request inside one container
// never rejects, restores, hides, or activates anything in another. Only the
// whole-session gates (topology/lifecycle/shutdown) touch every container,
// because a coordinator scene transaction re-plans every group. A single
// session-wide owner made shared-chrome maximize/minimize/restore on one
// container silently restore and re-activate another container's member.
class HybridMemberPolicy final
{
public:
    explicit HybridMemberPolicy(HybridMemberPolicyPlatform &platform);

    [[nodiscard]] bool synchronize(QVector<MemberGroupBaseline> groups,
                                   QString *error = nullptr);
    [[nodiscard]] bool interactiveMoveStarted(const QString &windowId,
                                              bool interactiveMove,
                                              QString *error = nullptr);
    // True while windowId is an owned member that is not mid-detach. The
    // adapter vetoes native interactive resize when this holds; a mid-detach
    // member is already owned by the detach transaction and is excluded.
    [[nodiscard]] bool blocksInteractiveResize(const QString &windowId) const;
    [[nodiscard]] bool maximizedChanged(const QString &windowId,
                                        bool maximized,
                                        QString *error = nullptr);
    [[nodiscard]] bool fullscreenChanged(const QString &windowId,
                                         bool fullscreen,
                                         QString *error = nullptr);
    [[nodiscard]] bool minimizedChanged(const QString &windowId,
                                        bool minimized,
                                        QString *error = nullptr);
    [[nodiscard]] bool memberClosed(const QString &windowId,
                                    QString *error = nullptr);
    // Leaves temporary member maximize/fullscreen presentation in every
    // container before a caller mutates topology through a scene transaction.
    // Transactions re-plan every group, so this gate cannot be narrowed to one
    // container. Call it before the transaction: restoring after a page/member
    // move can replay obsolete hidden state over the newly committed layout.
    [[nodiscard]] bool restoreForTopologyMutation(QString *error = nullptr);
    // Leaves focus presentation for exactly one container before a
    // placement-only action on it (group maximize/restore/minimize/shade/
    // raise) that reflows or hides only that container and runs no scene
    // transaction. Every other container keeps its presentation. Idempotent.
    [[nodiscard]] bool restoreForContainerAction(const QString &containerId,
                                                 QString *error = nullptr);
    // Add/Forget scene transactions also re-plan every group, but KWin may
    // already have activated a newly mapped window or a close successor. This
    // variant clears focus presentation without stealing that activation.
    [[nodiscard]] bool restoreForLifecycleMutation(QString *error = nullptr);
    // Idempotent explicit lifecycle gate. Every focused baseline must be
    // restored before the owning compositor adapter or scene restoration is
    // destroyed.
    [[nodiscard]] bool restoreForShutdown(
        QSet<QString> missingWindowIds = {}, QString *error = nullptr);

    [[nodiscard]] std::optional<MemberFocusState> focusState(
        const QString &containerId) const;
    // Every container currently presenting one member alone, in stable
    // container-ID order.
    [[nodiscard]] QVector<MemberFocusState> focusStates() const;
    [[nodiscard]] std::optional<MemberGroupBaseline> focusBaseline(
        const QString &containerId) const;
    [[nodiscard]] bool ownsTransition() const noexcept { return m_applying; }

private:
    struct MemberLocation final
    {
        qsizetype groupIndex = -1;
        qsizetype memberIndex = -1;

        [[nodiscard]] bool isValid() const noexcept
        {
            return groupIndex >= 0 && memberIndex >= 0;
        }
    };

    // The pre-action committed copy travels with the owner it restores.
    struct FocusEntry final
    {
        MemberFocusState state;
        MemberGroupBaseline baseline;
    };

    [[nodiscard]] MemberLocation locate(const QString &windowId) const;
    // Container whose focus presentation is owned by windowId, or empty.
    [[nodiscard]] QString focusedContainerOf(const QString &windowId) const;
    [[nodiscard]] bool enter(const MemberLocation &location,
                             MemberFocusMode mode,
                             QString *error);
    [[nodiscard]] bool restoreRejectedPresentation(const QString &containerId,
                                                   const QString &windowId,
                                                   MemberFocusMode mode,
                                                   QString *error);
    [[nodiscard]] bool restore(const QString &containerId,
                               const QString &minimizeWindowId,
                               const QSet<QString> &missingWindowIds,
                               MemberRestoreActivation activation,
                               QString *error);
    [[nodiscard]] bool restoreAll(const QSet<QString> &missingWindowIds,
                                  MemberRestoreActivation activation,
                                  QString *error);

    HybridMemberPolicyPlatform &m_platform;
    QVector<MemberGroupBaseline> m_groups;
    QMap<QString, FocusEntry> m_focus;
    QSet<QString> m_detaching;
    bool m_applying = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
