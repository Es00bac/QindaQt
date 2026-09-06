// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantList>

namespace QindaQt::Apps::SettingsCustomize::TestSupport {

class StubCustomizeSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading READ falseValue CONSTANT)
    Q_PROPERTY(bool ready READ trueValue CONSTANT)
    Q_PROPERTY(bool saving READ falseValue CONSTANT)
    Q_PROPERTY(bool conflict READ falseValue CONSTANT)
    Q_PROPERTY(bool unavailable READ falseValue CONSTANT)
    Q_PROPERTY(bool canEdit READ trueValue CONSTANT)
    Q_PROPERTY(bool dirty READ dirty NOTIFY contentChanged)
    Q_PROPERTY(bool applyAvailable READ falseValue CONSTANT)
    Q_PROPERTY(bool canUndo READ falseValue CONSTANT)
    Q_PROPERTY(bool canRedo READ falseValue CONSTANT)
    Q_PROPERTY(bool visualDragActive READ falseValue CONSTANT)
    Q_PROPERTY(bool dropAccepted READ trueValue CONSTANT)
    Q_PROPERTY(QString dropReason READ emptyString CONSTANT)
    Q_PROPERTY(QString statusText READ statusText CONSTANT)
    Q_PROPERTY(QString errorText READ emptyString CONSTANT)
    Q_PROPERTY(QString announcement READ emptyString CONSTANT)
    Q_PROPERTY(QString selectedProfileId READ selectedProfileId CONSTANT)
    Q_PROPERTY(QVariantList profiles READ profiles CONSTANT)
    Q_PROPERTY(QVariantList panels READ panels CONSTANT)
    Q_PROPERTY(QVariantList palette READ palette CONSTANT)
    Q_PROPERTY(QString selectedKind READ emptyString CONSTANT)
    Q_PROPERTY(QString selectedPanelId READ emptyString CONSTANT)
    Q_PROPERTY(QString selectedAppletId READ emptyString CONSTANT)
    Q_PROPERTY(QVariantMap selectedProperties READ selectedProperties CONSTANT)

public:
    explicit StubCustomizeSettingsModel(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    [[nodiscard]] bool falseValue() const { return false; }
    [[nodiscard]] bool trueValue() const { return true; }
    [[nodiscard]] bool dirty() const { return m_dirty; }
    [[nodiscard]] QString emptyString() const { return {}; }
    [[nodiscard]] QString statusText() const
    {
        return QStringLiteral(
            "Your applied layout will be used the next time you start your desktop");
    }
    [[nodiscard]] QString selectedProfileId() const
    {
        return QStringLiteral("fixture");
    }
    [[nodiscard]] QVariantList profiles() const
    {
        return {QVariantMap{{QStringLiteral("id"), QStringLiteral("fixture")},
                            {QStringLiteral("name"), QStringLiteral("Fixture")},
                            {QStringLiteral("description"), QStringLiteral("Test")}}};
    }
    [[nodiscard]] QVariantList palette() const
    {
        return {QVariantMap{{QStringLiteral("id"), QStringLiteral("clock")},
                            {QStringLiteral("name"), QStringLiteral("Clock")},
                            {QStringLiteral("description"),
                             QStringLiteral("Fixture clock")}}};
    }
    [[nodiscard]] QVariantList panels() const
    {
        const QVariantMap applet{
            {QStringLiteral("id"), QStringLiteral("clock-instance")},
            {QStringLiteral("pluginId"), QStringLiteral("clock")},
            {QStringLiteral("name"), QStringLiteral("Clock")},
            {QStringLiteral("zone"), QStringLiteral("end")},
            {QStringLiteral("position"), 1},
            {QStringLiteral("count"), 1},
            {QStringLiteral("settings"), QVariantMap{}},
        };
        return {QVariantMap{
            {QStringLiteral("id"), QStringLiteral("bar")},
            {QStringLiteral("name"), QStringLiteral("Top panel")},
            {QStringLiteral("output"), QStringLiteral("primary")},
            {QStringLiteral("edge"), QStringLiteral("top")},
            {QStringLiteral("alignment"), QStringLiteral("fill")},
            {QStringLiteral("layer"), QStringLiteral("above")},
            {QStringLiteral("hideMode"), QStringLiteral("never")},
            {QStringLiteral("rows"), 1},
            {QStringLiteral("thickness"), 32},
            {QStringLiteral("length"), 1.0},
            {QStringLiteral("x"), 0},
            {QStringLiteral("y"), 0},
            {QStringLiteral("width"), 1920},
            {QStringLiteral("height"), 32},
            {QStringLiteral("applets"), QVariantList{applet}},
        }};
    }
    [[nodiscard]] QVariantMap selectedProperties() const { return {}; }

    Q_INVOKABLE bool selectProfile(const QString &) { return true; }
    Q_INVOKABLE void selectPanel(const QString &) {}
    Q_INVOKABLE void selectApplet(const QString &, const QString &) {}
    Q_INVOKABLE bool startPaletteDrag(const QString &) { return true; }
    Q_INVOKABLE bool startAppletDrag(const QString &, const QString &) { return true; }
    Q_INVOKABLE bool hoverDropTarget(const QString &, const QString &,
                                     const QString & = {}) { return true; }
    Q_INVOKABLE bool commitDrag() { return true; }
    Q_INVOKABLE bool cancelDrag() { return true; }
    Q_INVOKABLE bool keyboardInsert(const QString &, const QString &,
                                    const QString &, const QString & = {})
    {
        ++keyboardInsertCalls;
        return true;
    }
    Q_INVOKABLE bool keyboardMoveMode() { return true; }
    Q_INVOKABLE bool keyboardStep(const QString &) { return true; }
    Q_INVOKABLE bool removeSelected() { return true; }
    Q_INVOKABLE bool duplicateSelected() { return true; }
    Q_INVOKABLE bool configureSelectedPanel(const QString &, const QVariant &)
    {
        return true;
    }
    Q_INVOKABLE bool undo() { return true; }
    Q_INVOKABLE bool redo() { return true; }
    Q_INVOKABLE bool apply() { return true; }
    Q_INVOKABLE bool discard() { return true; }
    Q_INVOKABLE void retry() {}

    void setDirty(bool dirty)
    {
        if (m_dirty == dirty) {
            return;
        }
        m_dirty = dirty;
        Q_EMIT contentChanged();
    }

Q_SIGNALS:
    void contentChanged();

public:
    int keyboardInsertCalls = 0;

private:
    bool m_dirty = false;
};

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
