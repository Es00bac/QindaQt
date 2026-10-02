// SPDX-License-Identifier: GPL-3.0-or-later
// Dialog/result semantics adapted from the four xdg-desktop-portal-kde
// v6.6.6 families; native public widgets and ForeignParent replace KDE UI.
#include "misc_dialog.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QImageReader>
#include <QBuffer>
#include <QLabel>
#include <QLineEdit>
#include <QJsonArray>
#include <QPushButton>
#include <QVBoxLayout>
MiscDialog::MiscDialog(QJsonObject frame) : m_frame(std::move(frame)) {
    setWindowTitle(m_frame.value("title").toString()); setObjectName(QStringLiteral("portalMiscWindow"));
    setWindowModality(m_frame.value("modal").toBool(true) ? Qt::ApplicationModal : Qt::NonModal);
    auto *layout = new QVBoxLayout(this);
    auto addText = [layout](const QString &text) { auto *label = new QLabel(text); label->setTextFormat(Qt::PlainText); label->setWordWrap(true); layout->addWidget(label); };
    addText(m_frame.value("app").toString().isEmpty() ? tr("A host application requests permission.") : m_frame.value("app").toString());
    QList<QCheckBox *> boxes; QLineEdit *name = nullptr;
    const auto kind = m_frame.value("type").toString();
    if (kind == "account") {
        addText(m_frame.value("reason").toString());
        for (const auto *key : {"id", "name", "image"}) {
            auto *box = new QCheckBox(QString::fromLatin1(key) + QStringLiteral(": ") + m_frame.value(key).toString());
            box->setObjectName(QString::fromLatin1(key)); box->setChecked(true); boxes.append(box); layout->addWidget(box);
        }
    } else if (kind == "usb") {
        addText(tr("Select the devices this application may acquire."));
        for (const auto &value : m_frame.value("devices").toArray()) {
            const auto device = value.toObject();
            auto *box = new QCheckBox(device.value("label").toString() + (device.value("writable").toBool() ? tr(" (read and write)") : tr(" (read only)")));
            box->setObjectName(device.value("id").toString()); box->setChecked(true); boxes.append(box); layout->addWidget(box);
        }
    } else if (kind == "launcher") {
        QByteArray bytes = QByteArray::fromBase64(m_frame.value("icon").toString().toLatin1());
        QBuffer buffer(&bytes); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer); reader.setAllocationLimit(64);
        const auto size = reader.size();
        if (!size.isValid() || size.width() > 4096 || size.height() > 4096) { m_response = 2; return; }
        const auto image = reader.read(); if (image.isNull()) { m_response = 2; return; }
        auto *icon = new QLabel; icon->setPixmap(QPixmap::fromImage(image).scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation)); layout->addWidget(icon);
        name = new QLineEdit(m_frame.value("name").toString()); name->setObjectName(QStringLiteral("launcherName")); name->setMaxLength(256);
        name->setReadOnly(!m_frame.value("editable_name").toBool()); layout->addWidget(name); addText(m_frame.value("target").toString());
    } else { m_response = 2; return; }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok); layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setText(kind == "launcher" ? tr("Add Launcher") : tr("Allow"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, boxes, name, kind] {
        QJsonObject result;
        if (kind == "account") for (const auto *box : boxes) result.insert(box->objectName(), box->isChecked());
        else if (kind == "usb") { QJsonArray devices; for (const auto *box : boxes) if (box->isChecked()) devices.append(box->objectName()); result.insert("devices", devices); }
        else { if (!name || name->text().trimmed().isEmpty()) return; result.insert("name", name->text()); }
        acceptResults(result);
    });
    setEnabled(false); resize(480, sizeHint().height());
}
void MiscDialog::markReady() { if (m_response == 2) { fail(); return; } m_ready = true; setEnabled(true); }
void MiscDialog::fail() { m_ready = false; m_results = {}; m_response = 2; done(QDialog::Rejected); }
void MiscDialog::acceptResults(const QJsonObject &results) { if (!m_ready) return; m_results = results; m_response = 0; done(QDialog::Accepted); }
