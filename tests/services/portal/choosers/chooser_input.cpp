// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTreeView>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>
#include <QtTest/QTest>
#include <wayland-client.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
// Test-only visible input linked to unchanged production dialog/main. No
// result injection, production automation API or synthetic helper response.
bool audited = false, entered = false;
void input() {
    const auto desired = QJsonDocument::fromJson(qgetenv("QINDAQT_CHOOSER_TEST_INPUT")).object();
    for (auto *widget : QApplication::topLevelWidgets()) {
        if (auto *message = qobject_cast<QMessageBox *>(widget); message && message->isVisible()) {
            auto *button = message->button(desired.value("overwrite").toBool() ? QMessageBox::Yes : QMessageBox::No);
            if (button) QTest::mouseClick(button, Qt::LeftButton);
            if (!desired.value("overwrite").toBool()) {
                for (auto *window : QApplication::topLevelWidgets()) if (window->objectName() == QStringLiteral("portalChooserWindow")) {
                    auto *cancel = window->findChild<QPushButton *>(QStringLiteral("portalChooserCancel")); if (cancel) QTest::mouseClick(cancel, Qt::LeftButton);
                }
            }
            return;
        }
    }
    for (auto *window : QApplication::topLevelWidgets()) {
        if (window->objectName() != QStringLiteral("portalChooserWindow") || !window->isVisible()
            || !window->isEnabled() || !window->windowHandle() || !window->windowHandle()->isExposed()) continue;
        if (!audited) {
            auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
            ucred peer{}; socklen_t length = sizeof(peer);
            if (!native || getsockopt(wl_display_get_fd(native->display()), SOL_SOCKET, SO_PEERCRED, &peer, &length) != 0
                || peer.pid != qEnvironmentVariableIntValue("QINDAQT_PORTAL_TEST_COMPOSITOR_PID")) { QCoreApplication::exit(3); return; }
            QFile audit(qEnvironmentVariable("QINDAQT_CHOOSER_TEST_AUDIT"));
            if (!audit.open(QIODevice::WriteOnly | QIODevice::Append)) { QCoreApplication::exit(4); return; }
            audit.write(QByteArray::number(getpid()) + " ordinary-wayland exact-peer mapped\n"); audited = true;
        }
        if (desired.value("hold").toBool()) return;
        if (desired.value("cancel").toBool()) {
            auto *cancel = window->findChild<QPushButton *>(QStringLiteral("portalChooserCancel")); if (cancel) QTest::mouseClick(cancel, Qt::LeftButton); return;
        }
        if (auto *list = window->findChild<QListWidget *>(QStringLiteral("portalApplications"))) {
            const auto id = desired.value("app").toString(); bool found = false;
            for (int row = 0; row < list->count(); ++row) {
                auto *item = list->item(row); if (item->data(Qt::UserRole).toString() != id) continue;
                list->scrollToItem(item); QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(item).center());
                QFile audit(qEnvironmentVariable("QINDAQT_CHOOSER_TEST_AUDIT"));
                if (audit.open(QIODevice::WriteOnly | QIODevice::Append)) audit.write("selected-app " + item->data(Qt::UserRole).toString().toUtf8() + '\n');
                found = true; break;
            }
            if (!found) { QTimer::singleShot(100, qApp, input); return; }
        } else {
            auto *folder = window->findChild<QLineEdit *>(QStringLiteral("portalFolder"));
            if (!entered && desired.value("folder").isString() && folder) {
                folder->setFocus(); QTest::keyClick(folder, Qt::Key_A, Qt::ControlModifier); QTest::keyClicks(folder, desired.value("folder").toString()); QTest::keyClick(folder, Qt::Key_Return);
                entered = true; QTimer::singleShot(200, qApp, input); return;
            }
            auto *view = window->findChild<QTreeView *>(QStringLiteral("portalFiles"));
            const auto files = desired.value("files").toArray();
            if (!files.isEmpty()) {
                for (qsizetype item = 0; item < files.size(); ++item) {
                    bool found = false;
                    for (int row = 0; row < view->model()->rowCount(view->rootIndex()); ++row) {
                        const auto index = view->model()->index(row, 0, view->rootIndex());
                        if (index.data().toString() != files[item].toString()) continue;
                        view->scrollTo(index); QTest::mouseClick(view->viewport(), Qt::LeftButton,
                            item == 0 ? Qt::NoModifier : Qt::ControlModifier, view->visualRect(index).center()); found = true; break;
                    }
                    if (!found) { QTimer::singleShot(100, qApp, input); return; }
                }
            }
            if (desired.value("name").isString()) {
                auto *name = window->findChild<QLineEdit *>(QStringLiteral("portalFilename"));
                name->setFocus(); QTest::keyClick(name, Qt::Key_A, Qt::ControlModifier); QTest::keyClicks(name, desired.value("name").toString());
            }
            if (desired.value("filter").isDouble()) {
                auto *filter = window->findChild<QComboBox *>(QStringLiteral("portalFilter"));
                filter->setFocus(); QTest::keyClick(filter, Qt::Key_Home);
                for (int index = 0; index < desired.value("filter").toInt(); ++index) QTest::keyClick(filter, Qt::Key_Down);
            }
            if (desired.value("check").toBool()) {
                auto *check = window->findChild<QCheckBox *>(QStringLiteral("portalChoice_check"));
                if (check && !check->isChecked()) { check->setFocus(); QTest::keyClick(check, Qt::Key_Space); }
            }
            if (desired.value("choice").isDouble()) {
                auto *choice = window->findChild<QComboBox *>(QStringLiteral("portalChoice_encoding"));
                choice->setFocus(); QTest::keyClick(choice, Qt::Key_Home);
                for (int index = 0; index < desired.value("choice").toInt(); ++index) QTest::keyClick(choice, Qt::Key_Down);
            }
        }
        auto *button = window->findChild<QPushButton *>(QStringLiteral("portalChooserAccept"));
        // The overwrite question runs a nested event loop inside the click.
        // Schedule input first so its real visible buttons remain exercisable.
        QTimer::singleShot(100, qApp, input);
        if (button && button->isEnabled()) QTest::mouseClick(button, Qt::LeftButton);
        return;
    }
    QTimer::singleShot(100, qApp, input);
}
void start() { QTimer::singleShot(100, QCoreApplication::instance(), input); }
Q_COREAPP_STARTUP_FUNCTION(start)
}
