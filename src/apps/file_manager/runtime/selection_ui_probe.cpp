// SPDX-License-Identifier: GPL-3.0-or-later
#include "selection_ui_probe.h"
#include "model/navigation_controller.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJSValue>
#include <QKeyEvent>
#include <QVariant>

namespace QindaQt::Apps::FileManager {
bool verifySelectionUi(QObject *root, NavigationController *navigation,
                       const QString &fixtureRoot, QString *error) {
    const auto reject = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    auto *selection = root->findChild<QObject *>(QStringLiteral("entrySelection"));
    auto *list = root->findChild<QObject *>(QStringLiteral("entryListView"));
    auto *grid = root->findChild<QObject *>(QStringLiteral("entryGridView"));
    if (!selection || !list || !grid) return reject(QStringLiteral("missing selection views"));
    const QString directory = QDir(fixtureRoot).filePath(QStringLiteral("selection-fixture"));
    if (!QDir().mkpath(directory)) return reject(QStringLiteral("selection fixture directory"));
    for (int i = 0; i < 4; ++i) {
        QFile file(QDir(directory).filePath(QString(QChar('a' + i))));
        if (!file.open(QIODevice::WriteOnly) || file.write(QByteArray(4-i, 'x')) != 4-i)
            return reject(QStringLiteral("selection fixture file"));
    }
    const auto call = [selection](const char *method, int index) {
        return QMetaObject::invokeMethod(selection, method, Q_ARG(QVariant, QVariant(index)));
    };
    const auto names = [selection]() {
        QVariant result;
        if (!QMetaObject::invokeMethod(selection, "selectedEntries", Q_RETURN_ARG(QVariant, result)))
            return QStringList{QStringLiteral("invoke failed")};
        if (result.canConvert<QJSValue>()) result = result.value<QJSValue>().toVariant();
        QStringList value;
        for (const auto &entry : result.toList()) value.append(entry.toMap().value(QStringLiteral("name")).toString());
        value.sort();
        return value;
    };
    const auto pump = [] { QCoreApplication::processEvents(); };
    const auto key = [root, &pump](int code, Qt::KeyboardModifiers modifiers) {
        QKeyEvent press(QEvent::KeyPress, code, modifiers);
        QKeyEvent release(QEvent::KeyRelease, code, modifiers);
        QCoreApplication::sendEvent(root, &press);
        QCoreApplication::sendEvent(root, &release);
        pump();
    };
    navigation->navigateTo(directory); pump();
    call("selectOnly", navigation->indexOfName(QStringLiteral("b")));
    call("toggle", navigation->indexOfName(QStringLiteral("d")));
    navigation->setSortColumn(QStringLiteral("size")); pump();
    if (names() != QStringList{QStringLiteral("b"),QStringLiteral("d")})
        return reject(QStringLiteral("sort changed selected identities"));
    navigation->refresh(); pump();
    if (names() != QStringList{QStringLiteral("b"),QStringLiteral("d")})
        return reject(QStringLiteral("refresh changed selected identities"));
    navigation->setViewMode(QStringLiteral("grid")); pump();
    if (names() != QStringList{QStringLiteral("b"),QStringLiteral("d")}
        || grid->property("currentIndex") != list->property("currentIndex"))
        return reject(QStringLiteral("grid switch lost selection/focus"));
    call("selectOnly", 0); pump();
    QMetaObject::invokeMethod(grid, "forceActiveFocus");
    key(Qt::Key_Right, Qt::ShiftModifier);
    if (names().size() != 2) return reject(QStringLiteral("grid Shift+Right did not extend selection"));
    key(Qt::Key_Left, Qt::NoModifier);
    if (names().size() != 1) return reject(QStringLiteral("grid arrow did not replace selection"));
    navigation->setViewMode(QStringLiteral("list")); pump();
    QMetaObject::invokeMethod(list, "forceActiveFocus");
    key(Qt::Key_Down, Qt::ShiftModifier);
    if (names().size() != 2) return reject(QStringLiteral("list Shift+Down did not extend selection"));
    key(Qt::Key_Down, Qt::NoModifier);
    if (names().size() != 1) return reject(QStringLiteral("list arrow did not replace selection"));
    navigation->navigateTo(fixtureRoot); pump();
    if (!names().isEmpty()) return reject(QStringLiteral("navigation retained selected files"));
    navigation->setSortColumn(QStringLiteral("name"));
    if (!QDir(directory).removeRecursively()) return reject(QStringLiteral("selection fixture cleanup"));
    navigation->refresh();
    return true;
}
}
