// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_surface_qml_test_support.h"

#include <QDir>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopSurfacePlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Tests::DesktopSurface;

namespace {

// Recording stand-in for the borrowed LiveCustomizationController facade.
class StubCustomization final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool editMode READ editMode NOTIFY editModeChanged)
    Q_PROPERTY(bool canUndo READ canUndo CONSTANT)
    Q_PROPERTY(int chordModifiers READ chordModifiers CONSTANT)
    Q_PROPERTY(bool customizeRouteAvailable READ customizeRouteAvailable CONSTANT)

public:
    [[nodiscard]] bool available() const { return true; }
    [[nodiscard]] bool editMode() const { return m_editMode; }
    [[nodiscard]] bool canUndo() const { return true; }
    [[nodiscard]] int chordModifiers() const { return static_cast<int>(Qt::MetaModifier); }
    [[nodiscard]] bool customizeRouteAvailable() const { return true; }

    Q_INVOKABLE QVariantList appletSettingRows(const QString &owner, const QString &applet)
    {
        calls.append({QStringLiteral("appletSettingRows"), owner, applet});
        return {QVariantMap{{QStringLiteral("key"), QStringLiteral("snapToGrid")},
                            {QStringLiteral("title"), QStringLiteral("Snap to grid")},
                            {QStringLiteral("kind"), QStringLiteral("boolean")},
                            {QStringLiteral("value"), true}},
                QVariantMap{{QStringLiteral("key"), QStringLiteral("placement")},
                            {QStringLiteral("title"), QStringLiteral("Placement")},
                            {QStringLiteral("kind"), QStringLiteral("choice")},
                            {QStringLiteral("value"), QStringLiteral("left")},
                            {QStringLiteral("choices"),
                             QStringList{QStringLiteral("left"), QStringLiteral("right")}}}};
    }
    Q_INVOKABLE bool setAppletSetting(const QString &owner, const QString &applet,
                                      const QString &key, const QVariant &value)
    {
        calls.append({QStringLiteral("setAppletSetting"), owner, applet, key, value});
        return true;
    }
    Q_INVOKABLE bool addPanel(const QString &edge)
    {
        calls.append({QStringLiteral("addPanel"), edge});
        return true;
    }
    Q_INVOKABLE bool undo()
    {
        calls.append({QStringLiteral("undo")});
        return true;
    }
    Q_INVOKABLE void toggleEditMode()
    {
        m_editMode = !m_editMode;
        calls.append({QStringLiteral("toggleEditMode")});
        Q_EMIT editModeChanged();
    }
    Q_INVOKABLE void enterEditMode()
    {
        m_editMode = true;
        calls.append({QStringLiteral("enterEditMode")});
        Q_EMIT editModeChanged();
    }
    Q_INVOKABLE bool openCustomize()
    {
        calls.append({QStringLiteral("openCustomize")});
        return true;
    }
    Q_INVOKABLE bool openWallpaperSettings()
    {
        calls.append({QStringLiteral("openWallpaperSettings")});
        return true;
    }

    QList<QVariantList> calls;

Q_SIGNALS:
    void editModeChanged();

private:
    bool m_editMode = false;
};

QObject *itemAt(QObject *menu, int index)
{
    QQuickItem *item = nullptr;
    QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem *, item), Q_ARG(int, index));
    return item;
}

QObject *subMenuAt(QObject *menu, int index)
{
    QObject *item = itemAt(menu, index);
    return item != nullptr ? item->property("subMenu").value<QObject *>() : nullptr;
}

void trigger(QObject *item)
{
    QVERIFY(item != nullptr);
    QVERIFY(QMetaObject::invokeMethod(item, "triggered"));
}

} // namespace

// The desktop half of live customization (O9): the chord opens the
// customize menu (never the styled context menu), every entry is one facade
// call, and the Desktop icons submenu carries the typed desktop-icons rows.
class DesktopCustomizeMenuTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void chordOpensTheCustomizeMenuAndEntriesDispatch();
    void chordIsInertWithoutTheFacade();
    void contextMenuEditPanelsEntersEditMode();

