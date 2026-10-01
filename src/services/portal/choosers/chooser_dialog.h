// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/chooser_types.h>
#include <QDialog>
class ChooserDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChooserDialog(const QindaQt::Services::Portal::AccessQuestion &);
    QJsonObject results() const { return m_results; }
    quint32 response() const { return m_response; }
    void markReady();
    void fail();
    virtual void updateCandidates(const QJsonArray &) {}
protected:
    void succeed(const QJsonObject &);
private:
    QJsonObject m_results;
    quint32 m_response = 1;
    bool m_ready = false;
};
