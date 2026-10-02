// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QtTest/QTest>
#include <memory>
class PageModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows MEMBER rows NOTIFY changed)
  Q_PROPERTY(bool available MEMBER available NOTIFY changed)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY changed)
public:
  QVariantList rows;
  bool available = true;
  bool busy = false;
  QString errorText;
  QString revoked;
  Q_INVOKABLE bool revoke(const QString &key) { revoked = key; return true; }
  Q_INVOKABLE void refresh() {}
Q_SIGNALS:
  void changed();
};
class PermissionsPageTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void rendersUnavailableAndRevokeAction() {
    QQuickView view; view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(*view.engine(), &error);
    QVERIFY2(facade, qPrintable(error));
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error)); QVERIFY2(facade->publish(theme.theme, {}, &error), qPrintable(error));
    PageModel model;
    model.rows = {QVariantMap{{QStringLiteral("key"), QStringLiteral("opaque-row")},
        {QStringLiteral("app"), QStringLiteral("org.example.App")}, {QStringLiteral("family"), QStringLiteral("Screen sharing")}}};
    view.engine()->rootContext()->setContextProperty(QStringLiteral("testPermissions"), &model);
    QQmlComponent component(view.engine());
    component.setData(R"QML(
import QtQuick
import QindaQt.SettingsApp.PortalPermissions
Loader {
    active: true
    sourceComponent: Component {
        PortalPermissionsPage { permissions: testPermissions }
    }
}
)QML", QUrl());
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(page.get()); QVERIFY(item);
    item->setParentItem(view.contentItem()); item->setSize({640,480}); view.resize(640,480); view.show();
    QObject *button = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((button = find(item, QStringLiteral("portalPermissionRevoke"))) != nullptr, 1500);
    QVERIFY(button->property("enabled").toBool()); QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
    QCOMPARE(model.revoked, QStringLiteral("opaque-row"));
    model.available = false; model.errorText = QStringLiteral("Unavailable"); emit model.changed();
    QTRY_VERIFY(!button->property("enabled").toBool());
    const auto *notice = find(item, QStringLiteral("portalPermissionsError")); QVERIFY(notice);
    QCOMPARE(notice->property("text").toString(), QStringLiteral("Unavailable"));
  }
private:
  static QObject *find(QQuickItem *root, const QString &name) {
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *result = find(child, name)) return result;
    return nullptr;
  }
};
QTEST_MAIN(PermissionsPageTest)
#include "tst_permissions_page.moc"
