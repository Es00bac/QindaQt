// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_transport/feature.h>

#include <functional>

namespace QindaQt::Link {

// Desktop-to-phone `ring`. The phone never sends one, so incoming frames are
// answered unsupported. The result callback gets ok/err with a protocol code.
class RingFeature final : public FeatureHandler {
public:
    [[nodiscard]] QString ns() const override { return QStringLiteral("ring"); }
    void handle(Session &session, const Envelope &envelope) override;
    static void ring(Session &session, std::function<void(bool ok, const QString &code)> done);
};

} // namespace QindaQt::Link
