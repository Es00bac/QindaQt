// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QString>
#include <qindaqt/services/keyring_protocol/prompt_metadata.h>
namespace qindaqt::keyring::service {
// One process/request. QML/framework text allocations cannot be promised locked
// or entirely wiped; explicit owned copies are wiped, fields clear, dump disabled,
// and the process exits immediately after approved pipe output (ADR-0296).
class PromptController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool creation MEMBER creation CONSTANT)
    Q_PROPERTY(bool changing MEMBER changing CONSTANT)
    Q_PROPERTY(bool revealing MEMBER revealing CONSTANT)
    Q_PROPERTY(bool confirming MEMBER confirming CONSTANT)
    Q_PROPERTY(QString label MEMBER label CONSTANT)
    Q_PROPERTY(QString requester MEMBER requester CONSTANT)
    Q_PROPERTY(QString requesterDetails MEMBER requesterDetails CONSTANT)
    Q_PROPERTY(QString parentContext MEMBER parentContext CONSTANT)
    Q_PROPERTY(QString operation MEMBER operation CONSTANT)
    Q_PROPERTY(QString scope MEMBER scope CONSTANT)
    Q_PROPERTY(QString application MEMBER application CONSTANT)
    Q_PROPERTY(bool requesterKnown MEMBER requesterKnown CONSTANT)
    Q_PROPERTY(bool executableKnown MEMBER executableKnown CONSTANT)
    Q_PROPERTY(bool reviewed READ reviewed NOTIFY reviewChanged)
    Q_PROPERTY(QString status MEMBER status NOTIFY statusChanged)
public:
    bool creation = false, changing = false, revealing = false, confirming = false;
    QString label;
    QString status;
    QString requester, requesterDetails, parentContext, operation, scope, application;
    bool requesterKnown = false, executableKnown = false;
    // Set once before QML construction. Reject mixed/unknown action metadata.
    bool setRequest(const QString &action, const protocol::PromptMetadata &);
    bool reviewed() const { return reviewed_; }
    Q_INVOKABLE bool review();
    Q_INVOKABLE bool approve(QString password, QString confirmation, QString oldPassword = {});
    Q_INVOKABLE void cancel();
Q_SIGNALS:
    void clearFields();
    void statusChanged();
    void reviewChanged();
private:
    bool reviewed_ = false;
};
}
