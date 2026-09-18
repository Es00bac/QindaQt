// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsStartup {

// Route-local composition root: owns the concrete XdgAutostartStore and the
// model that projects it to QML. No D-Bus, no public client -- the store is
// a plain local filesystem boundary.
class StartupRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)

public:
    explicit StartupRouteComposition(QObject *parent = nullptr);
    ~StartupRouteComposition() override;
    [[nodiscard]] QObject *model() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsStartup
