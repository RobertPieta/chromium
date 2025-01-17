// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_DELEGATE_H_
#define CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "cc/trees/layer_tree_host_client.h"

namespace cc {
class LayerTreeFrameSink;
class SwapPromise;
struct ElementId;
}  // namespace cc

namespace viz {
class CopyOutputRequest;
}

namespace content {

// Consumers of LayerTreeView implement this delegate in order to
// transport compositing information across processes.
class LayerTreeViewDelegate {
 public:
  using LayerTreeFrameSinkCallback =
      base::OnceCallback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>;

  // Report viewport related properties during a commit from the compositor
  // thread.
  virtual void ApplyViewportChanges(
      const cc::ApplyViewportChangesArgs& args) = 0;

  // Record use count of wheel/touch sources for scrolling on the compositor
  // thread.
  virtual void RecordWheelAndTouchScrollingCount(
      bool has_scrolled_by_wheel,
      bool has_scrolled_by_touch) = 0;

  // Send overscroll DOM event when overscrolling has happened on the compositor
  // thread.
  virtual void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) = 0;

  // Send scrollend DOM event when gesture scrolling on the compositor thread
  // has finished.
  virtual void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) = 0;

  // Notifies that the compositor has issued a BeginMainFrame.
  virtual void BeginMainFrame(base::TimeTicks frame_time) = 0;

  // Requests a LayerTreeFrameSink to submit CompositorFrames to.
  virtual void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) = 0;

  // Notifies that the draw commands for a committed frame have been issued.
  virtual void DidCommitAndDrawCompositorFrame() = 0;

  // Notifies about a compositor frame commit operation having finished.
  virtual void DidCommitCompositorFrame() = 0;

  // Called by the compositor when page scale animation completed.
  virtual void DidCompletePageScaleAnimation() = 0;

  // Requests that a UMA and UKM metrics be recorded for the total frame time
  // and the portion of frame time spent in various sub-systems.
  // Call RecordStartOfFrameMetrics when a main frame is starting, and call
  // RecordEndOfFrameMetrics as soon as the total frame time becomes known for
  // a given frame. For example, ProxyMain::BeginMainFrame calls
  // RecordStartOfFrameMetrics just be WillBeginCompositorFrame() and
  // RecordEndOfFrameMetrics immediately before aborting or committing a frame
  // (at the same time Tracing measurements are taken).
  virtual void RecordStartOfFrameMetrics() = 0;
  virtual void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) = 0;

  // Requests a visual frame-based update to the state of the delegate if there
  // is an update available.
  virtual void UpdateVisualState() = 0;

  // Indicates that the compositor is about to begin a frame. This is primarily
  // to signal to flow control mechanisms that a frame is beginning, not to
  // perform actual painting work. When |record_main_frame_metrics| is true
  // we are in a frame that shoujld capture metrics data, and the local frame's
  // UKM aggregator must be informed that the frame is starting.
  virtual void WillBeginCompositorFrame() = 0;

  // For use in web test mode only, attempts to copy the full content of the
  // compositor.
  virtual std::unique_ptr<cc::SwapPromise> RequestCopyOfOutputForWebTest(
      std::unique_ptr<viz::CopyOutputRequest> request) = 0;

 protected:
  virtual ~LayerTreeViewDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_LAYER_TREE_VIEW_DELEGATE_H_
