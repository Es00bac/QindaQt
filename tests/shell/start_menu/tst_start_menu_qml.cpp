// SPDX-License-Identifier: GPL-3.0-or-later
#include "start_menu_qml_test_support.h"

#include <QAccessible>
#include <QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Tests::StartMenu;

namespace {

// Minimal stand-ins for the borrowed shell facades. They publish exactly the
// surface StartMenuApplet reads — launcher: query/sections/launchGranted/
// activate(entryId, actionId); places: rows/open(id); session via
// systemMenu.sessionActions — and record every dispatch for assertions. The
// real contracts live in LauncherAppletController, PlacesController, and
// SystemMenuController; keeping the stubs one-property-per-contract means a
// facade surface change fails this test loudly instead of silently.
class StubPrograms final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)
    Q_PROPERTY(bool launchGranted READ launchGranted CONSTANT)

public:
    explicit StubPrograms(QVariantList sections, QObject *parent = nullptr)
        : QObject(parent), m_sections(std::move(sections))
    {
    }

    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &query)
    {
        if (m_query == query)
            return;
        m_query = query;
        Q_EMIT queryChanged();
    }
    [[nodiscard]] QVariantList sections() const { return m_sections; }
    [[nodiscard]] bool launchGranted() const { return true; }

    Q_INVOKABLE bool activate(const QString &entryId, const QString &actionId)
    {
        activated.append(entryId);
        lastActionId = actionId;
        return true;
    }

    QStringList activated;
    QString lastActionId;

Q_SIGNALS:
    void queryChanged();

private:
    QString m_query;
    QVariantList m_sections;
};

class StubSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canLogout READ canLogout CONSTANT)
    Q_PROPERTY(bool pending READ pending CONSTANT)

public:
    explicit StubSession(bool canLogout, QObject *parent = nullptr)
        : QObject(parent), m_canLogout(canLogout)
    {
    }

    [[nodiscard]] bool canLogout() const { return m_canLogout; }
    [[nodiscard]] bool pending() const { return false; }

    Q_INVOKABLE void requestLogout() { requests.append(QStringLiteral("logout")); }

    QStringList requests;

private:
    bool m_canLogout = false;
};

class StubSystemMenu final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *sessionActions READ sessionActions CONSTANT)
    Q_PROPERTY(bool sessionActionsAvailable READ sessionActionsAvailable CONSTANT)

public:
    explicit StubSystemMenu(StubSession *session, QObject *parent = nullptr)
        : QObject(parent), m_session(session)
    {
    }

    [[nodiscard]] QObject *sessionActions() const { return m_session; }
    [[nodiscard]] bool sessionActionsAvailable() const
    {
        return m_session != nullptr;
    }

private:
    StubSession *m_session = nullptr;
};

class StubPlaces final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit StubPlaces(QVariantList rows, QObject *parent = nullptr)
        : QObject(parent), m_rows(std::move(rows))
    {
    }

    [[nodiscard]] QVariantList rows() const { return m_rows; }
    [[nodiscard]] bool available() const { return true; }

    Q_INVOKABLE bool open(const QString &placeId)
    {
        opened.append(placeId);
        return true;
    }

    QStringList opened;

private:
    QVariantList m_rows;
};

class StubControls final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *places READ places CONSTANT)
    Q_PROPERTY(QObject *systemMenu READ systemMenu CONSTANT)

public:
    StubControls(StubPlaces *places, StubSystemMenu *systemMenu,
                 QObject *parent = nullptr)
        : QObject(parent), m_places(places), m_systemMenu(systemMenu)
    {
    }

    [[nodiscard]] QObject *places() const { return m_places; }
    [[nodiscard]] QObject *systemMenu() const { return m_systemMenu; }

private:
    StubPlaces *m_places = nullptr;
    StubSystemMenu *m_systemMenu = nullptr;
};

QVariantMap programRow(const QString &entryId, const QString &displayText,
                       const QString &iconName)
{
    return {{QStringLiteral("entryId"), entryId},
            {QStringLiteral("displayText"), displayText},
            {QStringLiteral("iconName"), iconName},
            {QStringLiteral("accessibleDescription"), displayText},
            {QStringLiteral("pinned"), false}};
}

