// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "chooser_dialog.h"
class QListWidget;
class AppChooserDialog final : public ChooserDialog {
    Q_OBJECT
public:
    explicit AppChooserDialog(QindaQt::Services::Portal::AppChooserRequest);
    void updateCandidates(const QJsonArray &) override;
public Q_SLOTS:
    void accept() override;
private:
    QindaQt::Services::Portal::AppChooserRequest m_request;
    QListWidget *m_list;
};
