// SPDX-License-Identifier: GPL-3.0-or-later

#include "launch_execution.h"

#include <QtTest>

using namespace QindaQt::Shell::Launcher;

namespace {

QString document(const QString &body, const QString &actions = {})
{
    return QStringLiteral("[Desktop Entry]\nType=Application\nName=Fixture\n") + body
        + actions;
}

ExecExpansionValues values()
{
    return { QStringLiteral("Fixture App"), QStringLiteral("fixture-icon"),
             QStringLiteral("/data/applications/fixture.desktop") };
}

} // namespace

class LaunchExecutionTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesPrimaryAndActionExecutionKeys();
    void rejectsHostileExecutionGroups();
    void expandsFieldCodesWithoutAShell();
    void rejectsUnsatisfiableFieldCodes();
    void enforcesOutputCeilings();
};

void LaunchExecutionTests::parsesPrimaryAndActionExecutionKeys()
{
    const auto primary = LaunchExecutionParser::parse(document(
        QStringLiteral("Exec=fixture --open %U\nPath=/srv/data\nDBusActivatable=false\n")));
    QVERIFY2(primary.ok(), qPrintable(primary.message));
    QCOMPARE(primary.keys->exec, QStringLiteral("fixture --open %U"));
    QCOMPARE(primary.keys->path, QStringLiteral("/srv/data"));
    QVERIFY(!primary.keys->terminal);
    QVERIFY(!primary.keys->dbusActivatable);

    const QString text = document(
        QStringLiteral("Exec=fixture\nPath=/entry/path\n"
                       "Terminal=true\nDBusActivatable=true\n"),
        QStringLiteral("Actions=new-window;\n\n[Desktop Action new-window]\n"
                       "Name=New Window\nExec=fixture --new-window\n"
                       "Terminal=false\nPath=/action/path\n"
                       "DBusActivatable=false\n"));
    const auto action = LaunchExecutionParser::parse(text, QStringLiteral("new-window"));
    QVERIFY2(action.ok(), qPrintable(action.message));
    QCOMPARE(action.keys->exec, QStringLiteral("fixture --new-window"));
    // The action parser contributes only Exec. Unsupported action-local policy
    // keys cannot replace entry-level dispatch or working-directory truth.
    QVERIFY(action.keys->path.isEmpty());
    QVERIFY(!action.keys->terminal);
    QVERIFY(!action.keys->dbusActivatable);

    const auto entry = LaunchExecutionParser::parse(text);
    QVERIFY(entry.ok());
    QCOMPARE(entry.keys->path, QStringLiteral("/entry/path"));
    QVERIFY(entry.keys->terminal);
    QVERIFY(entry.keys->dbusActivatable);

    const auto missing = LaunchExecutionParser::parse(text, QStringLiteral("no-such"));
    QCOMPARE(missing.error, ExecutionParseError::GroupNotFound);

    // A D-Bus-activatable entry without Exec is legal.
    const auto activatable = LaunchExecutionParser::parse(document(
        QStringLiteral("DBusActivatable=true\n")));
    QVERIFY2(activatable.ok(), qPrintable(activatable.message));

    const auto bare = LaunchExecutionParser::parse(document(QString()));
    QCOMPARE(bare.error, ExecutionParseError::MissingExec);
}

void LaunchExecutionTests::rejectsHostileExecutionGroups()
{
    const auto duplicate = LaunchExecutionParser::parse(document(
        QStringLiteral("Exec=one\nExec=two\n")));
    QCOMPARE(duplicate.error, ExecutionParseError::DuplicateKey);

    const auto badBoolean = LaunchExecutionParser::parse(document(
        QStringLiteral("Exec=one\nTerminal=yes\n")));
    QCOMPARE(badBoolean.error, ExecutionParseError::InvalidBoolean);

    const auto badEscape = LaunchExecutionParser::parse(document(
        QStringLiteral("Exec=one\\qtwo\n")));
    QCOMPARE(badEscape.error, ExecutionParseError::InvalidEscape);

    QString oversized = QStringLiteral("Exec=");
    oversized += QString(5000, QLatin1Char('a'));
    const auto tooLarge = LaunchExecutionParser::parse(document(oversized + QLatin1Char('\n')));
    QCOMPARE(tooLarge.error, ExecutionParseError::ExecTooLarge);

    // Locale variants never feed execution; an entry with only a localized
    // Exec has no command.
    const auto localized = LaunchExecutionParser::parse(document(
        QStringLiteral("Exec[de]=fixture --german\nDBusActivatable=false\n")));
    QCOMPARE(localized.error, ExecutionParseError::MissingExec);

    // Hostile policy lookalikes inside an action group are opaque. Invalid
    // values there must neither poison action Exec nor influence dispatch.
    const auto hostileAction = LaunchExecutionParser::parse(document(
        QStringLiteral("Exec=fixture\n"),
        QStringLiteral("Actions=hostile;\n[Desktop Action hostile]\n"
                       "Name=Hostile\nExec=fixture --action\n"
                       "Terminal=maybe\nPath=bad\\qpath\n"
                       "DBusActivatable=maybe\n")),
        QStringLiteral("hostile"));
    QVERIFY2(hostileAction.ok(), qPrintable(hostileAction.message));
    QCOMPARE(hostileAction.keys->exec, QStringLiteral("fixture --action"));
    QVERIFY(hostileAction.keys->path.isEmpty());
}

