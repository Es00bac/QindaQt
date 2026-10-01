// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_chooser_dialog.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
using namespace QindaQt::Services::Portal;
AppChooserDialog::AppChooserDialog(AppChooserRequest request)
    : ChooserDialog(request.question), m_request(std::move(request)), m_list(new QListWidget(this)) {
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(m_request.filename.isEmpty() ? m_request.contentType : m_request.filename, this);
    description->setTextFormat(Qt::PlainText); description->setWordWrap(true); layout->addWidget(description);
    m_list->setObjectName(QStringLiteral("portalApplications")); m_list->setAccessibleName(tr("Applications")); layout->addWidget(m_list, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this); layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Open)->setObjectName(QStringLiteral("portalChooserAccept"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("portalChooserCancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AppChooserDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { accept(); });
    updateCandidates(candidateFrame(m_request.candidates));
}
void AppChooserDialog::updateCandidates(const QJsonArray &candidates) {
    const auto current = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : m_request.lastChoice;
    auto frame = appChooserFrame(m_request); frame.insert(QStringLiteral("candidates"), candidates);
    const auto updated = appChooserFromFrame(frame); if (!updated) { fail(); return; }
    m_request = *updated; m_list->clear();
    for (const auto &candidate : m_request.candidates) {
        auto *item = new QListWidgetItem(candidate.label, m_list); item->setData(Qt::UserRole, candidate.id);
        if (candidate.id == current) m_list->setCurrentItem(item);
    }
}
void AppChooserDialog::accept() {
    if (!m_list->currentItem()) return;
    const QJsonObject result{{"choice", m_list->currentItem()->data(Qt::UserRole).toString()}};
    if (appChooserResults(m_request, result)) succeed(result);
}
