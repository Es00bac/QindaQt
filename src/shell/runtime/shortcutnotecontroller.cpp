// SPDX-License-Identifier: GPL-3.0-or-later
#include "shortcutnotecontroller.h"

#include "globalshortcutregistrar.h"
#include "kglobalaccelshortcutregistrar.h"
#include "shortcutnoteshortcut.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDBusConnection>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>

namespace QindaQt::Shell {
namespace {

// AGENT-GUARD: declaration order makes the client die before its borrowed
// transport, mirroring the shell's quieting-bridge member convention. A
// transport-first destruction order would leave ~SettingsClient touching a
// dangling reference.
class ProductionSession final : public QObject {
public:
    explicit ProductionSession(const QDBusConnection &bus, QObject *parent)
        : QObject(parent),
          transport(bus),
          client(transport, QStringList{ShortcutNoteController::settingsKey()})
    {
    }

    Services::SettingsClient::QtSettingsTransport transport;
    Services::SettingsClient::SettingsClient client;
};

} // namespace

QString ShortcutNoteController::settingsKey()
{
    return QStringLiteral("shell.shortcutNoteDismissed");
}

ShortcutNoteController::ShortcutNoteController(
    QGuiApplication &app, QQmlEngine &engine,
    Services::SettingsClient::SettingsClient &settings,
    GlobalShortcutRegistrar &registrar, QObject *parent)
    : QObject(parent),
      m_app(app),
      m_engine(engine),
      m_settings(settings)
{
    // AGENT-NOTE: fail visible. Before the first confirmed snapshot, and
    // whenever Settings1 is unreachable, the note stays visible so the
    // advertised shortcuts are never silently lost; dismissal then lasts for
    // the session only.
    if (const QScreen *primary = app.primaryScreen()) {
        m_primaryScreenName = primary->name();
    }
    connect(&m_app, &QGuiApplication::primaryScreenChanged, this,
            [this](QScreen *screen) {
                const QString name = screen ? screen->name() : QString{};
                if (name == m_primaryScreenName) {
                    return;
                }
                m_primaryScreenName = name;
                Q_EMIT primaryScreenNameChanged();
            });
    connect(&m_settings,
            &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &ShortcutNoteController::applySnapshot);
    connect(
        &m_settings,
        &Services::SettingsClient::SettingsClient::commitFinished, this,
        [this](const Services::SettingsClient::CommitOutcome &outcome) {
            if (outcome.status
                != Services::SettingsProtocol::SettingsWireStatus::Applied) {
                qWarning().noquote()
                    << "QindaQt shortcut-note preference was not applied:"
                    << outcome.message;
            }
        });
    m_shortcut =
        new ShortcutNoteShortcut(registrar, [this] { toggle(); }, this);
}

ShortcutNoteController *ShortcutNoteController::createProduction(
    QGuiApplication &app, QQmlEngine &engine, QObject *parent)
{
    auto *session =
        new ProductionSession(QDBusConnection::sessionBus(), nullptr);
    // AGENT-NOTE: KGlobalAccelShortcutRegistrar is stateless and registration
    // completes synchronously inside the constructor; nothing retains the
    // registrar afterwards, so this stack instance may die at scope exit.
    KGlobalAccelShortcutRegistrar registrar;
    auto *controller = new ShortcutNoteController(app, engine, session->client,
                                                   registrar, parent);
    session->setParent(controller);
    QString error;
    if (!session->client.start(&error)) {
        qWarning().noquote()
            << "QindaQt shortcut-note settings scope unavailable; dismissal"
               " will last for this session only:"
            << error;
    }
    return controller;
}

bool ShortcutNoteController::noteVisible() const noexcept
{
    return m_noteVisible;
}

QString ShortcutNoteController::primaryScreenName() const
{
    return m_primaryScreenName;
}

int ShortcutNoteController::topInset() const noexcept
{
    return m_topInset;
}

int ShortcutNoteController::rightInset() const noexcept
{
    return m_rightInset;
}

QVariantMap ShortcutNoteController::theme() const
{
    return m_theme;
}

ShortcutNoteShortcut *ShortcutNoteController::shortcut() const noexcept
{
    return m_shortcut;
}

void ShortcutNoteController::setTheme(const QVariantMap &theme)
{
    if (m_theme == theme) {
        return;
    }
    m_theme = theme;
    Q_EMIT themeChanged();
}

void ShortcutNoteController::setPanelInsets(int top, int right)
{
    const int clampedTop = top < 0 ? 0 : top;
    const int clampedRight = right < 0 ? 0 : right;
    if (clampedTop == m_topInset && clampedRight == m_rightInset) {
        return;
    }
    m_topInset = clampedTop;
    m_rightInset = clampedRight;
    Q_EMIT insetsChanged();
}

void ShortcutNoteController::dismiss()
{
    if (!m_noteVisible) {
        return;
    }
    m_noteVisible = false;
    Q_EMIT noteVisibleChanged();
    persist(true);
}

void ShortcutNoteController::toggle()
{
    if (m_noteVisible) {
        dismiss();
        return;
    }
    m_noteVisible = true;
    Q_EMIT noteVisibleChanged();
    persist(false);
}

void ShortcutNoteController::applySnapshot()
{
    // AGENT-GUARD: skip snapshots observed while our own commit is in flight;
    // accepting one could resurrect a just-dismissed note from a pre-write
    // refresh before the commit reply confirms the newer truth.
    if (m_settings.writeInFlight()) {
        return;
    }
    const auto &snapshot = m_settings.snapshot();
    if (!snapshot.has_value()) {
        return;
    }
    const QVariant value = snapshot->values.value(settingsKey());
    if (value.metaType().id() != QMetaType::Bool) {
        return;
    }
    const bool visible = !value.toBool();
    if (visible == m_noteVisible) {
        return;
    }
    m_noteVisible = visible;
    Q_EMIT noteVisibleChanged();
}

void ShortcutNoteController::persist(bool dismissed)
{
    QString error;
    if (!m_settings.setUserValue(settingsKey(), dismissed, &error)) {
        qWarning().noquote()
            << "QindaQt shortcut-note preference write was rejected; the"
               " visibility change lasts for this session only:"
            << error;
    }
}

bool ShortcutNoteController::ensureCardComponent()
{
    if (m_cardComponent || m_cardComponentFailed) {
        return m_cardComponent != nullptr;
    }
    m_cardComponent = std::make_unique<QQmlComponent>(&m_engine);
    m_cardComponent->loadFromModule(QStringLiteral("QindaQt.Shell.Runtime"),
                                    QStringLiteral("ShortcutNoteCard"));
    if (!m_cardComponent->isReady()) {
        qWarning().noquote()
            << "QindaQt shortcut note could not load its card component:"
            << m_cardComponent->errorString();
        m_cardComponent.reset();
        m_cardComponentFailed = true;
        return false;
    }
    return true;
}

void ShortcutNoteController::attachToWindow(QQuickWindow &window,
                                            const QString &screenName)
{
    if (!ensureCardComponent()) {
        return;
    }
    QObject *object = m_cardComponent->createWithInitialProperties(
        {{QStringLiteral("note"), QVariant::fromValue(static_cast<QObject *>(this))},
         {QStringLiteral("screenName"), screenName}});
    auto *item = qobject_cast<QQuickItem *>(object);
    if (item == nullptr) {
        qWarning().noquote()
            << "QindaQt shortcut note card did not create an item";
        delete object;
        return;
    }
    // The window owns the card: screen removal or wallpaper teardown destroys
    // it without any controller bookkeeping.
    item->setParent(&window);
    item->setParentItem(window.contentItem());
}

} // namespace QindaQt::Shell
