// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinworkspacecontroller.h"
#include "kwinworkspaceuiport.h"

#include <KGlobalAccel>
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QKeySequence>
#include <QMessageBox>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
KWinWorkspaceController::KWinWorkspaceController(
    KWinWorkspaceUiPort &port, std::function<QString()> activeContainer,
    QString storageRoot, QObject *parent)
    : QObject(parent), m_port(port), m_activeContainer(std::move(activeContainer)),
      m_storageRoot(std::move(storageRoot)), m_action(std::make_unique<QAction>())
{
    m_action->setObjectName(QStringLiteral("qindaqt_saved_workspaces"));
    m_action->setText(tr("Reopen or save a QindaQt workspace"));
    connect(m_action.get(), &QAction::triggered, this,
            &KWinWorkspaceController::showLibrary);
    const QList<QKeySequence> keys{QKeySequence(Qt::META | Qt::CTRL | Qt::Key_W)};
    auto *const shortcuts = KGlobalAccel::self();
    const bool defaults = shortcuts->setDefaultShortcut(
        m_action.get(), keys, KGlobalAccel::Autoloading);
    const bool active = shortcuts->setShortcut(
        m_action.get(), keys, KGlobalAccel::Autoloading);
    m_shortcutRegistered = defaults && active;
    if (!m_shortcutRegistered) {
        qWarning() << "QindaQt could not register the saved-workspaces shortcut";
    }
    connect(&m_port, &KWinWorkspaceUiPort::restoreWarning, this,
            [this](const QString &warning) {
                // Queue after the assignment dialog accepts its committed restore.
                // A presentation warning must never look like a retryable failure.
                auto *notice = new QMessageBox(QMessageBox::Warning,
                    tr("Workspace restored"), warning, QMessageBox::Ok,
                    m_dialog.get());
                notice->setAttribute(Qt::WA_DeleteOnClose);
                notice->open();
            }, Qt::QueuedConnection);
}

KWinWorkspaceController::~KWinWorkspaceController() = default;

bool KWinWorkspaceController::shortcutRegistered() const noexcept
{
    return m_shortcutRegistered;
}

void KWinWorkspaceController::setPalette(const QPalette &palette)
{
    m_palette = palette;
    if (m_dialog) {
        m_dialog->setPalette(m_palette);
    }
}

void KWinWorkspaceController::showLibrary()
{
    showLibraryForContainer(m_activeContainer ? m_activeContainer() : QString{});
}

void KWinWorkspaceController::showLibraryForContainer(const QString &containerId)
{
    if (m_dialog && m_dialog->isVisible()) {
        // Preserve the captured container and any in-progress assignment when
        // the shortcut is pressed again while the library owns keyboard focus.
        QWidget *target = QApplication::activeModalWidget();
        if (!target) {
            target = m_dialog.get();
        }
        target->raise();
        target->activateWindow();
        return;
    }
    QString ignored;
    // Empty/invalid selection deliberately clears an earlier source. Reopen is
    // still useful with no active group; Save reports the absence when invoked.
    (void)m_port.selectContainer(containerId, &ignored);
    if (!m_dialog) {
        m_dialog = std::make_unique<WorkspacesUi::WorkspaceLibraryDialog>(
            m_storageRoot, m_port);
        m_dialog->setPalette(m_palette);
        connect(&m_port, &KWinWorkspaceUiPort::launchFailed, m_dialog.get(),
                &WorkspacesUi::WorkspaceLibraryDialog::reportLaunchFailure,
                Qt::QueuedConnection);
    } else {
        m_dialog->reload();
    }
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}
} // namespace QindaQt::Compositor::KWinIntegration
