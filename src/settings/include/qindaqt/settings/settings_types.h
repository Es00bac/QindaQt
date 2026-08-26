// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace QindaQt::Settings {

enum class SettingLayer {
    SystemDefaults = 0,
    ProfileDefaults = 1,
    UserOverrides = 2,
    SessionOverrides = 3,
};

enum class SettingDomain {
    Appearance,
    Fonts,
    Displays,
    Input,
    Panels,
    WindowManagement,
    Accessibility,
    Services,
};

enum class SettingValueType {
    Boolean,
    Integer,
    Number,
    String,
    StringList,
    Object,
};

[[nodiscard]] QString toString(SettingLayer layer);
[[nodiscard]] QString toString(SettingDomain domain);
[[nodiscard]] QString toString(SettingValueType type);
[[nodiscard]] QString domainKeyPrefix(SettingDomain domain);

[[nodiscard]] bool parseSettingLayer(const QString &text, SettingLayer *layer);
[[nodiscard]] bool parseSettingDomain(const QString &text, SettingDomain *domain);
[[nodiscard]] bool parseSettingValueType(const QString &text, SettingValueType *type);

[[nodiscard]] bool isMutableLayer(SettingLayer layer) noexcept;
[[nodiscard]] bool isPersistentLayer(SettingLayer layer) noexcept;

struct ValidationIssue final {
    QString key;
    QString code;
    QString message;
};

class ValidationResult final {
public:
    [[nodiscard]] bool isValid() const noexcept { return m_issues.isEmpty(); }
    [[nodiscard]] const QVector<ValidationIssue> &issues() const noexcept { return m_issues; }
    [[nodiscard]] QString summary() const;

    void add(QString key, QString code, QString message);
    void append(const ValidationResult &other);

private:
    QVector<ValidationIssue> m_issues;
};

struct EffectiveSettingChange final {
    QString key;
    SettingDomain domain = SettingDomain::Appearance;
    QVariant previousValue;
    QVariant currentValue;
    SettingLayer previousSource = SettingLayer::SystemDefaults;
    SettingLayer currentSource = SettingLayer::SystemDefaults;
};

struct SettingsChangeSet final {
    quint64 revisionBefore = 0;
    quint64 revisionAfter = 0;
    SettingLayer editedLayer = SettingLayer::UserOverrides;
    QStringList touchedKeys;
    QVector<EffectiveSettingChange> effectiveChanges;

    [[nodiscard]] bool isEmpty() const noexcept { return touchedKeys.isEmpty(); }
};

enum class CommitStatus {
    Applied,
    ValidationFailed,
    Conflict,
    ReadOnlyLayer,
};

struct CommitResult final {
    CommitStatus status = CommitStatus::ValidationFailed;
    ValidationResult validation;
    SettingsChangeSet changes;
    QString message;

    [[nodiscard]] bool ok() const noexcept { return status == CommitStatus::Applied; }
};

} // namespace QindaQt::Settings
