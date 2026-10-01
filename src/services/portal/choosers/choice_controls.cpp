// SPDX-License-Identifier: GPL-3.0-or-later
#include "choice_controls.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
ChoiceControls::ChoiceControls(const QindaQt::Services::Portal::AccessChoices &choices, QWidget *parent) : QWidget(parent) {
    auto *form = new QFormLayout(this);
    for (const auto &choice : choices) {
        QWidget *widget;
        if (choice.options.isEmpty()) {
            auto *check = new QCheckBox(choice.label, this); check->setChecked(choice.initial == QStringLiteral("true")); widget = check;
            form->addRow(check);
        } else {
            auto *combo = new QComboBox(this);
            for (const auto &option : choice.options) combo->addItem(option.label, option.id);
            const int selected = combo->findData(choice.initial); if (selected >= 0) combo->setCurrentIndex(selected);
            auto *label = new QLabel(choice.label, this); label->setTextFormat(Qt::PlainText);
            widget = combo; form->addRow(label, combo);
        }
        widget->setObjectName(QStringLiteral("portalChoice_") + choice.id); m_controls.append({choice.id, widget});
    }
}
QJsonArray ChoiceControls::values() const {
    QJsonArray result;
    for (const auto &control : m_controls) {
        QString value;
        if (auto *check = qobject_cast<QCheckBox *>(control.widget)) value = check->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
        else value = qobject_cast<QComboBox *>(control.widget)->currentData().toString();
        result.append(QJsonObject{{"id", control.id}, {"value", value}});
    }
    return result;
}
