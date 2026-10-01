// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_filter_proxy.h"
#include <QFileSystemModel>
#include <QMimeDatabase>
#include <QRegularExpression>
void FileFilterProxy::select(const QindaQt::Services::Portal::FileFilter &filter, bool directories) {
    beginFilterChange(); m_filter = filter; m_directories = directories; endFilterChange(Direction::Rows);
}
bool FileFilterProxy::filterAcceptsRow(int row, const QModelIndex &parent) const {
    const auto *model = qobject_cast<QFileSystemModel *>(sourceModel()); if (!model) return false;
    const auto info = model->fileInfo(model->index(row, 0, parent));
    if (info.isDir()) return true;
    if (m_directories) return false;
    if (m_filter.rules.isEmpty()) return true;
    for (const auto &rule : m_filter.rules) {
        if (rule.kind == 0) {
            const QRegularExpression expression(QRegularExpression::wildcardToRegularExpression(rule.pattern));
            if (expression.match(info.fileName()).hasMatch()) return true;
        } else {
            const auto mime = QMimeDatabase{}.mimeTypeForFile(info, QMimeDatabase::MatchDefault);
            if (mime.name() == rule.pattern || mime.inherits(rule.pattern)) return true;
        }
    }
    return false;
}
