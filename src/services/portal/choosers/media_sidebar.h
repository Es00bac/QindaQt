// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
class ChooserMediaPresenter;
class QListWidget;
class QLabel;
class QPushButton;
class ChooserMediaSidebar final : public QWidget {
    Q_OBJECT
public:
    explicit ChooserMediaSidebar(ChooserMediaPresenter &, QWidget *parent = nullptr);
private:
    void populate();
    void update();
    QString handle() const;
    ChooserMediaPresenter &m_presenter;
    QListWidget *m_devices;
    QLabel *m_notice;
    QPushButton *m_open, *m_readOnly, *m_unmount, *m_remove, *m_details, *m_recovery, *m_owner;
};
