// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/session/idle_policy/source_preferences.h>
namespace QindaQt::Session::IdlePolicy {
struct SharedIdlePreferences final {
    QString lineage;
    bool dimEnabled = false;
    int dimSeconds = 0;
    DisplayOffPreferences display;
    bool lockBeforeDisplayOff = false;
    QString suspendAction;
    int suspendSeconds = 0;
    friend bool operator==(const SharedIdlePreferences &, const SharedIdlePreferences &) = default;
};
QStringList sharedIdleSettingsKeys();
std::optional<SharedIdlePreferences> sharedIdlePreferencesFor(
    const Services::SettingsClient::SettingsSnapshot &snapshot,
    const QString &expectedOwner, PowerSourceProfile source);
}
