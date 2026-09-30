// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <QObject>
class ConsentController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString subtitle READ subtitle CONSTANT)
    Q_PROPERTY(QString body READ body CONSTANT)
    Q_PROPERTY(QString appId READ appId CONSTANT)
    Q_PROPERTY(QString grantLabel READ grantLabel CONSTANT)
    Q_PROPERTY(QString denyLabel READ denyLabel CONSTANT)
    Q_PROPERTY(QVariantList choices READ choices CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
public:
    explicit ConsentController(QindaQt::Services::Portal::AccessQuestion question);
    QString title() const { return m_question.title; }
    QString subtitle() const { return m_question.subtitle; }
    QString body() const { return m_question.body; }
    QString appId() const { return m_question.appId; }
    QString grantLabel() const { return m_question.grantLabel; }
    QString denyLabel() const { return m_question.denyLabel; }
    QVariantList choices() const;
    bool ready() const { return m_ready; }
    void markReady();
    Q_INVOKABLE void choose(const QString &id, const QString &value);
    Q_INVOKABLE void grant();
    Q_INVOKABLE void deny();
    void fail();
Q_SIGNALS:
    void readyChanged();
private:
    void finish(quint32 response);
    QindaQt::Services::Portal::AccessQuestion m_question;
    QindaQt::Services::Portal::ChoiceValues m_values;
    bool m_ready = false, m_finished = false;
};
