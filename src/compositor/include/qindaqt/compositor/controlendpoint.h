// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>

namespace QindaQt::Compositor {

class ContainerControlBridge;

class ControlEndpoint final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Compositor1")

public:
    explicit ControlEndpoint(ContainerControlBridge &bridge, QObject *parent = nullptr);

public Q_SLOTS:
    [[nodiscard]] QByteArray Capabilities() const;
    [[nodiscard]] QByteArray Snapshot(const QString &containerId) const;
    [[nodiscard]] QByteArray Submit(const QByteArray &requestJson);

Q_SIGNALS:
    void ContainerCommitted(const QByteArray &eventJson);

private:
    // AGENT-CONTRACT: This codec boundary has no bus registration or policy
    // side effects. The owning KWin integration must gate mutation methods for
    // its deployment context before forwarding them here.
    ContainerControlBridge &m_bridge;
};

} // namespace QindaQt::Compositor