QVariantMap placeRow(const QString &id, const QString &label,
                     const QString &iconName, int index)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("label"), label},
            {QStringLiteral("path"), QStringLiteral("/home/fixture")},
            {QStringLiteral("iconName"), iconName},
            {QStringLiteral("accessibleName"),
             QStringLiteral("%1, /home/fixture").arg(label)},
            {QStringLiteral("index"), index}};
}

} // namespace

class StartMenuQmlTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void disabledFallbackWithoutAccess();
    void popupTraversesAndActivatesThroughStubs();
    void logOffConfirmsThroughSessionFacade();
    void buttonWidthContainsItsLabel();
};

void StartMenuQmlTests::disabledFallbackWithoutAccess()
{
    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("StartMenuApplet"), nullptr, nullptr,
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    // Fail closed: identical visuals at half opacity, a present-but-disabled
    // button, and a popup that can never open.
    QCOMPARE(host.item->opacity(), 0.5);
    auto *button = host.child<QQuickItem>(QStringLiteral("startMenuButton"));
    QVERIFY(button != nullptr);
    QVERIFY(!button->isEnabled());
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(button);
    QVERIFY(interface != nullptr);
    QCOMPARE(interface->role(), QAccessible::Button);
    QCOMPARE(interface->text(QAccessible::Name),
             QStringLiteral("Start menu is unavailable"));

    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    auto *popup = host.child<QObject>(QStringLiteral("startMenuPopup"));
    QVERIFY(popup != nullptr);
    QVERIFY(!popup->property("opened").toBool());
    QVERIFY(host.visualItemsNamed(QStringLiteral("startMenuProgramsUnavailable"))
                .size() > 0);
    QVERIFY(host.visualItemsNamed(QStringLiteral("startMenuPlacesUnavailable"))
                .size() > 0);
}

