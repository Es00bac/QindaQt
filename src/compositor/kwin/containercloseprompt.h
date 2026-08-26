// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>

#include <functional>

class QWidget;

namespace QindaQt::Compositor::KWinIntegration {

enum class ContainerCloseDecision {
    CloseAll,
    Ungroup,
    Cancel,
};

// Owns non-blocking close-choice windows, never the resulting compositor
// mutation. The sink is invoked on the GUI thread after the prompt is removed;
// callers must revalidate the container because members can close meanwhile.
class ContainerClosePrompt final : public QObject
{
public:
    using DecisionSink =
        std::function<void(const QString &, ContainerCloseDecision)>;

    explicit ContainerClosePrompt(DecisionSink sink, QObject *parent = nullptr);
    ~ContainerClosePrompt() override;

    ContainerClosePrompt(const ContainerClosePrompt &) = delete;
    ContainerClosePrompt &operator=(const ContainerClosePrompt &) = delete;

    [[nodiscard]] bool request(const QString &containerId,
                               qsizetype memberCount,
                               QWidget *transientParent = nullptr);
    [[nodiscard]] qsizetype activePromptCount() const noexcept;
    [[nodiscard]] QWidget *promptWindow(const QString &containerId) const noexcept;
    void cancelAll() noexcept;

private:
    DecisionSink m_sink;
    QHash<QString, QPointer<QWidget>> m_prompts;
};

} // namespace QindaQt::Compositor::KWinIntegration
