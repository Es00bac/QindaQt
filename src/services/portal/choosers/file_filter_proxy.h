// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/chooser_types.h>
#include <QSortFilterProxyModel>
class FileFilterProxy final : public QSortFilterProxyModel {
public:
    explicit FileFilterProxy(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}
    void select(const QindaQt::Services::Portal::FileFilter &filter, bool directories);
protected:
    bool filterAcceptsRow(int, const QModelIndex &) const override;
private:
    QindaQt::Services::Portal::FileFilter m_filter;
    bool m_directories = false;
};
