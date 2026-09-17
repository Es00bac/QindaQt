// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

// Atomic platform seam for one independent window rolled up to its icon chip
// (ADR-0191). Implementations must be idempotent and tolerate a dead or
// unmanaged windowId. A call that returns false leaves no partial state.
class HybridIconifyPlatform
{
public:
    virtual ~HybridIconifyPlatform() = default;

    // Removes the window's content, decoration, shadow, and owned transients
    // from paint and from pointer targeting, while keeping the window's own
    // scene item paintable so a chip anchored to it stays visible. Calling it
    // again for an already-hidden window re-applies the treatment (a scene
    // restart recreates the item it was applied to).
    [[nodiscard]] virtual bool hideWindow(const QString &windowId,
                                          QString *error = nullptr) = 0;
    [[nodiscard]] virtual bool showWindow(const QString &windowId,
                                          QString *error = nullptr) = 0;
};

struct IconifiedWindowRecord final
{
    QString windowId;
    // Global logical frame the window returns to on unroll. It follows the
    // chip: dragging the chip translates this frame by the same delta.
    QRectF restoreFrame;
    // The chip's current global logical frame.
    QRectF chipFrame;
    // Whether the window held focus when it was rolled up.
    bool wasActive = false;

    friend bool operator==(const IconifiedWindowRecord &,
                           const IconifiedWindowRecord &) = default;
};

// Pure orchestration for iconified windows: which windows are rolled up,
// where each one restores to, where its chip is, and exactly which platform
// treatment to undo. Session-local, like shade: nothing here persists.
// Retains no KWin reference; the platform is borrowed and must outlive it.
//
// AGENT-CONTRACT: KWin clears Window::isHidden() by itself when it activates
// a window (Workspace::activateWindow calls setHidden(false)). Unlike shade,
// that reveal is a deliberate unroll for an iconified window (dock/task-list
// activation, client activation): the KWin adapter reports it through
// revealed(), which forgets the record and undoes the content treatment so
// the window is a real, focusable client again. It is never re-hidden.
class HybridIconifyController final
{
public:
    explicit HybridIconifyController(HybridIconifyPlatform &platform);

    // Rejects an empty id, an already-iconified window, and non-finite or
    // empty frames. On platform failure no record is kept.
    [[nodiscard]] bool iconify(const QString &windowId,
                               const QRectF &restoreFrame,
                               const QRectF &chipFrame,
                               bool wasActive,
                               QString *error = nullptr);
    // Explicit unroll (wheel, double-click, dock drop). Drops the record and
    // shows the window; the caller applies the returned restore frame and
    // activation. A platform failure still drops the record (a window that
    // closed mid-restore must not stay recorded) and reports the error.
    [[nodiscard]] std::optional<IconifiedWindowRecord> restore(
        const QString &windowId, QString *error = nullptr);
    // KWin revealed the window behind this controller's back (activation).
    // Drops the record and undoes the content treatment; returns the record
    // so the caller can restore the frame. Empty when not iconified.
    [[nodiscard]] std::optional<IconifiedWindowRecord> revealed(
        const QString &windowId);
    // Moves the chip so its top-left is topLeft, clamped so the chip stays
    // inside bounds when bounds is valid; the restore frame moves by the
    // same (clamped) delta so the window reappears under the chip.
    [[nodiscard]] bool relocateChip(const QString &windowId,
                                    const QPointF &topLeft,
                                    const QRectF &bounds,
                                    QString *error = nullptr);
    // Re-applies the platform treatment (after a compositor scene restart).
    [[nodiscard]] bool reapply(const QString &windowId, QString *error = nullptr);
    // The window closed while iconified: drop it, never target it again.
    void windowClosed(const QString &windowId) noexcept;
    // Shutdown: shows every iconified window and returns the records, in
    // iconify order, so the caller can restore frames. Records are dropped
    // even when a show fails.
    [[nodiscard]] QVector<IconifiedWindowRecord> restoreAll(QString *error = nullptr);

    [[nodiscard]] bool isIconified(const QString &windowId) const noexcept;
    [[nodiscard]] std::optional<IconifiedWindowRecord> record(
        const QString &windowId) const;
    // Iconify order, oldest first.
    [[nodiscard]] QStringList iconifiedWindowIds() const;
    [[nodiscard]] qsizetype count() const noexcept { return m_records.size(); }

private:
    [[nodiscard]] qsizetype indexOf(const QString &windowId) const noexcept;

    HybridIconifyPlatform &m_platform;
    QVector<IconifiedWindowRecord> m_records;
};

} // namespace QindaQt::Compositor::KWinIntegration
