// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRect>

#include <optional>

class QDBusInterface;

namespace QindaQt::Test::PanelVisibilityWindowProof {

struct WindowMoveProof final {
    QRect before;
    QRect after;
    QJsonArray authorityBefore;
};

std::optional<QRect> proofWindowGeometry(QDBusInterface &endpoint);
bool waitForMaximizedProofWindowGeometry(QDBusInterface &endpoint,
                                         const QRect &output);
QJsonObject geometryObject(const QRect &geometry);
std::optional<WindowMoveProof> moveProofWindowFromPanel(
    QDBusInterface &endpoint, const QRect &output);
std::optional<WindowMoveProof> restoreProofWindowOntoPanel(
    QDBusInterface &endpoint, const QRect &output);

} // namespace QindaQt::Test::PanelVisibilityWindowProof
