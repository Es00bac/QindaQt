// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_model.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>

#include <functional>
#include <vector>

namespace QindaQt::DisplayService
{

class DisplayServiceObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Display1")

public:
    explicit DisplayServiceObject(DisplayServiceModel &model,
                                  std::function<void(bool)> transitionCallback,
                                  std::function<void()> brightnessCallback,
                                  QObject *parent = nullptr);

    void notifyChanged();
    // Sends the delayed D-Bus reply for an accepted immediate request. A
    // finish whose caller did not arrive over D-Bus has no reply to send.
    void finishBrightness(const BrightnessFinish &finish);

public Q_SLOTS:
    Q_SCRIPTABLE Display::Snapshot GetSnapshot();
    Q_SCRIPTABLE Display::OperationResult Stage(
        const QString &transactionId, const Display::Candidate &candidate);
    Q_SCRIPTABLE Display::OperationResult Preview(const QString &transactionId);
    Q_SCRIPTABLE Display::OperationResult Confirm(const QString &transactionId);
    Q_SCRIPTABLE Display::OperationResult Cancel(const QString &transactionId);
    Q_SCRIPTABLE Display::BrightnessSnapshot GetBrightness();
    Q_SCRIPTABLE Display::OperationResult SetOutputBrightness(
        const Display::BrightnessRequest &request);

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(const QString &epoch, quint64 revision, bool available);

private:
    struct DelayedReply {
        quint64 requestId = 0;
        QDBusConnection connection;
        QDBusMessage call;
    };

    Display::OperationResult complete(const ServiceOperationResult &result);
    void unavailableReply();

    DisplayServiceModel &m_model;
    std::function<void(bool)> m_transitionCallback;
    std::function<void()> m_brightnessCallback;
    std::vector<DelayedReply> m_brightnessReplies;
};

} // namespace QindaQt::DisplayService
