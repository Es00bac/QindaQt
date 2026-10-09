// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>
#include <optional>

namespace QindaQt::AndroidManager {
enum class SetupTarget { Runtime, Images };
enum class SetupOutcomeCode { Accepted, Unavailable, Failed, InvalidTarget };
struct SetupOutcome {
    SetupOutcomeCode code = SetupOutcomeCode::Unavailable;
    QString message;
    [[nodiscard]] bool accepted() const { return code == SetupOutcomeCode::Accepted; }
};

// The existing Software Manager CLI receives only these closed package
// selectors. This pure function returns no command/shell or arbitrary atom.
[[nodiscard]] std::optional<QStringList> softwareSetupArguments(SetupTarget target);

// AGENT-CONTRACT: Borrowed on the controller's GUI thread; QObject loss
// withdraws the action. Opens native package planning only. It never installs,
// initializes/mounts a guest, mints registration or reports Android Running.
// QindaPortage retains its own plan preview/approval/helper authority.
class SetupPort : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual bool available() const = 0;
    [[nodiscard]] virtual SetupOutcome openSelection(SetupTarget target) = 0;
Q_SIGNALS:
    void availabilityChanged();
};
} // namespace QindaQt::AndroidManager