private:
    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_previousHome;
    QByteArray m_previousDataHome;
};

// AGENT-GUARD: both rows right-click the middle of an 800x600 surface. They
// must run over an EMPTY redirected Desktop and data home: listing the real
// ~/Desktop put a user's icon under that point (24 entries fill four columns),
// so the click opened the icon menu and both rows failed on that machine, and
// the shared placement store would have read and migrated the real layout.
void DesktopCustomizeMenuTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
    m_previousHome = qgetenv("HOME");
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("HOME", m_home->path().toLocal8Bit());
    qputenv("XDG_DATA_HOME",
            (m_home->path() + QStringLiteral("/.local/share")).toLocal8Bit());
    QVERIFY(QDir().mkpath(m_home->path() + QStringLiteral("/Desktop")));
}

void DesktopCustomizeMenuTests::cleanup()
{
    qputenv("HOME", m_previousHome);
    qputenv("XDG_DATA_HOME", m_previousDataHome);
    m_home.reset();
}

void DesktopCustomizeMenuTests::chordOpensTheCustomizeMenuAndEntriesDispatch()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    StubCustomization customization;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, &launcher,
                         {{QStringLiteral("contextMenuStyle"), QStringLiteral("windows")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    host.window->setProperty("customizationAccess", QVariant::fromValue<QObject *>(&customization));

    auto *customize = host.child<QObject>(QStringLiteral("desktopCustomizeMenu"));
    auto *context = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(customize != nullptr && context != nullptr);

    host.clickWindow(Qt::RightButton, Qt::MetaModifier, QPointF(400, 300));
    QTRY_VERIFY(customize->property("opened").toBool());
    QVERIFY(!context->property("opened").toBool());

    // Entry order is the keyboard contract for the nested rows.
    const QStringList names{QStringLiteral("desktopCustomizeAddPanel"),
                            QStringLiteral("desktopCustomizeWallpaper"),
                            QStringLiteral("desktopCustomizeIcons"),
                            QStringLiteral("desktopCustomizeEditMode"),
                            QStringLiteral("desktopCustomizeUndo"),
                            QStringLiteral("desktopCustomizeOpenCustomize")};
    for (int index = 0; index < names.size(); ++index) {
        QObject *item = itemAt(customize, index);
        QVERIFY2(item != nullptr, qPrintable(QStringLiteral("entry %1").arg(index)));
        // A submenu's row is an auto-created item named after its submenu.
        auto *subMenu = item->property("subMenu").value<QObject *>();
        QCOMPARE(subMenu != nullptr ? subMenu->objectName() : item->objectName(), names.at(index));
    }
    QCOMPARE(customization.calls.size(), 1);
    QCOMPARE(customization.calls.first().first().toString(), QStringLiteral("appletSettingRows"));
    QCOMPARE(customization.calls.first().at(1).toString(), QStringLiteral("@desktop"));

    trigger(itemAt(subMenuAt(customize, 0), 0));
    QCOMPARE(customization.calls.last(), (QVariantList{QStringLiteral("addPanel"), QStringLiteral("top")}));
    trigger(itemAt(customize, 1));
    QCOMPARE(customization.calls.last().first().toString(), QStringLiteral("openWallpaperSettings"));
    trigger(itemAt(customize, 3));
    QCOMPARE(customization.calls.last().first().toString(), QStringLiteral("toggleEditMode"));
    QTRY_COMPARE(itemAt(customize, 3)->property("text").toString(), QStringLiteral("Exit edit mode"));
    trigger(itemAt(customize, 4));
    QCOMPARE(customization.calls.last().first().toString(), QStringLiteral("undo"));
    trigger(itemAt(customize, 5));
    QCOMPARE(customization.calls.last().first().toString(), QStringLiteral("openCustomize"));

    QObject *icons = subMenuAt(customize, 2);
    QVERIFY(icons != nullptr);
    QTRY_COMPARE(icons->property("count").toInt(), 2);
    QCOMPARE(itemAt(icons, 0)->objectName(), QStringLiteral("desktopCustomizeIcons:snapToGrid"));
    trigger(itemAt(icons, 0));
    QCOMPARE(customization.calls.last().mid(0, 4),
             (QVariantList{QStringLiteral("setAppletSetting"), QStringLiteral("@desktop"),
                           QStringLiteral("desktop-icons"), QStringLiteral("snapToGrid")}));
    QObject *placement = subMenuAt(icons, 1);
    QVERIFY(placement != nullptr);
    QTRY_COMPARE(placement->property("count").toInt(), 2);
    QCOMPARE(itemAt(placement, 1)->objectName(),
             QStringLiteral("desktopCustomizeIconsChoice:placement:right"));
    trigger(itemAt(placement, 1));
    QCOMPARE(customization.calls.last(),
             (QVariantList{QStringLiteral("setAppletSetting"), QStringLiteral("@desktop"),
                           QStringLiteral("desktop-icons"), QStringLiteral("placement"),
                           QStringLiteral("right")}));
    QMetaObject::invokeMethod(customize, "close");
    QTRY_VERIFY(!customize->property("opened").toBool());
}

void DesktopCustomizeMenuTests::chordIsInertWithoutTheFacade()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, &launcher,
                         {{QStringLiteral("contextMenuStyle"), QStringLiteral("windows")},
                          {QStringLiteral("applicationsMenuModifier"), QStringLiteral("none")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *customize = host.child<QObject>(QStringLiteral("desktopCustomizeMenu"));
    auto *context = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    host.clickWindow(Qt::RightButton, Qt::MetaModifier, QPointF(400, 300));
    QTRY_VERIFY(context->property("opened").toBool());
    QVERIFY(!customize->property("opened").toBool());
    QMetaObject::invokeMethod(context, "close");
    QTRY_VERIFY(!context->property("opened").toBool());
}

// ADR-0266: every context-menu style offers Edit Panels; it needs the facade
// and enters panel edit mode through it.
void DesktopCustomizeMenuTests::contextMenuEditPanelsEntersEditMode()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    StubCustomization customization;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, &launcher,
                         {{QStringLiteral("contextMenuStyle"), QStringLiteral("windows")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *context = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(context != nullptr);

    for (const char *style : {"windows", "mac", "traditional"}) {
        QVERIFY(host.window->setProperty(
            "applets", makeApplets({{QStringLiteral("contextMenuStyle"),
                                     QLatin1String(style)}})));
        bool offered = false;
        for (const QVariant &entry : context->property("entries").toList()) {
            const QVariantMap map = entry.toMap();
            if (map.value(QStringLiteral("objectName")).toString()
                == QLatin1String("desktopContextEditPanels")) {
                offered = map.value(QStringLiteral("kind")).toString()
                          == QLatin1String("editPanels");
            }
        }
        QVERIFY2(offered, style);
    }

    // Without the facade the entry is disabled; with it, it enters edit mode.
    // (Rows of the replaced styles are deleted later: let them go first.)
    const QString name = QStringLiteral("desktopContextEditPanels");
    QTRY_COMPARE(host.visualItemsNamed(name).size(), 1);
    QVERIFY(!host.visualItemsNamed(name).constFirst()->isEnabled());
    host.window->setProperty("customizationAccess",
                             QVariant::fromValue<QObject *>(&customization));
    QTRY_VERIFY(host.visualItemsNamed(name).constFirst()->isEnabled());
    const auto items = host.visualItemsNamed(name);
    QCOMPARE(items.size(), 1);
    QVERIFY(QMetaObject::invokeMethod(items.constFirst(), "triggered"));
    QCOMPARE(customization.calls.size(), 1);
    QCOMPARE(customization.calls.first().first().toString(), QStringLiteral("enterEditMode"));
    QVERIFY(customization.editMode());
}

QTEST_MAIN(DesktopCustomizeMenuTests)
#include "tst_desktop_customize_menu.moc"
