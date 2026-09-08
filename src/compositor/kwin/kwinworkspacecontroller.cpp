// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinworkspacecontroller.h"
#include "kwinworkspaceuiport.h"

#include <KGlobalAccel>
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QDebug>
#include <QWidget>
#include <QKeyEvent>
#include <input.h>
#include <input_event.h>
#include <QKeySequence>
#include <QMessageBox>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {

class KWinWorkspaceController::KeyboardFilter final : public KWin::InputEventFilter
{
public:
    KeyboardFilter(KWinWorkspaceController &owner, KWin::InputRedirection &input)
        : KWin::InputEventFilter(KWin::InputFilterOrder::InternalWindow)
        , m_owner(owner)
    {
        input.installInputEventFilter(this);
    }

    bool keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        return m_owner.forwardWorkspaceDialogKey(event);
    }

private:
    KWinWorkspaceController &m_owner;
};

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
    if (auto *const app = QApplication::instance()) {
        app->installEventFilter(this);
    }
    if (auto *const input = KWin::input()) {
        m_keyboardFilter = std::make_unique<KeyboardFilter>(*this, *input);
    }
}

KWinWorkspaceController::~KWinWorkspaceController()
{
    if (auto *const app = QApplication::instance()) {
        app->removeEventFilter(this);
    }
    // InputEventFilter unregisters during destruction. Keep the controller's
    // widgets alive until it cannot forward a held-key release to them.
    m_keyboardFilter.reset();
}

bool KWinWorkspaceController::eventFilter(QObject *watched, QEvent *event)
{
    auto *const dialog = qobject_cast<QDialog *>(watched);
    if (!dialog || !event) {
        return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::Show && dialog->isModal()) {
        pruneModalDialogs();
        for (auto iterator = m_modalDialogs.begin(); iterator != m_modalDialogs.end();) {
            if (*iterator == dialog) {
                iterator = m_modalDialogs.erase(iterator);
            } else {
                ++iterator;
            }
        }
        m_modalDialogs.append(dialog);
    } else if (event->type() == QEvent::Hide || event->type() == QEvent::Close) {
        pruneModalDialogs();
    }
    return QObject::eventFilter(watched, event);
}

void KWinWorkspaceController::pruneModalDialogs()
{
    for (auto iterator = m_modalDialogs.begin(); iterator != m_modalDialogs.end();) {
        QDialog *const dialog = iterator->data();
        if (!dialog || !dialog->isVisible() || !dialog->isModal()) {
            iterator = m_modalDialogs.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

QDialog *KWinWorkspaceController::activeModalDialog() const
{
    for (auto iterator = m_modalDialogs.crbegin(); iterator != m_modalDialogs.crend();
         ++iterator) {
        QDialog *const dialog = iterator->data();
        if (dialog && dialog->isVisible() && dialog->isModal()
            && dialog->windowHandle()) {
            return dialog;
        }
    }
    return nullptr;
}

bool KWinWorkspaceController::forwardWorkspaceDialogKey(KWin::KeyboardKeyEvent *event)
{
    if (!event) {
        return false;
    }
    const quint32 scanCode = event->nativeScanCode;
    const bool release = event->state == KWin::KeyboardKeyState::Released;
    QDialog *const dialog = activeModalDialog();
    QWidget *const focused = dialog ? dialog->focusWidget() : nullptr;
    if (!dialog || !focused) {
        if (m_forwardedKeys.contains(scanCode)) {
            if (release) {
                m_forwardedKeys.remove(scanCode);
            }
            return true;
        }
        return false;
    }
    // Popup keeps first claim. This filter handles KWin-owned modal dialogs,
    // which the native InternalWindow filter cannot type into; it never sends
    // a key to a client window.
    QKeyEvent keyEvent(release ? QEvent::KeyRelease : QEvent::KeyPress,
                       event->key, event->modifiers, event->nativeScanCode,
                       event->nativeVirtualKey, 0, event->text,
                       event->state == KWin::KeyboardKeyState::Repeated, 1);
    QCoreApplication::sendEvent(focused, &keyEvent);
    if (release) {
        m_forwardedKeys.remove(scanCode);
    } else {
        m_forwardedKeys.insert(scanCode);
    }
    return true;
}

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
    // The library is parentless, so preserve modal ownership without a nested
    // event loop. Its visible dialog state also makes it eligible for the
    // controller's bounded internal-dialog keyboard route.
    m_dialog->setWindowModality(Qt::ApplicationModal);
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}
} // namespace QindaQt::Compositor::KWinIntegration
