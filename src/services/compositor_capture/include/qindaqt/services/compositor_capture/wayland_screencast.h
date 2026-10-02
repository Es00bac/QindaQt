// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>
#include <memory>
namespace QindaQt::CompositorCapture {
struct MonitorSource { QString id, name; QPoint position; QSize size; };
// Owns one supplied native Wayland FD (ordinary or separately tagged), up to
// sixteen explicitly selected monitor streams and generated protocol
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
    bool start(const QString &offeredId, quint32 cursorMode = 1);
    // One atomic selection; duplicate/unoffered IDs and nonstandard cursor
    // modes fail before any producer request. A failed member retires the batch.
    bool start(const QStringList &offeredIds, quint32 cursorMode = 1);
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
