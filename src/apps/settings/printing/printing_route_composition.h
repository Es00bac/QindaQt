// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <memory>

namespace QindaQt::Apps::SettingsPrinting {
class PrintingRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)
public:
    explicit PrintingRouteComposition(QObject *parent = nullptr);
    ~PrintingRouteComposition() override;
    [[nodiscard]] QObject *model() const;
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
