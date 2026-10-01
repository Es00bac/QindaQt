// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QList>
#include <functional>
#include <memory>
namespace QindaQt::CompositorCapture {
struct MonitorSource { QString id, name; QPoint position; QSize size; };
// Owns one supplied ordinary FD, one monitor stream and generated protocol
// resources. No pathname/display fallback, permission, persistence or UI policy.
// Same-thread borrowed admission is readonly/non-reentrant and outlives the port.
// Sources and node publication recheck admission; loss invalidates getters before
// notifying and tears down safely after protocol dispatch. Never restart/replay.
class WaylandScreenCast final : public QObject {
    Q_OBJECT
public:
    WaylandScreenCast(int ownedOrdinaryFd, std::function<bool()> admission, QObject *parent = nullptr);
    ~WaylandScreenCast() override;
    QList<MonitorSource> sources() const;
    bool start(const QString &offeredId); // one monitor, Hidden cursor only
    void stop();
Q_SIGNALS:
    void sourcesReady();
    void streamCreated(quint32 node, const QindaQt::CompositorCapture::MonitorSource &source);
    void closed();
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
