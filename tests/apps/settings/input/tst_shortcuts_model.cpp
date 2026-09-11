// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/shortcuts_model.h>

#include <QTest>

using QindaQt::Apps::SettingsInput::ShortcutAction;
using QindaQt::Apps::SettingsInput::ShortcutPort;
using QindaQt::Apps::SettingsInput::ShortcutsModel;

namespace {

class FakeShortcutPort final : public ShortcutPort
{
public:
    bool authorityPresent = true;
    QString nextFailure;
    QList<QPair<QString, QList<int>>> assignments;

    QList<ShortcutAction> actions(QString *error) const override
    {
        if (!authorityPresent) {
            if (error != nullptr) {
                *error = QStringLiteral("authority absent");
            }
            return {};
        }
        if (error != nullptr) {
            error->clear();
        }
        return m_scripted;
    }

    bool setShortcuts(const QString &componentUnique,
                      const QString &actionUnique,
                      const QList<QKeySequence> &keys,
                      QString *error) const override
    {
        if (!nextFailure.isEmpty()) {
            if (error != nullptr) {
                *error = nextFailure;
            }
            return false;
        }
        mutableAssignments().append({componentUnique,
                                     QindaQt::Apps::SettingsInput::
                                         shortcutKeysToInts(keys)});
        for (ShortcutAction &action : mutableScripted()) {
            if (action.componentUnique == componentUnique &&
                action.actionUnique == actionUnique) {
                action.active = keys;
            }
        }
        return true;
    }

    bool addCommandShortcut(const QString &name, const QString &command,
                            const QList<QKeySequence> &keys,
                            QString *componentUnique,
                            QString *error) const override
    {
        if (!nextFailure.isEmpty()) {
            if (error != nullptr) {
                *error = nextFailure;
            }
            return false;
        }
        const QString id = QStringLiteral("qindaqt-custom-%1.desktop")
                               .arg(name.toLower());
        ShortcutAction action;
        action.componentUnique = id;
        action.componentFriendly = id;
        action.actionUnique = id;
        action.actionFriendly = name;
        action.active = keys;
        action.command = command;
        mutableScripted().append(action);
        if (componentUnique != nullptr) {
            *componentUnique = id;
        }
        return true;
    }

    bool removeCommandShortcut(const QString &componentUnique,
                               QString *error) const override
    {
        if (!nextFailure.isEmpty()) {
            if (error != nullptr) {
                *error = nextFailure;
            }
            return false;
        }
        auto &rows = mutableScripted();
        for (int row = 0; row < rows.size(); ++row) {
            if (rows.at(row).componentUnique == componentUnique) {
                rows.removeAt(row);
                return true;
            }
        }
        if (error != nullptr) {
            *error = QStringLiteral("unknown component");
        }
        return false;
    }

    QList<ShortcutAction> &mutableScripted() const { return m_scripted; }
    QList<QPair<QString, QList<int>>> &mutableAssignments() const
    {
        return m_assignments;
    }

private:
    mutable QList<ShortcutAction> m_scripted;
    mutable QList<QPair<QString, QList<int>>> m_assignments;
};

ShortcutAction shellAction(const QString &actionUnique,
                           const QString &actionFriendly,
                           const QList<QKeySequence> &active)
{
    ShortcutAction action;
    action.componentUnique = QStringLiteral("qindaqt-shell");
    action.componentFriendly = QStringLiteral("QindaQt Shell");
    action.actionUnique = actionUnique;
    action.actionFriendly = actionFriendly;
    action.active = active;
    action.defaults = {QKeySequence(Qt::META | Qt::Key_Space)};
    return action;
}

} // namespace

class ShortcutsModelTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void authorityLossIsDegraded();
    void searchFiltersAcrossComponentAndActionNames();
    void conflictDetectionNamesTheConflictingAction();
    void assignDelegatesEncodedKeys();
    void resetAndClearDelegateDefaultsAndEmpty();
    void failedAssignmentReportsAndDoesNotRefreshIntoLoss();
    void commandLifecycleAddsAndRemoves();

private:
    FakeShortcutPort m_port;
};

void ShortcutsModelTest::authorityLossIsDegraded()
{
    FakeShortcutPort port;
    port.authorityPresent = false;
    ShortcutsModel model(port);
    model.refresh();
    QVERIFY(!model.available());
    QCOMPARE(model.count(), 0);
}

void ShortcutsModelTest::searchFiltersAcrossComponentAndActionNames()
{
    FakeShortcutPort port;
    port.mutableScripted().append(
        shellAction(QStringLiteral("qindaqt_reveal_panels"),
                    QStringLiteral("Reveal QindaQt panels"), {}));
    ShortcutAction lock;
    lock.componentUnique = QStringLiteral("ksmserver");
    lock.componentFriendly = QStringLiteral("Session Management");
    lock.actionUnique = QStringLiteral("Lock Session");
    lock.actionFriendly = QStringLiteral("Lock Session");
    port.mutableScripted().append(lock);
    ShortcutsModel model(port);
    model.refresh();
    QCOMPARE(model.count(), 2);

    model.setFilter(QStringLiteral("lock"));
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.index(0, 0).data(ShortcutsModel::ActionUniqueRole),
             QStringLiteral("Lock Session"));

    model.setFilter(QStringLiteral("qindaqt"));
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.index(0, 0).data(ShortcutsModel::ComponentNameRole),
             QStringLiteral("QindaQt Shell"));

    model.setFilter(QStringLiteral("nothing-matches"));
    QCOMPARE(model.count(), 0);
    model.setFilter(QString());
    QCOMPARE(model.count(), 2);
}

