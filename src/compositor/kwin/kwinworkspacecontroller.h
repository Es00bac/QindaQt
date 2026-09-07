// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPalette>
#include <QString>
#include <functional>
#include <memory>

class QAction;
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

private:
    KWinWorkspaceUiPort &m_port;
    std::function<QString()> m_activeContainer;
    QString m_storageRoot;
    QPalette m_palette;
    std::unique_ptr<QAction> m_action;
    std::unique_ptr<WorkspacesUi::WorkspaceLibraryDialog> m_dialog;
    bool m_shortcutRegistered = false;
};
} // namespace QindaQt::Compositor::KWinIntegration
