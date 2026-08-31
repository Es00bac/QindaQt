// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/portal/appearance_policy.h"

#include <QObject>

#include <optional>

namespace QindaQt::Services::Portal {

struct AppearanceTruth final {
    AppearancePolicy policy;
    QString owner;
    QString epoch;
    quint64 revision = 0;

    [[nodiscard]] bool operator==(const AppearanceTruth &) const = default;
};

// Thread-confined source port owned by the portal service. `current()` is
// authoritative only while engaged. Loss or replacement must disengage it
// before any replacement baseline can be published.
class AppearanceSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~AppearanceSource() override = default;

    [[nodiscard]] virtual bool start(QString *error = nullptr) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual const std::optional<AppearanceTruth> &current() const = 0;
    [[nodiscard]] virtual QString diagnostic() const = 0;

Q_SIGNALS:
    void currentChanged();
};

} // namespace QindaQt::Services::Portal
