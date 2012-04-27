// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANEL_LAYOUT_MANAGER_H_
#define ASH_WM_PANEL_LAYOUT_MANAGER_H_
#pragma once

#include <list>

#include "ash/ash_export.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {
class Launcher;

namespace internal {

// PanelLayoutManager is responsible for organizing panels within the
// workspace. It is associated with a specific container window (i.e.
// kShellWindowId_PanelContainer) and controls the layout of any windows
// added to that container.
//
// The constructor takes a |panel_container| argument which is expected to set
// its layout manager to this instance, e.g.:
// panel_container->SetLayoutManager(new PanelLayoutManager(panel_container));

class ASH_EXPORT PanelLayoutManager : public aura::LayoutManager,
                                      public ash::LauncherIconObserver,
                                      public aura::WindowObserver {
 public:
  explicit PanelLayoutManager(aura::Window* panel_container);
  virtual ~PanelLayoutManager();

  void StartDragging(aura::Window* panel);
  void FinishDragging();

  void ToggleMinimize(aura::Window* panel);

  void SetLauncher(ash::Launcher* launcher);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overridden from ash::LauncherIconObserver
  virtual void OnLauncherIconPositionsChanged() OVERRIDE;

  // Overridden from aura::WindowObserver
  virtual void OnWindowPropertyChanged(
      aura::Window* window, const void* key, intptr_t old) OVERRIDE;

 private:
  typedef std::list<aura::Window*> PanelList;

  // Called whenever the panel layout might change.
  void Relayout();

  // Called whenever the panel stacking order needs to be updated (e.g. focus
  // changes or a panel is moved).
  void UpdateStacking(aura::Window* active_panel);

  // Parent window associated with this layout manager.
  aura::Window* panel_container_;
  // Protect against recursive calls to Relayout().
  bool in_layout_;
  // Ordered list of unowned pointers to panel windows.
  PanelList panel_windows_;
  // The panel being dragged.
  aura::Window* dragged_panel_;
  // The launcher we are observing for launcher icon changes.
  Launcher* launcher_;
  // The last active panel. Used to maintain stacking even if no panels are
  // currently focused.
  aura::Window* last_active_panel_;

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_PANEL_LAYOUT_MANAGER_H_