void StartMenuQmlTests::popupTraversesAndActivatesThroughStubs()
{
    const QVariantList sections{
        QVariantMap{{QStringLiteral("identity"), QStringLiteral("pinned")},
                    {QStringLiteral("items"),
                     QVariantList{programRow(QStringLiteral("editor"),
                                             QStringLiteral("Text Editor"),
                                             QStringLiteral("applications-other")),
                                  programRow(QStringLiteral("terminal"),
                                             QStringLiteral("Terminal"),
                                             QStringLiteral("utilities-terminal"))}}},
        QVariantMap{{QStringLiteral("identity"), QStringLiteral("recent")},
                    {QStringLiteral("items"),
                     QVariantList{programRow(QStringLiteral("readme"),
                                             QStringLiteral("Read Me"),
                                             QString())}}}};
    const QVariantList places{placeRow(QStringLiteral("home"),
                                       QStringLiteral("Home"),
                                       QStringLiteral("user-home"), 0),
                              placeRow(QStringLiteral("documents"),
                                       QStringLiteral("Documents"),
                                       QStringLiteral("folder-documents"), 1)};
    StubPrograms launcher(sections);
    StubSession session(true);
    StubSystemMenu systemMenu(&session);
    StubPlaces placesFacade(places);
    StubControls controls(&placesFacade, &systemMenu);

    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("StartMenuApplet"), &launcher,
                         &controls, &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    auto *button = host.child<QQuickItem>(QStringLiteral("startMenuButton"));
    QVERIFY(button != nullptr);
    QVERIFY(button->isEnabled());
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(button);
    QVERIFY(interface != nullptr);
    QCOMPARE(interface->role(), QAccessible::Button);
    QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Start"));

    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QObject *popup = host.child<QObject>(QStringLiteral("startMenuPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    QCOMPARE(popup->property("popupType").toInt(), PopupTypeWindow);

    // Initial focus seeds the search field; both columns rendered from the
    // stub inventories.
    auto *search = host.child<QQuickItem>(QStringLiteral("startMenuSearchField"));
    QVERIFY(search != nullptr);
    QTRY_VERIFY(search->hasActiveFocus());
    QCOMPARE(host.visualItemsNamed(QStringLiteral("startMenuSectionHeader-pinned"))
                 .size(),
             1);
    QCOMPARE(host.visualItemsNamed(QStringLiteral("startMenuProgramRow-editor"))
                 .size(),
             1);
    QCOMPARE(host.visualItemsNamed(QStringLiteral("startMenuPlaceRow")).size(), 2);
    QCOMPARE(QAccessible::queryAccessibleInterface(
                 host.visualItemsNamed(QStringLiteral("startMenuProgramRow-editor"))
                     .constFirst())
                 ->text(QAccessible::Name),
             QStringLiteral("Text Editor"));

    // Flat Up/Down traversal from the search into the rows; activation
    // re-enters the launcher facade.
    keyClickFocused(host, Qt::Key_Down);
    auto *firstRow =
        host.visualItemsNamed(QStringLiteral("startMenuProgramRow-editor"))
            .constFirst();
    QTRY_VERIFY(firstRow->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Down);
    auto *secondRow =
        host.visualItemsNamed(QStringLiteral("startMenuProgramRow-terminal"))
            .constFirst();
    QTRY_VERIFY(secondRow->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QCOMPARE(launcher.activated, QStringList{QStringLiteral("terminal")});
    QCOMPARE(launcher.lastActionId, QString());
    QTRY_VERIFY(popup->property("opened").toBool());

    // Escape closes through the popup window.
    keyClickFocused(host, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("opened").toBool());

    // Places dispatch through the bounded seam and close the panel.
    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Return);
    QTRY_VERIFY(popup->property("opened").toBool());
    auto *placeRow = host.visualItemsNamed(QStringLiteral("startMenuPlaceRow"))
                         .constFirst();
    placeRow->forceActiveFocus(Qt::PopupFocusReason);
    QTRY_VERIFY(placeRow->hasActiveFocus());
    keyClickFocused(host, Qt::Key_Return);
    QCOMPARE(placesFacade.opened, QStringList{QStringLiteral("home")});
    QTRY_VERIFY(!popup->property("opened").toBool());
}

void StartMenuQmlTests::logOffConfirmsThroughSessionFacade()
{
    const QVariantList sections{
        QVariantMap{{QStringLiteral("identity"), QStringLiteral("pinned")},
                    {QStringLiteral("items"),
                     QVariantList{programRow(QStringLiteral("editor"),
                                             QStringLiteral("Text Editor"),
                                             QString())}}}};
    StubPrograms launcher(sections);
    StubPlaces placesFacade(QVariantList{});
    StubControls controls(&placesFacade, nullptr);

    // Without the system-menu facade the footer button renders disabled.
    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("StartMenuApplet"), &launcher,
                         &controls, &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *button = host.child<QQuickItem>(QStringLiteral("startMenuButton"));
    QVERIFY(button != nullptr);
    host.focus(button);
    QTest::keyClick(host.window.get(), Qt::Key_Space);
    QObject *popup = host.child<QObject>(QStringLiteral("startMenuPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    auto *logOff =
        host.visualItemsNamed(QStringLiteral("startMenuLogOffButton")).constFirst();
    QVERIFY(logOff != nullptr);
    QVERIFY(!logOff->isEnabled());
    keyClickFocused(host, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("opened").toBool());

    // With the facade present, destructive logout confirms first and never
    // dispatches on open (the system-menu session-actions rule).
    StubSession session(true);
    StubSystemMenu systemMenu(&session);
    StubControls controlsWithSession(&placesFacade, &systemMenu);
    AppletHost sessionHost;
    QVERIFY2(sessionHost.create(QStringLiteral("StartMenuApplet"), &launcher,
                                &controlsWithSession, &error),
             qPrintable(error));
    QTRY_VERIFY(sessionHost.window->isExposed());
    auto *sessionButton =
        sessionHost.child<QQuickItem>(QStringLiteral("startMenuButton"));
    QVERIFY(sessionButton != nullptr);
    sessionHost.focus(sessionButton);
    QTest::keyClick(sessionHost.window.get(), Qt::Key_Space);
    QObject *sessionPopup =
        sessionHost.child<QObject>(QStringLiteral("startMenuPopup"));
    QVERIFY(sessionPopup != nullptr);
    QTRY_VERIFY(sessionPopup->property("opened").toBool());
    auto *enabledLogOff =
        sessionHost.visualItemsNamed(QStringLiteral("startMenuLogOffButton"))
            .constFirst();
    QVERIFY(enabledLogOff->isEnabled());
    enabledLogOff->forceActiveFocus(Qt::PopupFocusReason);
    QTRY_VERIFY(enabledLogOff->hasActiveFocus());
    keyClickFocused(sessionHost, Qt::Key_Return);
    QTRY_VERIFY(!sessionPopup->property("opened").toBool());
    QObject *confirmation =
        sessionHost.child<QObject>(QStringLiteral("startMenuLogOffConfirmation"));
    QVERIFY(confirmation != nullptr);
    QTRY_VERIFY(confirmation->property("opened").toBool());
    QCOMPARE(session.requests.size(), 0);
    keyClickFocused(sessionHost, Qt::Key_Escape);
    QTRY_VERIFY(!confirmation->property("opened").toBool());
    QCOMPARE(session.requests.size(), 0);

    // Reopen and confirm this time; Return triggers the dialog's default
    // button, which is the only path that dispatches requestLogout.
    sessionHost.focus(sessionButton);
    QTest::keyClick(sessionHost.window.get(), Qt::Key_Space);
    QTRY_VERIFY(sessionPopup->property("opened").toBool());
    enabledLogOff = sessionHost.visualItemsNamed(QStringLiteral("startMenuLogOffButton"))
                        .constFirst();
    enabledLogOff->forceActiveFocus(Qt::PopupFocusReason);
    QTRY_VERIFY(enabledLogOff->hasActiveFocus());
    keyClickFocused(sessionHost, Qt::Key_Return);
    QTRY_VERIFY(!sessionPopup->property("opened").toBool());
    QTRY_VERIFY(confirmation->property("opened").toBool());
    // The default-button Return path is QQC2 behavior, not this module's
    // contract; the wiring under test is accepted() → requestLogout().
    QVERIFY(QMetaObject::invokeMethod(confirmation, "accept"));
    QCOMPARE(session.requests, QStringList{QStringLiteral("logout")});
    QTRY_VERIFY(!confirmation->property("opened").toBool());
}

// AGENT-GUARD (regression): the panel chip takes the applet's implicit width.
// A fixed 56 px let the bold italic label spill past the chip, where the next
// applet painted over it and the button read "star".
void StartMenuQmlTests::buttonWidthContainsItsLabel()
{
    AppletHost host;
    QString error;
    QVERIFY2(host.create(QStringLiteral("StartMenuApplet"), nullptr, nullptr,
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    auto *button = host.child<QQuickItem>(QStringLiteral("startMenuButton"));
    auto *content =
        host.child<QQuickItem>(QStringLiteral("startMenuButtonContent"));
    auto *label = host.child<QQuickItem>(QStringLiteral("startMenuButtonLabel"));
    QVERIFY(button != nullptr);
    QVERIFY(content != nullptr);
    QVERIFY(label != nullptr);
    QVERIFY(label->isVisible());
    const QFont font = label->property("font").value<QFont>();
    QVERIFY(font.bold());
    QVERIFY(font.italic());

    const qreal padding = button->property("leftPadding").toReal()
        + button->property("rightPadding").toReal();
    QVERIFY(host.item->implicitWidth() > 56.0);
    QVERIFY(host.item->width() + 0.5 >= content->implicitWidth() + padding);
    const QRectF labelBounds =
        label->mapRectToItem(host.item, QRectF(0, 0, label->width(), label->height()));
    QVERIFY2(labelBounds.left() >= 0.0
                 && labelBounds.right() <= host.item->width() + 0.5,
             qPrintable(QStringLiteral("label %1..%2 outside applet width %3")
                            .arg(labelBounds.left())
                            .arg(labelBounds.right())
                            .arg(host.item->width())));

    // Vertical panels show the icon alone inside the manifest extent.
    // The Row re-lays out on its next polish, so wait for the width too.
    QVERIFY(host.item->setProperty("vertical", true));
    QTRY_VERIFY(!label->isVisible());
    QTRY_COMPARE(host.item->implicitWidth(), 56.0);
}

QTEST_MAIN(StartMenuQmlTests)
#include "tst_start_menu_qml.moc"
