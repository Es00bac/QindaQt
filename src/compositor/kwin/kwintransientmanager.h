// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "hybridtransientpolicy.h"

#include "qindaqt/hybrid/windowtopology.h"

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

namespace KWin {
class Window;
}

namespace QindaQt::Compositor::KWinIntegration {

class ManagedWindowRegistry;

// Associates non-topology dialog/transient windows with grouped owners and
// follows owner geometry, output, workspace, activities, and stacking.
class KWinTransientManager final : public QObject
{
public:
    explicit KWinTransientManager(ManagedWindowRegistry &registry,
                                  QObject *parent = nullptr);
    ~KWinTransientManager() override;

    [[nodiscard]] bool synchronize(const Hybrid::WindowTopology &topology,
                                   QString *error = nullptr);
    [[nodiscard]] std::optional<TransientAssociation> association(
        const QString &transientId) const;

private:
    [[nodiscard]] bool synchronizeCurrent(QString *error = nullptr);
    [[nodiscard]] bool synchronizeWithOwners(QHash<QString, QString> groupOwners,
                                             QString *error);
    [[nodiscard]] KWin::Window *groupedOwner(
        KWin::Window *transient,
        const QHash<QString, QString> &groupOwners) const;
    void reconnect();
    void followContext(const TransientAssociation &association);
    void applyPlacements(const QVector<TransientPlacement> &placements);

    ManagedWindowRegistry &m_registry;
    HybridTransientPolicy m_policy;
    QHash<QString, QString> m_groupOwners;
    QHash<QString, QPointer<KWin::Window>> m_transients;
    QHash<QString, QVector<QMetaObject::Connection>> m_connections;
    bool m_applyingGeometry = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
