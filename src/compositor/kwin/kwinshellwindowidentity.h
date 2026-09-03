// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor/shellwindowidentity.h"

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QVector>

namespace KWin {
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class KWinShellVisibilityPublisher;

// GUI-thread projection of the one KWin active window. KWin object pointers
// never cross this boundary; callers receive only validated immutable JSON.
class KWinShellWindowIdentityPublisher final : public QObject,
                                                public ShellWindowIdentitySource
{
    Q_OBJECT

public:
    explicit KWinShellWindowIdentityPublisher(
        KWinShellVisibilityPublisher &visibility, QObject *parent = nullptr);
    ~KWinShellWindowIdentityPublisher() override;

    [[nodiscard]] const QByteArray &snapshotJson() override;

Q_SIGNALS:
    void snapshotChanged();

private:
    void trackWindow(KWin::Window *window);
    void forgetWindow(KWin::Window *window);
    void scheduleRefresh();
    void refresh();
    [[nodiscard]] std::optional<ShellWindowIdentityCandidate> sample(
        QString *error);

    KWinShellVisibilityPublisher &m_visibility;
    ShellWindowIdentityStore m_store;
    QHash<KWin::Window *, QVector<QMetaObject::Connection>> m_connections;
    bool m_refreshScheduled = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
