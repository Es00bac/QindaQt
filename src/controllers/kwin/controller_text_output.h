// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../text_delivery_p.h"
namespace QindaQt::Controllers {
// GUI-thread adapter over the release-matched KWin SDK. No clipboard, shell,
// synthetic gamepad, or private InputMethod method is used.
class ControllerTextOutput final : public TextOutput {
public:
    QObject *target() const override;
    bool allowed() const override;
    int chunkCodepoints() const override;
    bool insert(const QString &text) override;
};
} // namespace QindaQt::Controllers
