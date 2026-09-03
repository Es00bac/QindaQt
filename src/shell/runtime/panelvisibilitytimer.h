// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QtTypes>

#include <functional>

namespace QindaQt::Shell {

// One-shot scheduling seam shared by visibility producers. Callbacks execute
// on the owning GUI thread; cancellation is idempotent and prevents delivery.
class PanelVisibilityTimerPort {
public:
    virtual ~PanelVisibilityTimerPort() = default;
    [[nodiscard]] virtual quint64 schedule(
        int delayMilliseconds, std::function<void()> callback) = 0;
    virtual void cancel(quint64 token) = 0;
};

class QtPanelVisibilityTimer final : public QObject,
                                     public PanelVisibilityTimerPort {
    Q_OBJECT

public:
    explicit QtPanelVisibilityTimer(QObject *parent = nullptr);
    ~QtPanelVisibilityTimer() override;

    [[nodiscard]] quint64 schedule(
        int delayMilliseconds, std::function<void()> callback) override;
    void cancel(quint64 token) override;

private:
    class Private;
    Private *m_private = nullptr;
};

} // namespace QindaQt::Shell
