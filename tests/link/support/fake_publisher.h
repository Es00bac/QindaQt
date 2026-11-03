// SPDX-License-Identifier: GPL-3.0-or-later
// In-memory ServicePublisher: records what a LanController asked to publish.
#pragma once

#include <qindaqt/services/link_service/service_publisher.h>

namespace QindaQt::Link::Test {

class FakePublisher final : public ServicePublisher {
public:
    void publish(const ServiceAdvert &a) override
    {
        advert = a;
        published = true;
        ++publishCount;
        applyState(QStringLiteral("published"));
    }
    void withdraw() override
    {
        published = false;
        ++withdrawCount;
        applyState(QStringLiteral("off"));
    }
    [[nodiscard]] QString state() const override { return m_state; }
    // Simulates Avahi renaming the instance after a collision.
    void collide()
    {
        advert.name += QStringLiteral("-2");
        applyState(QStringLiteral("renamed"));
    }
    void daemonGone() { applyState(QStringLiteral("unavailable")); }

    ServiceAdvert advert;
    bool published = false;
    int publishCount = 0;
    int withdrawCount = 0;

private:
    void applyState(const QString &s)
    {
        m_state = s;
        Q_EMIT stateChanged(s);
    }
    QString m_state = QStringLiteral("off");
};

} // namespace QindaQt::Link::Test