void LaunchExecutionTests::expandsFieldCodesWithoutAShell()
{
    const auto simple = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture --open"), values());
    QVERIFY(simple.ok());
    QCOMPARE(simple.plan->program, QStringLiteral("fixture"));
    QCOMPARE(simple.plan->arguments, QStringList({ QStringLiteral("--open") }));

    // Quoting groups arguments; escapes decode inside quotes.
    const auto quoted = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture \"two words\" \\\"escaped\\\" \"a\\\"b\""), values());
    QVERIFY2(quoted.ok(), qPrintable(quoted.message));
    QCOMPARE(quoted.plan->arguments,
             QStringList({ QStringLiteral("two words"), QStringLiteral("\"escaped\""),
                           QStringLiteral("a\"b") }));

    // File/URL codes are dropped as whole tokens: the launcher passes none.
    const auto withFiles = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture %u %F --flag"), values());
    QVERIFY2(withFiles.ok(), qPrintable(withFiles.message));
    QCOMPARE(withFiles.plan->arguments, QStringList({ QStringLiteral("--flag") }));

    // %c and %k expand in place; %% is a literal percent.
    const auto display = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture --name=%c --file=%k 100%%"), values());
    QVERIFY2(display.ok(), qPrintable(display.message));
    QCOMPARE(display.plan->arguments,
             QStringList({ QStringLiteral("--name=Fixture App"),
                           QStringLiteral("--file=/data/applications/fixture.desktop"),
                           QStringLiteral("100%") }));

    // %i becomes the two-argument --icon form only when an Icon exists.
    const auto icon = ExecFieldCodeExpander::expand(QStringLiteral("fixture %i"), values());
    QVERIFY(icon.ok());
    QCOMPARE(icon.plan->arguments,
             QStringList({ QStringLiteral("--icon"), QStringLiteral("fixture-icon") }));
    ExecExpansionValues noIcon = values();
    noIcon.iconName.clear();
    const auto noIconPlan = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture %i --go"), noIcon);
    QVERIFY(noIconPlan.ok());
    QCOMPARE(noIconPlan.plan->arguments, QStringList({ QStringLiteral("--go") }));

    // Deprecated codes drop as whole tokens.
    const auto deprecated = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture %d %v --ok"), values());
    QVERIFY2(deprecated.ok(), qPrintable(deprecated.message));
    QCOMPARE(deprecated.plan->arguments, QStringList({ QStringLiteral("--ok") }));

    // AGENT-GUARD: nothing here ever invokes a shell. A command line meant to
    // exploit shell interpolation stays literal argv text.
    const auto injection = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture ; rm -rf / | cat `id` $(id)"), values());
    QVERIFY2(injection.ok(), qPrintable(injection.message));
    QCOMPARE(injection.plan->arguments,
             QStringList({ QStringLiteral(";"), QStringLiteral("rm"),
                           QStringLiteral("-rf"), QStringLiteral("/"),
                           QStringLiteral("|"), QStringLiteral("cat"),
                           QStringLiteral("`id`"), QStringLiteral("$(id)") }));
}

void LaunchExecutionTests::rejectsUnsatisfiableFieldCodes()
{
    const auto unknown = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture %z"), values());
    QCOMPARE(unknown.error, ExecPlanError::UnsupportedFieldCode);

    // A list code embedded in a larger token would expand to a variable
    // argument count at an attacker-chosen position.
    const auto embeddedList = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture --uri=%U"), values());
    QCOMPARE(embeddedList.error, ExecPlanError::UnsupportedFieldCode);

    const auto embeddedIcon = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture x%i"), values());
    QCOMPARE(embeddedIcon.error, ExecPlanError::UnsupportedFieldCode);

    const auto dangling = ExecFieldCodeExpander::expand(QStringLiteral("fixture %"), values());
    QCOMPARE(dangling.error, ExecPlanError::UnsupportedFieldCode);

    const auto unterminated = ExecFieldCodeExpander::expand(
        QStringLiteral("fixture \"open"), values());
    QCOMPARE(unterminated.error, ExecPlanError::UnterminatedQuote);

    const auto empty = ExecFieldCodeExpander::expand(QStringLiteral("   "), values());
    QCOMPARE(empty.error, ExecPlanError::MissingProgram);

    // An Exec of only dropped codes has no program.
    const auto onlyCodes = ExecFieldCodeExpander::expand(QStringLiteral("%u %f"), values());
    QCOMPARE(onlyCodes.error, ExecPlanError::MissingProgram);
}

void LaunchExecutionTests::enforcesOutputCeilings()
{
    QString oversized = QString(5000, QLatin1Char('a'));
    const auto tooLarge = ExecFieldCodeExpander::expand(oversized, values());
    QCOMPARE(tooLarge.error, ExecPlanError::ExecTooLarge);

    // %c inside a repeated token cannot grow the output without bound.
    QString repeated = QStringLiteral("fixture");
    for (int index = 0; index < 70; ++index)
        repeated += QStringLiteral(" %c%c%c%c%c%c%c%c%c%c");
    const auto grown = ExecFieldCodeExpander::expand(repeated, values());
    QVERIFY(!grown.ok());
}

QTEST_GUILESS_MAIN(LaunchExecutionTests)
#include "tst_launch_execution.moc"
