// SPDX-License-Identifier: GPL-3.0-or-later
#include "chooser_dialog.h"
ChooserDialog::ChooserDialog(const QindaQt::Services::Portal::AccessQuestion &question) {
    setWindowTitle(question.title); setObjectName(QStringLiteral("portalChooserWindow"));
    setWindowModality(question.modal ? Qt::ApplicationModal : Qt::NonModal);
    resize(760, 540); setEnabled(false);
}
void ChooserDialog::markReady() { m_ready = true; setEnabled(true); }
void ChooserDialog::fail() { m_ready = false; m_results = {}; m_response = 2; done(QDialog::Rejected); }
void ChooserDialog::succeed(const QJsonObject &results) {
    if (!m_ready) return;
    m_results = results; m_response = 0; done(QDialog::Accepted);
}
