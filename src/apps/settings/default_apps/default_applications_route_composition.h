// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsDefaultApps {

class DefaultApplicationsRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)

public:
    explicit DefaultApplicationsRouteComposition(QObject *parent = nullptr);
    ~DefaultApplicationsRouteComposition() override;
    [[nodiscard]] QObject *model() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsDefaultApps
