// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

class QObject;

namespace QindaQt::Controls::TestSupport {

class StateCardAccessibilityProbe final {
public:
    StateCardAccessibilityProbe();
    ~StateCardAccessibilityProbe();

    StateCardAccessibilityProbe(const StateCardAccessibilityProbe &) = delete;
    StateCardAccessibilityProbe &operator=(const StateCardAccessibilityProbe &) = delete;
    StateCardAccessibilityProbe(StateCardAccessibilityProbe &&) = delete;
    StateCardAccessibilityProbe &operator=(StateCardAccessibilityProbe &&) = delete;

    void verifyConstructionSilence() const;
    void verifyAnnouncements(QObject *stateCard);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace QindaQt::Controls::TestSupport
