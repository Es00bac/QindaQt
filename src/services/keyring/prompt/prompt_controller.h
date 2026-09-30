// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QString>
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
    Q_PROPERTY(QString status MEMBER status NOTIFY statusChanged)
public:
    bool creation = false, changing = false, revealing = false, confirming = false;
    QString label;
    QString status;
    Q_INVOKABLE bool approve(QString password, QString confirmation, QString oldPassword = {});
    Q_INVOKABLE void cancel();
Q_SIGNALS:
    void clearFields();
    void statusChanged();
};
}
