// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPalette>
#include <QPointer>
#include <QSet>
#include <QString>
#include <functional>
#include <memory>

class QAction;
class QDialog;
namespace KWin { class InputRedirection; struct KeyboardKeyEvent; }
namespace QindaQt::WorkspacesUi { class WorkspaceLibraryDialog; }
namespace QindaQt::Compositor::KWinIntegration {
class KWinWorkspaceUiPort;

// GUI-thread presentation owner. The borrowed port and selection callback
// dependencies must outlive this controller; destroy it before Hybrid shutdown.
class KWinWorkspaceController final : public QObject {
    Q_OBJECT
public:
    KWinWorkspaceController(KWinWorkspaceUiPort &port,
                            std::function<QString()> activeContainer,
                            QString storageRoot,
                            QObject *parent = nullptr);
    ~KWinWorkspaceController() override;
    void setPalette(const QPalette &palette);
    void showLibrary();
    void showLibraryForContainer(const QString &containerId);
    [[nodiscard]] bool shortcutRegistered() const noexcept;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    class KeyboardFilter;
    [[nodiscard]] QDialog *activeModalDialog() const;
    [[nodiscard]] bool forwardWorkspaceDialogKey(KWin::KeyboardKeyEvent *event);
    void pruneModalDialogs();

    KWinWorkspaceUiPort &m_port;
    std::function<QString()> m_activeContainer;
    QString m_storageRoot;
    QPalette m_palette;
    std::unique_ptr<QAction> m_action;
    std::unique_ptr<WorkspacesUi::WorkspaceLibraryDialog> m_dialog;
    // Destroyed before the dialog so it cannot forward to a torn-down widget.
    std::unique_ptr<KeyboardFilter> m_keyboardFilter;
    // qApp records each visible modal QDialog in show order. A QPointer makes
    // asynchronous deletes safe while held-key releases are still routed.
    QList<QPointer<QDialog>> m_modalDialogs;
    // Releases follow a dialog close (for example Enter accepting Save), so
    // remember the press long enough to prevent it reaching a member client.
    QSet<quint32> m_forwardedKeys;
    bool m_shortcutRegistered = false;
};
} // namespace QindaQt::Compositor::KWinIntegration
