// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared fixture for the ShellVisibilitySnapshotStore suites: the strict
// structural rows (tst_shellvisibilitysnapshot.cpp) and the ADR-0384
// per-window degradation rows (tst_shellvisibilitydegradation.cpp).

#include "qindaqt/compositor/shellvisibilitysnapshot.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QindaQt::Compositor::VisibilityFixtures {

inline ShellVisibilitySnapshotCandidate validCandidate()
{
    return {
        .scope = {QStringLiteral("workspace-1"), QStringLiteral("activity-1")},
        .outputs = {{QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5},
                    {QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0}},
        .windows = {{.id = QStringLiteral("window-b"),
                     .outputId = QStringLiteral("DP-2"),
                     .frameGeometry = QRect(2000, 40, 800, 600),
                     .workspaceIds = {},
                     .activityIds = {},
                     .onAllWorkspaces = true,
                     .maximized = true},
                    {.id = QStringLiteral("window-a"),
                     .outputId = QStringLiteral("DP-1"),
                     .frameGeometry = QRect(10, 20, 900, 700),
                     .workspaceIds = {QStringLiteral("workspace-2"),
                                      QStringLiteral("workspace-1")},
                     .activityIds = {QStringLiteral("activity-2"),
                                     QStringLiteral("activity-1")},
                     .active = true}},
    };
}

inline QJsonObject parse(const QByteArray &payload)
{
    const auto document = QJsonDocument::fromJson(payload);
    return document.isObject() ? document.object() : QJsonObject{};
}


inline QJsonObject publishedWindow(const ShellVisibilitySnapshotStore &store, const QString &id)
{
    const auto windows = parse(store.snapshotJson()).value(QStringLiteral("windows")).toArray();
    for (const auto &value : windows) {
        const auto window = value.toObject();
        if (window.value(QStringLiteral("id")).toString() == id) {
            return window;
        }
    }
    return {};
}

} // namespace QindaQt::Compositor::VisibilityFixtures
