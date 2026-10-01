// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/chooser_types.h>
#include <QFormLayout>
#include <QWidget>
class ChoiceControls final : public QWidget {
public:
    explicit ChoiceControls(const QindaQt::Services::Portal::AccessChoices &, QWidget *parent = nullptr);
    QJsonArray values() const;
private:
    struct Control { QString id; QWidget *widget = nullptr; };
    QList<Control> m_controls;
};