void ShortcutsModelTest::conflictDetectionNamesTheConflictingAction()
{
    FakeShortcutPort port;
    port.mutableScripted().append(
        shellAction(QStringLiteral("qindaqt_reveal_panels"),
                    QStringLiteral("Reveal QindaQt panels"),
                    {QKeySequence(Qt::META | Qt::Key_Space)}));
    ShortcutsModel model(port);
    model.refresh();

    const int metaSpace = int(Qt::META) | int(Qt::Key_Space);
    const QStringList conflicts =
        model.conflictsFor({QVariant(metaSpace)});
    QCOMPARE(conflicts.size(), 1);
    QVERIFY(conflicts.first().contains(QStringLiteral("QindaQt Shell")));
    QVERIFY(conflicts.first().contains(QStringLiteral("Reveal QindaQt panels")));
    // A free key has no conflicts.
    QVERIFY(model.conflictsFor({QVariant(int(Qt::META) | int(Qt::Key_P))})
                .isEmpty());
    // Zero and garbage are ignored, never reported as conflicts.
    QVERIFY(model.conflictsFor({QVariant(0)}).isEmpty());
}

void ShortcutsModelTest::assignDelegatesEncodedKeys()
{
    FakeShortcutPort port;
    port.mutableScripted().append(
        shellAction(QStringLiteral("qindaqt_reveal_panels"),
                    QStringLiteral("Reveal QindaQt panels"), {}));
    ShortcutsModel model(port);
    model.refresh();

    const int metaPlus = int(Qt::META) | int(Qt::Key_Plus);
    QVERIFY(model.assign(0, {QVariant(metaPlus)}));
    QCOMPARE(port.mutableAssignments().size(), 1);
    QCOMPARE(port.mutableAssignments().first().second,
             QList<int>{metaPlus});
    // The refreshed listing shows the new truth.
    QCOMPARE(model.index(0, 0).data(ShortcutsModel::KeysRole),
             QKeySequence(Qt::META | Qt::Key_Plus)
                 .toString(QKeySequence::NativeText));
}

void ShortcutsModelTest::resetAndClearDelegateDefaultsAndEmpty()
{
    FakeShortcutPort port;
    port.mutableScripted().append(
        shellAction(QStringLiteral("qindaqt_reveal_panels"),
                    QStringLiteral("Reveal QindaQt panels"),
                    {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R)}));
    ShortcutsModel model(port);
    model.refresh();

    QVERIFY(model.resetToDefault(0));
    QCOMPARE(port.mutableAssignments().last().second,
             QList<int>{int(Qt::META) | int(Qt::Key_Space)});

    QVERIFY(model.clear(0));
    QCOMPARE(port.mutableAssignments().last().second, QList<int>{});

    // Out-of-range rows are rejected, not dispatched.
    QVERIFY(!model.assign(7, {}));
    QVERIFY(!model.resetToDefault(-1));
    QCOMPARE(port.mutableAssignments().size(), 2);
}

void ShortcutsModelTest::failedAssignmentReportsAndDoesNotRefreshIntoLoss()
{
    FakeShortcutPort port;
    port.mutableScripted().append(
        shellAction(QStringLiteral("qindaqt_reveal_panels"),
                    QStringLiteral("Reveal QindaQt panels"), {}));
    ShortcutsModel model(port);
    model.refresh();
    port.nextFailure = QStringLiteral("daemon refused");
    QVERIFY(!model.assign(0, {QVariant(int(Qt::META) | int(Qt::Key_Space))}));
    QVERIFY(model.statusText().contains(QStringLiteral("refused")));
    QVERIFY(model.available());
}

void ShortcutsModelTest::commandLifecycleAddsAndRemoves()
{
    FakeShortcutPort port;
    ShortcutsModel model(port);
    model.refresh();
    QVERIFY(model.addCommand(QStringLiteral("Recorder"),
                             QStringLiteral("/usr/bin/record"),
                             {QVariant(int(Qt::META) | int(Qt::Key_R))}));
    QCOMPARE(model.count(), 1);
    QVERIFY(model.index(0, 0).data(ShortcutsModel::IsCommandRole).toBool());
    QCOMPARE(model.index(0, 0).data(ShortcutsModel::CommandRole),
             QStringLiteral("/usr/bin/record"));
    QVERIFY(model.removeCommand(0));
    QCOMPARE(model.count(), 0);
}

QTEST_MAIN(ShortcutsModelTest)
#include "tst_shortcuts_model.moc"
