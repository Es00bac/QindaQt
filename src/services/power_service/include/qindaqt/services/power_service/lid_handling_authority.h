// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
namespace QindaQt::Power {
// Same-thread borrowed platform admission. The implementation owns its exact
// logind handle-lid-switch block FD; false revokes synchronously and closes all
// local duplicates. Generation is an equality token, not an ordered value.
// Read-through admission includes current platform/session owner and active
// selected-session checks. Failure never grants ownership or replays an edge.
class LidHandlingAuthority : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~LidHandlingAuthority() override = default;
    virtual void setEnabled(bool enabled) = 0;
    [[nodiscard]] virtual bool admitted() const = 0;
    [[nodiscard]] virtual quint64 generation() const = 0;
Q_SIGNALS:
    void admissionChanged();
};
}
