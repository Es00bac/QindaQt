// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

struct TransientSnapshot final
{
    QString transientId;
    QString ownerWindowId;
    QString ownerContainerId;
    QRectF transientFrame;
    QRectF ownerFrame;
};

struct TransientAssociation final
{
    QString transientId;
    QString ownerWindowId;
    QString ownerContainerId;
    QRectF transientFrame;
    QRectF ownerFrame;
    QPointF ownerOffset;

    friend bool operator==(const TransientAssociation &,
                           const TransientAssociation &) = default;
};

struct TransientPlacement final
{
    QString transientId;
    QString ownerWindowId;
    QString ownerContainerId;
    QRectF frame;

    friend bool operator==(const TransientPlacement &,
                           const TransientPlacement &) = default;
};

// Pure owner-relative geometry policy. It neither mutates topology nor retains
// platform windows; the KWin adapter owns stacking/output/workspace effects.
class HybridTransientPolicy final
{
public:
    [[nodiscard]] bool synchronize(QVector<TransientSnapshot> transients,
                                   const QSet<QString> &groupedMemberIds,
                                   QString *error = nullptr);
    [[nodiscard]] QVector<TransientPlacement> ownerFrameChanged(
        const QString &ownerWindowId, const QRectF &frame);
    [[nodiscard]] bool transientFrameChanged(const QString &transientId,
                                             const QRectF &frame,
                                             QString *error = nullptr);
    [[nodiscard]] std::optional<TransientAssociation> association(
        const QString &transientId) const;
    [[nodiscard]] QVector<TransientAssociation> associations() const;

private:
    QVector<TransientAssociation> m_associations;
};

} // namespace QindaQt::Compositor::KWinIntegration
