// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_USER_ACTIVITY_USER_ACTIVITY_OBSERVER_H_
#define UI_BASE_USER_ACTIVITY_USER_ACTIVITY_OBSERVER_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace ui {
class Event;
}

namespace ui {

// Interface for classes that want to be notified about user activity.
// Implementations should register themselves with UserActivityDetector.
class COMPONENT_EXPORT(UI_BASE_USER_ACTIVITY) UserActivityObserver {
 public:
  // Invoked periodically while the user is active (i.e. generating input
  // events). |event| is the event that triggered the notification; it may
  // be NULL in some cases (e.g. testing or synthetic invocations).
  virtual void OnUserActivity(const ui::Event* event) = 0;

 protected:
  UserActivityObserver() {}
  virtual ~UserActivityObserver() {}

  DISALLOW_COPY_AND_ASSIGN(UserActivityObserver);
};

}  // namespace ui

#endif  // UI_BASE_USER_ACTIVITY_USER_ACTIVITY_OBSERVER_H_
