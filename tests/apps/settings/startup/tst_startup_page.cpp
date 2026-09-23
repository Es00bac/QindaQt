// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_startup/startup_settings_model.h>
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::SettingsStartup;

namespace {
class FakeStore final : public AutostartStore {
public:
    AutostartEntry entry{
        .id = QStringLiteral("foreign"),
        .name = QStringLiteral("Foreign app"),
        .comment = QStringLiteral("Needs GNOME"),
        .iconName = {},
        .exec = QStringLiteral("foreign-app"),
        .enabled = true,
        .eligible = false,
        .ineligibilityReason = QStringLiteral("Only starts in GNOME"),
        .custom = false,
    };
    int setCalls = 0;
    bool requestedEnabled = true;

    QList<AutostartEntry> list(QString *error) override {
        if (error != nullptr) error->clear();
        return {entry};
    }
    bool setEnabled(const QString &id, bool enabled, QString *error) override {
        if (id != entry.id) {
            if (error != nullptr) *error = QStringLiteral("unknown id");
            return false;
        }
        ++setCalls;
        requestedEnabled = enabled;
        entry.enabled = enabled;
        entry.eligible = false;
        entry.ineligibilityReason = enabled
            ? QStringLiteral("Only starts in GNOME")
            : QStringLiteral("Disabled for this user");
        return true;
    }
    QString addCommand(const QString &, const QString &, QString *error) override {
        if (error != nullptr) *error = QStringLiteral("not supported");
        return {};
    }
    bool removeCustom(const QString &, QString *error) override {
        if (error != nullptr) *error = QStringLiteral("not supported");
        return false;
    }
};

QQuickItem *findItem(QQuickItem *root, const QString &objectName) {
    if (root == nullptr) return nullptr;
    if (root->objectName() == objectName) return root;
    for (QQuickItem *child : root->childItems()) {
        if (auto *found = findItem(child, objectName)) return found;
    }
    return nullptr;
}
}

class StartupPageTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void showsIneligibilityAndRoutesTheSwitch();
};

void StartupPageTest::showsIneligibilityAndRoutesTheSwitch()
{
    QQuickView view;
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
        *view.engine(), &error);
    QVERIFY2(facade != nullptr, qPrintable(error));
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QVERIFY2(facade->publish(theme.theme, {}, &error), qPrintable(error));

    auto store = std::make_unique<FakeStore>();
    FakeStore *raw = store.get();
    StartupSettingsModel model(std::move(store));
    QQmlComponent component(view.engine());
    component.loadUrl(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_STARTUP_PAGE_QML_PATH)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> guard(component.createWithInitialProperties({
        {QStringLiteral("startupSettings"),
         QVariant::fromValue(static_cast<QObject *>(&model))},
    }));
    QVERIFY2(guard != nullptr, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(guard.get());
    QVERIFY(page != nullptr);
    view.resize(900, 700);
    page->setParentItem(view.contentItem());
    page->setSize(QSizeF(900, 700));
    view.show();
    QCoreApplication::processEvents();

    auto *reason = findItem(page, QStringLiteral("startupIneligibility_foreign"));
    auto *toggle = findItem(page, QStringLiteral("startupEnabled_foreign"));
    QVERIFY(reason != nullptr);
    QVERIFY(toggle != nullptr);
    QVERIFY(reason->isVisible());
    QCOMPARE(reason->property("text").toString(),
             QStringLiteral("Only starts in GNOME"));
    QVERIFY(toggle->property("checked").toBool());

    const QPointF center = toggle->mapToScene(
        QPointF(toggle->width() / 2, toggle->height() / 2));
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, center.toPoint());
    QTRY_COMPARE(raw->setCalls, 1);
    QVERIFY(!raw->requestedEnabled);
    QTRY_COMPARE(reason->property("text").toString(),
                 QStringLiteral("Disabled for this user"));
}

QTEST_MAIN(StartupPageTest)
#include "tst_startup_page.moc"
