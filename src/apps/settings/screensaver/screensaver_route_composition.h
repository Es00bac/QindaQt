// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsScreensaver {

// Route-local composition root for the Screen saver route (ADR-0226): owns
// the purpose-scoped Settings1 client for the saver pair, the desktop-entry
// catalog the saver list is discovered from, the lock-screen mirror, the
// saver/black-window preview, and the shared screen-lock model the walk-away
// section uses. The page reads both models from this singleton and never
// constructs a client of its own.
class ScreensaverRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)
    Q_PROPERTY(QObject *screenLockSettings READ screenLockSettings CONSTANT)

public:
    explicit ScreensaverRouteComposition(QObject *parent = nullptr);
    ~ScreensaverRouteComposition() override;
    [[nodiscard]] QObject *model() const;
    [[nodiscard]] QObject *screenLockSettings() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsScreensaver
