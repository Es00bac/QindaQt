// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller_policy.h"

namespace QindaQt::Controllers {
// Borrowed for Runtime's lifetime, called only on its Qt GUI thread. Every
// pressed token must be released by reset(), including a dictation capture.
class InputSink {
public:
    virtual ~InputSink() = default;
    virtual void button(const QString &token, const Binding &binding, bool down) = 0;
    virtual void motion(QPointF delta) = 0;
    virtual void scroll(QPointF delta) = 0;
    virtual void reset(const QString &controllerId = {}) = 0;
};
} // namespace QindaQt::Controllers
