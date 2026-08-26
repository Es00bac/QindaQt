// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

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
// mutation; false leaves presentation and topology unchanged. The controller
// serializes calls and ignores re-entrant state signals raised by an operation.
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
    [[nodiscard]] virtual bool restoreGroup(const MemberGroupBaseline &baseline,
                                            const QString &minimizeWindowId,
                                            const QSet<QString> &missingWindowIds,
                                            MemberRestoreActivation activation,
                                            QString *error = nullptr) = 0;
};

// Toolkit-neutral policy for native member-decoration actions. Synchronization
// copies committed layouts; no topology, KWin, or QObject references escape.
class HybridMemberPolicy final
{
public:
    explicit HybridMemberPolicy(HybridMemberPolicyPlatform &platform);

    [[nodiscard]] bool synchronize(QVector<MemberGroupBaseline> groups,
                                   QString *error = nullptr);
    [[nodiscard]] bool interactiveMoveStarted(const QString &windowId,
                                              bool interactiveMove,
                                              QString *error = nullptr);
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
    // Leaves temporary member maximize/fullscreen presentation before a
    // caller mutates topology or group placement. Call this before the scene
    // transaction: restoring after a page/member move can replay obsolete
    // hidden state over the newly committed layout.
    [[nodiscard]] bool restoreForTopologyMutation(QString *error = nullptr);
    // Add/Forget scene transactions also re-plan every group, but KWin may
    // already have activated a newly mapped window or a close successor. This
    // variant clears focus presentation without stealing that activation.
    [[nodiscard]] bool restoreForLifecycleMutation(QString *error = nullptr);
    // Idempotent explicit lifecycle gate. A focused baseline must be restored
    // before the owning compositor adapter or scene restoration is destroyed.
    [[nodiscard]] bool restoreForShutdown(
        QSet<QString> missingWindowIds = {}, QString *error = nullptr);

    [[nodiscard]] std::optional<MemberFocusState> focusState() const
    {
        return m_focus;
    }
    [[nodiscard]] std::optional<MemberGroupBaseline> focusBaseline() const
    {
        return m_focusBaseline;
    }
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

    [[nodiscard]] MemberLocation locate(const QString &windowId) const;
    [[nodiscard]] bool enter(const MemberLocation &location,
                             MemberFocusMode mode,
                             QString *error);
    [[nodiscard]] bool restore(const QString &minimizeWindowId,
                               QSet<QString> missingWindowIds,
                               MemberRestoreActivation activation,
                               QString *error);

    HybridMemberPolicyPlatform &m_platform;
    QVector<MemberGroupBaseline> m_groups;
    std::optional<MemberGroupBaseline> m_focusBaseline;
    std::optional<MemberFocusState> m_focus;
    QSet<QString> m_detaching;
    bool m_applying = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
