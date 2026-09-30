// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <memory>

namespace QindaQt::Apps::SettingsKeyring {
class KeyringRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)
public:
    explicit KeyringRouteComposition(QObject *parent = nullptr);
    ~KeyringRouteComposition() override;
    [[nodiscard]] QObject *model() const;
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Apps::SettingsKeyring
