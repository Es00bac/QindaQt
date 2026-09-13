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
    Q_PROPERTY(bool displayScopeChangeAvailable READ trueValue CONSTANT)
    Q_PROPERTY(bool primaryDisplayAvailable READ primaryDisplayAvailable NOTIFY contentChanged)
    Q_PROPERTY(QString displayScopeError READ displayScopeError NOTIFY contentChanged)
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
    Q_PROPERTY(QString selectedKind READ selectedKind CONSTANT)
    Q_PROPERTY(QString selectedPanelId READ selectedPanelId CONSTANT)
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
    [[nodiscard]] bool primaryDisplayAvailable() const
    {
        return m_primaryDisplayAvailable;
    }
    [[nodiscard]] QString displayScopeError() const
    {
        return m_primaryDisplayAvailable
            ? QString{} : QStringLiteral("The primary display is not currently known");
    }
    [[nodiscard]] QString statusText() const
    {
        return QStringLiteral(
            "The running desktop follows each applied layout change");
    }
    [[nodiscard]] QString selectedProfileId() const
    {
        return QStringLiteral("fixture");
    }
    [[nodiscard]] QVariantList profiles() const
    {
        // The gallery renders a miniature desktop per profile from the same
        // panel summary shape the production projection publishes.
        const QVariantList fixturePanels{QVariantMap{
            {QStringLiteral("id"), QStringLiteral("bar")},
            {QStringLiteral("edge"), QStringLiteral("top")},
            {QStringLiteral("alignment"), QStringLiteral("fill")},
            {QStringLiteral("layer"), QStringLiteral("above")},
            {QStringLiteral("hideMode"), QStringLiteral("never")},
            {QStringLiteral("thickness"), 32},
            {QStringLiteral("length"), 1.0},
            {QStringLiteral("appletCount"), 1},
        }};
        return {QVariantMap{{QStringLiteral("id"), QStringLiteral("fixture")},
                            {QStringLiteral("name"), QStringLiteral("Fixture")},
                            {QStringLiteral("description"), QStringLiteral("Test")},
                            {QStringLiteral("panels"), fixturePanels}}};
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
            {QStringLiteral("output"), QStringLiteral("DP-1")},
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
    [[nodiscard]] QString selectedKind() const { return QStringLiteral("panel"); }
    [[nodiscard]] QString selectedPanelId() const { return QStringLiteral("bar"); }
    [[nodiscard]] QVariantMap selectedProperties() const
    {
        return {
            {QStringLiteral("kind"), QStringLiteral("panel")},
            {QStringLiteral("name"), QStringLiteral("Top panel")},
            {QStringLiteral("output"), QStringLiteral("DP-1")},
            {QStringLiteral("outputScope"), QStringLiteral("primary")},
            {QStringLiteral("edge"), QStringLiteral("top")},
            {QStringLiteral("alignment"), QStringLiteral("fill")},
            {QStringLiteral("layer"), QStringLiteral("above")},
            {QStringLiteral("hideMode"), QStringLiteral("never")},
            {QStringLiteral("rows"), 1},
            {QStringLiteral("thickness"), 32},
            {QStringLiteral("length"), 1.0},
        };
    }

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
    Q_INVOKABLE bool configureSelectedPanel(const QString &field,
                                             const QVariant &value)
    {
        lastConfiguredField = field;
        lastConfiguredValue = value;
        ++configurePanelCalls;
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

    void setPrimaryDisplayAvailable(bool available)
    {
        if (m_primaryDisplayAvailable == available) {
            return;
        }
        m_primaryDisplayAvailable = available;
        Q_EMIT contentChanged();
    }

Q_SIGNALS:
    void contentChanged();

public:
    int keyboardInsertCalls = 0;
    int configurePanelCalls = 0;
    QString lastConfiguredField;
    QVariant lastConfiguredValue;

private:
    bool m_dirty = false;
    bool m_primaryDisplayAvailable = true;
};

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
