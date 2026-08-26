// SPDX-License-Identifier: GPL-3.0-or-later
#include "containercloseprompt.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>
#include <QVariant>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto DecisionProperty = "qindaqtContainerCloseDecision";

void markDecision(QAbstractButton *button, ContainerCloseDecision decision)
{
    button->setProperty(DecisionProperty, static_cast<int>(decision));
}

ContainerCloseDecision clickedDecision(const QMessageBox &prompt)
{
    const auto *button = prompt.clickedButton();
    if (!button) {
        return ContainerCloseDecision::Cancel;
    }
    bool converted = false;
    const int value = button->property(DecisionProperty).toInt(&converted);
    if (!converted || value < static_cast<int>(ContainerCloseDecision::CloseAll)
        || value > static_cast<int>(ContainerCloseDecision::Cancel)) {
        return ContainerCloseDecision::Cancel;
    }
    return static_cast<ContainerCloseDecision>(value);
}

} // namespace

ContainerClosePrompt::ContainerClosePrompt(DecisionSink sink, QObject *parent)
    : QObject(parent)
    , m_sink(std::move(sink))
{
}

ContainerClosePrompt::~ContainerClosePrompt()
{
    cancelAll();
}

bool ContainerClosePrompt::request(const QString &containerId,
                                   qsizetype memberCount,
                                   QWidget *transientParent)
{
    if (containerId.isEmpty() || memberCount < 2) {
        return false;
    }
    if (auto *existing = m_prompts.value(containerId).data()) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return true;
    }

    auto *const prompt = new QMessageBox(
        QMessageBox::Question,
        tr("Close window group?"),
        tr("This group contains %n windows.", nullptr, static_cast<int>(memberCount)),
        QMessageBox::NoButton,
        transientParent);
    prompt->setObjectName(QStringLiteral("qindaqt-container-close-prompt"));
    prompt->setInformativeText(
        tr("Close every window, keep the windows open and ungroup them, or cancel."));
    prompt->setWindowModality(Qt::NonModal);

    auto *const closeAll = prompt->addButton(tr("Close All"), QMessageBox::DestructiveRole);
    auto *const ungroup = prompt->addButton(tr("Ungroup"), QMessageBox::ActionRole);
    auto *const cancel = prompt->addButton(QMessageBox::Cancel);
    markDecision(closeAll, ContainerCloseDecision::CloseAll);
    markDecision(ungroup, ContainerCloseDecision::Ungroup);
    markDecision(cancel, ContainerCloseDecision::Cancel);
    prompt->setDefaultButton(qobject_cast<QPushButton *>(cancel));
    prompt->setEscapeButton(cancel);

    m_prompts.insert(containerId, prompt);
    connect(prompt, &QDialog::finished, this,
            [this, containerId, guarded = QPointer<QMessageBox>(prompt)] {
                if (!guarded
                    || m_prompts.value(containerId).data() != guarded.data()) {
                    return;
                }
                const auto decision = clickedDecision(*guarded);
                m_prompts.remove(containerId);
                if (m_sink) {
                    m_sink(containerId, decision);
                }
                guarded->deleteLater();
            });
    prompt->open();
    return true;
}

qsizetype ContainerClosePrompt::activePromptCount() const noexcept
{
    return m_prompts.size();
}

QWidget *ContainerClosePrompt::promptWindow(const QString &containerId) const noexcept
{
    return m_prompts.value(containerId).data();
}

void ContainerClosePrompt::cancelAll() noexcept
{
    const auto prompts = m_prompts.values();
    m_prompts.clear();
    for (auto &guarded : prompts) {
        if (auto *prompt = guarded.data()) {
            disconnect(prompt, nullptr, this, nullptr);
            prompt->close();
            prompt->deleteLater();
        }
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
