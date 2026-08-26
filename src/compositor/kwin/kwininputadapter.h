// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "normalizedinputevent.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

#include <memory>

namespace KWin {
class InputDevice;
class InputRedirection;
}

namespace QindaQt::Compositor::KWinIntegration {

class KWinInputAdapter final : public QObject
{
    Q_OBJECT

public:
    explicit KWinInputAdapter(KWin::InputRedirection *input, QObject *parent = nullptr);
    ~KWinInputAdapter() override;

    [[nodiscard]] bool observerActive() const;
    [[nodiscard]] QJsonObject capabilitiesJson() const;

Q_SIGNALS:
    void capabilitiesChanged();
    // AGENT-CONTRACT: This pre-filter stream stays process-local. Consumers
    // must enforce lock-state policy and must not log or export raw events.
    void inputEventObserved(
        const QindaQt::Compositor::KWinIntegration::NormalizedInputEvent &event);

private:
    class EventSpy;

    void trackDevice(KWin::InputDevice *device);
    void untrackDevice(KWin::InputDevice *device);
    [[nodiscard]] QString deviceId(KWin::InputDevice *device);
    void observe(NormalizedInputEvent event);

    QPointer<KWin::InputRedirection> m_input;
    std::unique_ptr<EventSpy> m_spy;
    QHash<KWin::InputDevice *, QString> m_deviceIds;
    quint64 m_nextDeviceId = 1;
    bool m_observerActive = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
