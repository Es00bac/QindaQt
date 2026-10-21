// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QList>
namespace QindaQt::InputActions {
inline constexpr auto Service = "org.qindaqt.InputActions1";
inline constexpr auto Object = "/org/qindaqt/InputActions1";
inline constexpr auto Interface = "org.qindaqt.InputActions1";
inline constexpr int MaxTextBytes = 4096;
inline constexpr int MaxMacros = 64;
inline constexpr int MaxSteps = 64;
inline constexpr qsizetype MaxLibraryBytes = 1024 * 1024;
struct Binding {
    QString action = QStringLiteral("none");
    QString shortcut; // Legacy single-key chord; retained for stored bindings.
    QStringList modifiers, keys, mouseButtons;
    QString text, page, holdPage, macro;
    friend bool operator==(const Binding &, const Binding &) = default;
};

struct MacroStep {
    QString kind = QStringLiteral("tap");
    Binding binding;
    int slot = 0, milliseconds = 40;
    double x = 0, y = 0;
    friend bool operator==(const MacroStep &, const MacroStep &) = default;
};
struct Macro {
    QString id, name, shortcut;
    QStringList mouseModifiers;
    QString mouseButton;
    QList<MacroStep> steps;
    friend bool operator==(const Macro &, const Macro &) = default;
};
QStringList actionIds();
Binding asCombination(const Binding &binding);
QJsonObject bindingJson(const Binding &binding);
bool readBinding(const QJsonObject &value, Binding &binding, QString &reason);
QJsonObject macroJson(const Macro &macro);
bool readMacro(const QJsonObject &value, Macro &macro, QString &reason);
QJsonObject stepJson(const MacroStep &step);
bool readStep(const QJsonObject &value, MacroStep &step, QString &reason);
} // namespace QindaQt::InputActions
