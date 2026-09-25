// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <memory>

namespace QindaQt::Apps::FileManager {

class NavigationController;

// ADR-0271: a window's navigation controllers beyond its first. Every tab,
// and each of the two Commander panes, browses with a NavigationController
// of its own, so a tab keeps its folder, history, listing and selection
// while another is in front. The window's commands follow the one the user
// is working in: setActive() moves every window-wide binding (the AppShell
// action states, search results, preview generations) to it.
//
// AGENT-CONTRACT: ui/FolderPanes.qml is the only caller. It creates a
// controller for each new tab or pane with create(), marks the tab the user
// works in with setActive(), and hands a closed tab's controller back with
// release() after destroying the views bound to it. The composition root's
// `recipe` builds a controller exactly as it built the first one; `binder`
// connects everything window-wide to one controller, with `context` as the
// connections' context object, which this class destroys to unbind.
// GUI-thread only.
//
// AGENT-GUARD: bind nothing window-wide to a controller outside `binder`.
// A connection made elsewhere keeps following the first controller after the
// user moves to another tab, and its stale state overwrites the right one.
class FolderNavigations final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *active READ active NOTIFY activeChanged FINAL)

public:
  using Recipe = std::function<std::unique_ptr<NavigationController>()>;
  using Binder = std::function<void(NavigationController &navigation, QObject &context)>;

  // Binds `first`, which the caller owns and which must outlive this object,
  // as the active controller at once.
  FolderNavigations(NavigationController &first, Recipe recipe, Binder binder,
                    QObject *parent = nullptr);
  ~FolderNavigations() override;

  // A new controller browsing `path`, owned by this object until release().
  Q_INVOKABLE QObject *create(const QString &path);
  // Deletes a controller create() made, on the next event-loop turn. The
  // first controller and unknown objects are ignored; releasing the active
  // controller unbinds it first.
  Q_INVOKABLE void release(QObject *navigation);
  // Binds every window-wide seam to `navigation` (a NavigationController).
  Q_INVOKABLE void setActive(QObject *navigation);

  [[nodiscard]] QObject *active() const;

signals:
  void activeChanged();

private:
  void bind(NavigationController *navigation);

  NavigationController &m_first;
  Recipe m_recipe;
  Binder m_binder;
  std::unique_ptr<QObject> m_context;
  QPointer<NavigationController> m_active;
};

} // namespace QindaQt::Apps::FileManager
