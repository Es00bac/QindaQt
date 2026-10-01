// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "chooser_dialog.h"
#include "file_filter_proxy.h"
class QFileSystemModel;
class QTreeView;
class QLineEdit;
class QComboBox;
class ChoiceControls;
class FileChooserDialog final : public ChooserDialog {
    Q_OBJECT
public:
    explicit FileChooserDialog(QindaQt::Services::Portal::FileChooserRequest);
public Q_SLOTS:
    void accept() override;
private:
    void navigate(const QString &);
    QStringList selections() const;
    QStringList saveMany(const QString &) const;
    QindaQt::Services::Portal::FileChooserRequest m_request;
    QFileSystemModel *m_files;
    FileFilterProxy *m_proxy;
    QTreeView *m_view;
    QLineEdit *m_folder, *m_name;
    QComboBox *m_filters;
    ChoiceControls *m_choices;
    QString m_currentFolder;
};
