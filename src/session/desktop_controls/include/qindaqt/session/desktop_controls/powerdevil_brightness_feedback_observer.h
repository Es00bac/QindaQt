// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtDBus/QDBusConnection>

#include <QObject>
#include <QString>

class QDBusServiceWatcher;

namespace QindaQt::Session::DesktopControls {

// Observes the public PowerDevil ScreenBrightness signal and projects only
// PowerDevil's own keyboard-step feedback into the existing notifier seam.
// AGENT-CONTRACT: this object never claims brightness shortcuts or writes a
// backlight; PowerDevil remains the sole brightness-key owner and its pinned
// source context is part of the 6.6.6 compatibility boundary.
class PowerDevilBrightnessFeedbackObserver final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

public:
    explicit PowerDevilBrightnessFeedbackObserver(QDBusConnection connection,
                                                  QObject *parent = nullptr);
    ~PowerDevilBrightnessFeedbackObserver() override;

    PowerDevilBrightnessFeedbackObserver(const PowerDevilBrightnessFeedbackObserver &) = delete;
    PowerDevilBrightnessFeedbackObserver &operator=(const PowerDevilBrightnessFeedbackObserver &) = delete;

    void start();
    void stop();
    [[nodiscard]] bool available() const noexcept;

Q_SIGNALS:
    void availabilityChanged(bool available);
    void brightnessFeedbackRequested(int percent);

private Q_SLOTS:
    void ownerChanged(const QString &serviceName, const QString &oldOwner,
                      const QString &newOwner);
    void onBrightnessChanged(const QString &displayName, int brightness,
                             const QString &sourceClientName,
                             const QString &sourceClientContext);

private:
    void setAvailable(bool available);
    void requestMaximum(const QString &displayName, int brightness);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    bool m_signalConnected = false;
    bool m_available = false;
    quint64 m_generation = 0;
};

} // namespace QindaQt::Session::DesktopControls
