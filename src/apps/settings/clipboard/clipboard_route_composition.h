// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsClipboard {

class ClipboardRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)

public:
    explicit ClipboardRouteComposition(QObject *parent = nullptr);
    ~ClipboardRouteComposition() override;
    [[nodiscard]] QObject *model() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsClipboard
