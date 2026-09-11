// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_input/keyboard_config_port.h>
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>
#include <qindaqt/apps/settings_input/pointer_device_port.h>
#include <qindaqt/apps/settings_input/shortcut_port.h>

#include <QObject>
#include <QVariantMap>

namespace QindaQt::Tests {

// In-process fake ports for QML page rows: scripted authority truth plus
// failure injection, no bus and no files involved.

class FakePointerPort final : public QindaQt::Apps::SettingsInput::PointerDevicePort {
public:
    QList<QindaQt::Apps::SettingsInput::PointerDeviceSnapshot> scripted;
    bool authorityPresent = true;
    QString nextWriteFailure;
    QList<QPair<QString, QString>> writtenProperties;

    QList<QindaQt::Apps::SettingsInput::PointerDeviceSnapshot>
    devices(QString *error) const override
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
        return scripted;
    }

    bool writeProperty(const QString &deviceId, const QString &property,
                       const QVariant &value, QString *error) const override
    {
        if (!nextWriteFailure.isEmpty()) {
            if (error != nullptr) {
                *error = nextWriteFailure;
            }
            return false;
        }
        m_written.append({deviceId, property});
        Q_UNUSED(value);
        return true;
    }

    QList<QPair<QString, QString>> &written() const { return m_written; }

private:
    mutable QList<QPair<QString, QString>> m_written;
};

class FakeKeyboardConfigPort final
    : public QindaQt::Apps::SettingsInput::KeyboardConfigPort {
public:
    QindaQt::Apps::SettingsInput::KeyboardConfig scripted;
    QindaQt::Apps::SettingsInput::StoreResult nextResult =
        QindaQt::Apps::SettingsInput::StoreResult::Stored;
    QString nextError;
    mutable int writes = 0;

    QindaQt::Apps::SettingsInput::KeyboardConfig
    read(QString *error) const override
    {
        if (error != nullptr) {
            error->clear();
        }
        return scripted;
    }

    QindaQt::Apps::SettingsInput::StoreResult
    write(const QindaQt::Apps::SettingsInput::KeyboardConfig &config,
          QString *error) const override
    {
        ++writes;
        if (nextResult == QindaQt::Apps::SettingsInput::StoreResult::Failed) {
            if (error != nullptr) {
                *error = nextError.isEmpty() ? QStringLiteral("refused")
                                             : nextError;
            }
            return QindaQt::Apps::SettingsInput::StoreResult::Failed;
        }
        stored = config;
        return nextResult;
    }

    mutable QindaQt::Apps::SettingsInput::KeyboardConfig stored;
};

class FakeLayoutPort final
    : public QindaQt::Apps::SettingsInput::KeyboardLayoutPort {
public:
    QList<QindaQt::Apps::SettingsInput::KeyboardLayoutSelection> scripted;
    QindaQt::Apps::SettingsInput::StoreResult nextResult =
        QindaQt::Apps::SettingsInput::StoreResult::Stored;
    QString nextError;
    mutable int writes = 0;

    QList<QindaQt::Apps::SettingsInput::KeyboardLayoutSelection>
    configuredLayouts(QString *error) const override
    {
        if (error != nullptr) {
            error->clear();
        }
        return scripted;
    }

    QindaQt::Apps::SettingsInput::StoreResult writeConfiguredLayouts(
        const QList<QindaQt::Apps::SettingsInput::KeyboardLayoutSelection>
            &layouts,
        QString *error) const override
    {
        ++writes;
        if (nextResult == QindaQt::Apps::SettingsInput::StoreResult::Failed) {
            if (error != nullptr) {
                *error = nextError.isEmpty() ? QStringLiteral("refused")
                                             : nextError;
            }
            return QindaQt::Apps::SettingsInput::StoreResult::Failed;
        }
        stored = layouts;
        return nextResult;
    }

    mutable QList<QindaQt::Apps::SettingsInput::KeyboardLayoutSelection>
        stored;
};

class FakeShortcutPort final
    : public QindaQt::Apps::SettingsInput::ShortcutPort {
public:
    QList<QindaQt::Apps::SettingsInput::ShortcutAction> scripted;
    bool authorityPresent = true;
    QString nextFailure;

    QList<QindaQt::Apps::SettingsInput::ShortcutAction>
    actions(QString *error) const override
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
        return scripted;
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
        m_assigned.append({actionUnique,
                           QindaQt::Apps::SettingsInput::shortcutKeysToInts(
                               keys)
                               .value(0, 0)});
        for (auto &action : m_scripted) {
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
        QindaQt::Apps::SettingsInput::ShortcutAction action;
        action.componentUnique = id;
        action.componentFriendly = id;
        action.actionUnique = id;
        action.actionFriendly = name;
        action.active = keys;
        action.command = command;
        m_scripted.append(action);
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
        for (int row = 0; row < m_scripted.size(); ++row) {
            if (m_scripted.at(row).componentUnique == componentUnique) {
                m_scripted.removeAt(row);
                return true;
            }
        }
        if (error != nullptr) {
            *error = QStringLiteral("unknown component");
        }
        return false;
    }

    QList<QPair<QString, int>> &assigned() const { return m_assigned; }
    QList<QindaQt::Apps::SettingsInput::ShortcutAction> &mutableScripted()
        const
    {
        return m_scripted;
    }

private:
    mutable QList<QindaQt::Apps::SettingsInput::ShortcutAction> m_scripted;
    mutable QList<QPair<QString, int>> m_assigned;
};

} // namespace QindaQt::Tests
