// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositorprobeclient.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Test {

// Thin scenario-gated development-input driver for the workspace UI proof.
// It owns no compositor state: events enter only through the public
// Compositor1 InjectTestInput method (development sessions only) and every
// gesture is verified through the read-only inventory afterwards. Key names
// are the DevelopmentInputCodec strings, including the staged
// "left-control"/"w" additions this row requires for Meta+Ctrl+W.
class WorkspaceReopenInput final
{
public:
    explicit WorkspaceReopenInput(CompositorProbeClient &client);

    [[nodiscard]] bool movePointer(const QPointF &point, QString *error);
    [[nodiscard]] bool clickButton(QLatin1StringView button, QString *error);
    [[nodiscard]] bool pressKey(const QString &key, QString *error);
    [[nodiscard]] bool pressChord(const QStringList &keys, QString *error);
    [[nodiscard]] bool drag(const QPointF &start, const QPointF &end,
                            bool metaShift, QString *error);
    [[nodiscard]] const QString &deviceId() const noexcept
    {
        return m_deviceId;
    }
    [[nodiscard]] CompositorProbeClient &client() const noexcept
    {
        return m_client;
    }
    [[nodiscard]] int requestCount() const noexcept { return m_requestCount; }

private:
    [[nodiscard]] bool inject(const QJsonArray &events, QString *error);

    CompositorProbeClient &m_client;
    QString m_deviceId;
    int m_requestCount = 0;
};

// Keyboard/menu helpers. A freshly popped QMenu has no current action, so
// the first Down selects the top item; Qt skips separators and disabled
// entries during traversal.
[[nodiscard]] bool pressSequence(WorkspaceReopenInput &driver,
                                 const QStringList &keys, QString *error);
[[nodiscard]] bool typeFixtureName(WorkspaceReopenInput &driver,
                                   const QString &text, QString *error);
[[nodiscard]] bool openMenuAt(WorkspaceReopenInput &driver,
                              const QPointF &point, QString *error);
[[nodiscard]] bool chooseMenuItem(WorkspaceReopenInput &driver, int downs,
                                  QString *error);
[[nodiscard]] bool chooseSubmenuItem(WorkspaceReopenInput &driver,
                                     int menuDowns, int submenuDowns,
                                     QString *error);

[[nodiscard]] std::optional<ObservedWindow>
awaitWindowTitle(CompositorProbeClient &client, const QString &title,
                 QString *error, int timeoutMilliseconds = 5000);
[[nodiscard]] bool awaitWindowTitleGone(CompositorProbeClient &client,
                                        const QString &title, QString *error,
                                        int timeoutMilliseconds = 5000);
// Unowned, unminimized, unhidden inventory entries in the compositor's own
// (sorted-id) order; matches the ordering availableWindows() presents in the
// production Reopen dialog's per-slot choice lists.
[[nodiscard]] std::optional<QJsonArray>
awaitEligibleWindows(CompositorProbeClient &client,
                     const QStringList &expectedTitles, QString *error,
                     int timeoutMilliseconds = 5000);

} // namespace QindaQt::Test
