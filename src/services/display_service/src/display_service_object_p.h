// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_service/display_service_model.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusContext>

#include <functional>

namespace QindaQt::DisplayService
{

class DisplayServiceObject final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Display1")

public:
    explicit DisplayServiceObject(DisplayServiceModel &model,
                                  std::function<void(bool)> transitionCallback,
                                  QObject *parent = nullptr);

    void notifyChanged();

public Q_SLOTS:
    Q_SCRIPTABLE Display::Snapshot GetSnapshot();
    Q_SCRIPTABLE Display::OperationResult Stage(
        const QString &transactionId, const Display::Candidate &candidate);
    Q_SCRIPTABLE Display::OperationResult Preview(const QString &transactionId);
    Q_SCRIPTABLE Display::OperationResult Confirm(const QString &transactionId);
    Q_SCRIPTABLE Display::OperationResult Cancel(const QString &transactionId);

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(const QString &epoch, quint64 revision, bool available);

private:
    Display::OperationResult complete(const ServiceOperationResult &result);
    void unavailableReply();

    DisplayServiceModel &m_model;
    std::function<void(bool)> m_transitionCallback;
};

} // namespace QindaQt::DisplayService
