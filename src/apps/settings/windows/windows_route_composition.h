// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsWindows {

// Route-local composition root (like ClipboardRouteComposition): one public
// Settings1 transport and a client scoped to exactly the four consumed
// windowManagement keys, handed to QML only as the route model.
class WindowsRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)

public:
    explicit WindowsRouteComposition(QObject *parent = nullptr);
    ~WindowsRouteComposition() override;
    [[nodiscard]] QObject *model() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsWindows
