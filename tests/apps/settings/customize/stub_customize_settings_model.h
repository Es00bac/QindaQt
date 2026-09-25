// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <utility>

namespace QindaQt::Apps::SettingsCustomize::TestSupport {

// Stand-in for CustomizeSettingsModel with the same QML-facing shape: four
// presets covering every kind (the active built-in, an edited built-in, the
// default, and one of the user's own), recorded actions, and settable state.
// The Settings Center page harnesses share it, so every property the page
// reads must exist here with its real type (they run warning-fatal).
class StubCustomizeSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ falseValue CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool unavailable READ unavailable NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ falseValue CONSTANT)
    Q_PROPERTY(bool canSwitch READ canSwitch NOTIFY stateChanged)
    Q_PROPERTY(bool canManage READ canManage NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY stateChanged)
    Q_PROPERTY(QString activePresetId READ activePresetId NOTIFY presetsChanged)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(QString defaultPresetId READ defaultPresetId CONSTANT)
    Q_PROPERTY(int maximumNameLength READ maximumNameLength CONSTANT)
    Q_PROPERTY(bool panelHideDelayAvailable READ panelHideDelayAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool panelHideDelayEditable READ panelHideDelayEditable NOTIFY stateChanged)
    Q_PROPERTY(bool panelHideDelayPending READ panelHideDelayPending NOTIFY stateChanged)
    Q_PROPERTY(int panelHideDelayMs READ panelHideDelayMs NOTIFY stateChanged)
    Q_PROPERTY(QString panelHideDelayStatus READ panelHideDelayStatus NOTIFY stateChanged)

