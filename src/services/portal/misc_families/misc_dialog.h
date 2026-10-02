// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QJsonObject>
class MiscDialog final : public QDialog {
    Q_OBJECT
public:
    explicit MiscDialog(QJsonObject);
    void markReady();
    void fail();
    int response() const { return m_response; }
    QJsonObject results() const { return m_results; }
private:
    void acceptResults(const QJsonObject &);
    QJsonObject m_frame, m_results; int m_response = 1; bool m_ready = false;
};
