// SPDX-License-Identifier: LGPL-3.0-or-later
#include "windowcontainer.h"

#include <QCoreApplication>
#include <utility>

namespace QindaQt::Core {

ContainerPage::ContainerPage(QString pageId, LayoutNode root, QString name, int number)
    : m_id(std::move(pageId)), m_root(std::move(root)), m_name(std::move(name)), m_number(number)
{
}

bool ContainerPage::isValidName(const QString &name)
{
    if (name.size() > 255 || name != name.trimmed() || !name.isValidUtf16()) {
        return false;
    }
    for (const auto character : name) {
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
    }
    return true;
}

QString ContainerPage::displayName() const
{
    return m_name.isEmpty()
        ? QCoreApplication::translate("QindaQt::Core::ContainerPage", "Tab %1").arg(m_number)
        : m_name;
}

bool WindowContainer::renamePage(const QString &pageId, const QString &name, QString *error)
{
    const auto normalized = name.trimmed();
    if (error) {
        error->clear();
    }
    if (!ContainerPage::isValidName(normalized)) {
        if (error) {
            *error = QStringLiteral("Tab names must be at most 255 characters of plain text");
        }
        return false;
    }
    for (auto &candidate : m_pages) {
        if (candidate.id() == pageId) {
            candidate.m_name = normalized;
            return true;
        }
    }
    if (error) {
        *error = QStringLiteral("The selected tab no longer exists");
    }
    return false;
}

} // namespace QindaQt::Core