public:
    explicit StubCustomizeSettingsModel(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    [[nodiscard]] bool falseValue() const { return false; }
    [[nodiscard]] bool ready() const { return !m_unavailable; }
    [[nodiscard]] bool unavailable() const { return m_unavailable; }
    [[nodiscard]] bool busy() const { return m_busy; }
    [[nodiscard]] bool canSwitch() const { return !m_unavailable && !m_busy; }
    [[nodiscard]] bool canManage() const { return !m_unavailable && !m_busy; }
    [[nodiscard]] QString statusText() const
    {
        return m_unavailable ? QStringLiteral("Settings1 transport is unavailable")
                             : QStringLiteral("Current layout: Fixture");
    }
    [[nodiscard]] QString errorText() const { return m_error; }
    [[nodiscard]] QString noticeText() const { return m_notice; }
    [[nodiscard]] QString activePresetId() const { return m_activeId; }
    [[nodiscard]] QString defaultPresetId() const { return QStringLiteral("macos-inspired"); }
    [[nodiscard]] int maximumNameLength() const { return 64; }
    [[nodiscard]] bool panelHideDelayAvailable() const { return m_delayAvailable; }
    [[nodiscard]] bool panelHideDelayEditable() const { return m_delayEditable; }
    [[nodiscard]] bool panelHideDelayPending() const { return m_delayPending; }
    [[nodiscard]] int panelHideDelayMs() const { return m_delayMs; }
    [[nodiscard]] QString panelHideDelayStatus() const { return m_delayStatus; }

    [[nodiscard]] QVariantList presets() const
    {
        // The same panel summary shape the production projection publishes.
        const QVariantList panels{QVariantMap{
            {QStringLiteral("id"), QStringLiteral("bar")},
            {QStringLiteral("edge"), QStringLiteral("top")},
            {QStringLiteral("alignment"), QStringLiteral("fill")},
            {QStringLiteral("layer"), QStringLiteral("above")},
            {QStringLiteral("hideMode"), QStringLiteral("never")},
            {QStringLiteral("thickness"), 32},
            {QStringLiteral("length"), 1.0},
            {QStringLiteral("appletCount"), 1},
        }};
        const auto entry = [&](const QString &id, const QString &name, bool builtIn,
                               bool modified) {
            return QVariantMap{
                {QStringLiteral("id"), id},
                {QStringLiteral("name"), name},
                {QStringLiteral("description"), QStringLiteral("Test layout")},
                {QStringLiteral("panels"), panels},
                {QStringLiteral("builtIn"), builtIn},
                {QStringLiteral("own"), !builtIn},
                {QStringLiteral("modified"), modified},
                {QStringLiteral("active"), id == m_activeId},
                {QStringLiteral("isDefault"), id == QLatin1String("macos-inspired")},
            };
        };
        QVariantList result{
            entry(QStringLiteral("fixture"), QStringLiteral("Fixture"), true, false),
            entry(QStringLiteral("alternate"), QStringLiteral("Alternate"), true, true),
            entry(QStringLiteral("macos-inspired"), QStringLiteral("Mac"), true, false),
        };
        if (m_ownPreset) {
            result.append(entry(QStringLiteral("user-mine"), QStringLiteral("Mine"), false,
                                false));
        }
        return result;
    }

    Q_INVOKABLE bool activatePreset(const QString &presetId)
    {
        calls.append({QStringLiteral("activatePreset"), presetId});
        return true;
    }
    Q_INVOKABLE QString presetNameError(const QString &name,
                                        const QString &renamingId = QString()) const
    {
        Q_UNUSED(renamingId);
        if (name.trimmed().isEmpty()) {
            return QStringLiteral("Enter a name for the preset.");
        }
        if (name.trimmed().compare(QStringLiteral("Fixture"), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("A preset named “Fixture” already exists.");
        }
        return {};
    }
    Q_INVOKABLE bool savePresetAs(const QString &sourceId, const QString &name)
    {
        calls.append({QStringLiteral("savePresetAs"), sourceId, name});
        return admitStoreActions;
    }
    Q_INVOKABLE bool duplicatePreset(const QString &presetId)
    {
        calls.append({QStringLiteral("duplicatePreset"), presetId});
        return admitStoreActions;
    }
    Q_INVOKABLE bool renamePreset(const QString &presetId, const QString &name)
    {
        calls.append({QStringLiteral("renamePreset"), presetId, name});
        return admitStoreActions;
    }
    Q_INVOKABLE bool deletePreset(const QString &presetId)
    {
        calls.append({QStringLiteral("deletePreset"), presetId});
        return admitStoreActions;
    }
    Q_INVOKABLE bool restorePreset(const QString &presetId)
    {
        calls.append({QStringLiteral("restorePreset"), presetId});
        return admitStoreActions;
    }
    Q_INVOKABLE bool setPanelHideDelayMs(int milliseconds)
    {
        ++delaySetCount;
        lastDelayMs = milliseconds;
        return admitDelay;
    }
    Q_INVOKABLE void retry() { calls.append({QStringLiteral("retry")}); }

    void setDelayState(bool available, bool editable, bool pending, int value,
                       QString status)
    {
        m_delayAvailable = available;
        m_delayEditable = editable;
        m_delayPending = pending;
        m_delayMs = value;
        m_delayStatus = std::move(status);
        Q_EMIT stateChanged();
    }
    void setUnavailable(bool unavailable)
    {
        m_unavailable = unavailable;
        Q_EMIT stateChanged();
    }
    void setBusy(bool busy)
    {
        m_busy = busy;
        Q_EMIT stateChanged();
    }
    void setActivePreset(const QString &presetId)
    {
        m_activeId = presetId;
        Q_EMIT presetsChanged();
    }
    void setOwnPreset(bool present)
    {
        m_ownPreset = present;
        Q_EMIT presetsChanged();
    }
    void setMessages(QString notice, QString error)
    {
        m_notice = std::move(notice);
        m_error = std::move(error);
        Q_EMIT stateChanged();
    }

    QList<QVariantList> calls;
    int delaySetCount = 0;
    int lastDelayMs = -1;
    bool admitDelay = true;
    bool admitStoreActions = true;

Q_SIGNALS:
    void stateChanged();
    void presetsChanged();

private:
    bool m_unavailable = false;
    bool m_busy = false;
    bool m_ownPreset = true;
    QString m_activeId = QStringLiteral("fixture");
    QString m_notice;
    QString m_error;
    bool m_delayAvailable = true;
    bool m_delayEditable = true;
    bool m_delayPending = false;
    int m_delayMs = 250;
    QString m_delayStatus = QStringLiteral("Saved for auto-hiding panels in every layout profile.");
};

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
