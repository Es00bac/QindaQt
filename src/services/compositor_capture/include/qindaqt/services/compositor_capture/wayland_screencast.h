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
// Owns one supplied native Wayland FD (ordinary or separately tagged), one monitor stream and generated protocol
// resources. No pathname/display fallback, permission, persistence or UI policy.
// Same-thread borrowed lineage/pixel callbacks are readonly/non-reentrant and
// outlive the port. Source enumeration requires lineage; stream creation and
// publication additionally require pixel permission. Loss invalidates getters before
// notifying and tears down safely after protocol dispatch. Never restart/replay.
class WaylandScreenCast final : public QObject {
    Q_OBJECT
public:
    WaylandScreenCast(int ownedNativeFd, std::function<bool()> lineage,
        std::function<bool()> pixelsAllowed, QObject *parent = nullptr);
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
