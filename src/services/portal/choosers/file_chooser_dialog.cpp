// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_chooser_dialog.h"
#include "choice_controls.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
using namespace QindaQt::Services::Portal;
FileChooserDialog::FileChooserDialog(FileChooserRequest request)
    : ChooserDialog(request.question), m_request(std::move(request)), m_files(new QFileSystemModel(this)),
      m_proxy(new FileFilterProxy(this)), m_view(new QTreeView(this)), m_folder(new QLineEdit(this)),
      m_name(new QLineEdit(this)), m_filters(new QComboBox(this)), m_choices(new ChoiceControls(m_request.question.choices, this)) {
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Folder"), this)); layout->addWidget(m_folder);
    m_folder->setObjectName(QStringLiteral("portalFolder")); m_folder->setAccessibleName(tr("Folder"));
    m_files->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden); m_files->setReadOnly(true);
    m_proxy->setSourceModel(m_files); m_view->setModel(m_proxy);
    m_view->setObjectName(QStringLiteral("portalFiles")); m_view->setAccessibleName(tr("Files and folders"));
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(m_request.multiple ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    m_view->setRootIsDecorated(false); m_view->setSortingEnabled(true); m_view->sortByColumn(0, Qt::AscendingOrder);
    layout->addWidget(m_view, 1);
    m_name->setObjectName(QStringLiteral("portalFilename")); m_name->setAccessibleName(tr("File name"));
    layout->addWidget(new QLabel(tr("File name"), this)); layout->addWidget(m_name);
    m_name->setVisible(!m_request.directory); m_name->setText(m_request.currentName);
    m_filters->setObjectName(QStringLiteral("portalFilter")); m_filters->setAccessibleName(tr("File type"));
    for (const auto &filter : m_request.filters) m_filters->addItem(filter.label);
    m_filters->setCurrentIndex(m_request.currentFilter); m_filters->setVisible(!m_request.filters.isEmpty());
    layout->addWidget(m_filters); layout->addWidget(m_choices);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(m_request.question.grantLabel);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("portalChooserAccept"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("portalChooserCancel"));
    layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::accepted, this, &FileChooserDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_folder, &QLineEdit::returnPressed, this, [this] { navigate(m_folder->text()); });
    connect(m_view, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        const auto info = m_files->fileInfo(m_proxy->mapToSource(index));
        if (info.isDir()) navigate(info.filePath()); else if (!m_request.directory) { m_name->setText(info.fileName()); accept(); }
    });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        m_name->clear();
        const auto selected = m_view->selectionModel()->selectedRows();
        if (selected.size() == 1) { const auto info = m_files->fileInfo(m_proxy->mapToSource(selected.first())); if (info.isFile()) m_name->setText(info.fileName()); }
    });
    connect(m_filters, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_proxy->select(index >= 0 ? m_request.filters.value(index) : FileFilter{}, m_request.directory);
    });
    m_proxy->select(m_request.currentFilter >= 0 ? m_request.filters.value(m_request.currentFilter) : FileFilter{}, m_request.directory);
    QString folder = m_request.folder;
    if (!m_request.currentFile.isEmpty()) { const QFileInfo info(m_request.currentFile); folder = info.absolutePath(); m_name->setText(info.fileName()); }
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) folder = QDir::homePath();
    navigate(folder);
}
void FileChooserDialog::navigate(const QString &folder) {
    const QFileInfo info(folder); const auto canonical = info.canonicalFilePath();
    if (!info.isDir() || canonical.isEmpty()) return;
    m_currentFolder = canonical; m_folder->setText(canonical);
    m_view->setRootIndex(m_proxy->mapFromSource(m_files->setRootPath(canonical)));
}
QStringList FileChooserDialog::selections() const {
    QStringList paths;
    if (m_request.mode == FileChooserMode::Save || (!m_request.directory && !m_name->text().isEmpty()
        && (!m_request.multiple || m_view->selectionModel()->selectedRows().size() <= 1))) {
        const auto name = m_name->text();
        if (name.isEmpty() || name.contains('/') || name.contains(QChar::Null) || name == QStringLiteral(".") || name == QStringLiteral("..")) return {};
        paths.append(QDir(m_currentFolder).filePath(name));
    } else {
        for (const auto &index : m_view->selectionModel()->selectedRows()) paths.append(m_files->filePath(m_proxy->mapToSource(index)));
        if (paths.isEmpty() && m_request.directory) paths.append(m_currentFolder);
    }
    return paths;
}
QStringList FileChooserDialog::saveMany(const QString &folder) const {
    QStringList paths; QSet<QString> reserved;
    for (const auto &name : m_request.files) {
        QString path = QDir(folder).filePath(name);
        // No file is opened/truncated here. Collision-free suggested names
        // avoid implicit overwrite; the eventual writer still owns its race.
        for (int suffix = 1; QFileInfo::exists(path) || QFileInfo(path).isSymbolicLink() || reserved.contains(path); ++suffix) {
            if (suffix > 1000) return {};
            path = QDir(folder).filePath(name + QStringLiteral(" (%1)").arg(suffix));
        }
        reserved.insert(path); paths.append(path);
    }
    return paths;
}
void FileChooserDialog::accept() {
    auto paths = selections();
    if (paths.isEmpty() || paths.size() > 128 || (!m_request.multiple && paths.size() != 1)) return;
    if (m_request.mode == FileChooserMode::SaveMany) {
        const QFileInfo folder(paths.first()); if (!folder.isDir()) return;
        paths = saveMany(folder.canonicalFilePath()); if (paths.size() != m_request.files.size()) return;
    }
    QJsonArray uris;
    for (const auto &path : paths) {
        const QFileInfo info(path); QString normalized;
        if (m_request.mode == FileChooserMode::Open) {
            if (!info.exists() || (m_request.directory ? !info.isDir() : !info.isFile())) return;
            normalized = info.canonicalFilePath();
        } else {
            if (info.exists() || info.isSymbolicLink()) {
                if (!info.isFile()) return;
                if (QMessageBox::question(this, tr("Replace existing file?"), tr("A file with this name already exists. Replace it?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
                normalized = info.canonicalFilePath();
            } else {
                const QFileInfo parent(info.absolutePath()); if (!parent.isDir()) return;
                normalized = QDir(parent.canonicalFilePath()).filePath(info.fileName());
            }
        }
        if (normalized.isEmpty()) return;
        uris.append(QUrl::fromLocalFile(normalized).toString(QUrl::FullyEncoded));
    }
    const QJsonObject result{{"uris", uris}, {"choices", m_choices->values()}, {"filter", m_filters->currentIndex()}};
    if (fileChooserResults(m_request, result)) succeed(result);
}
