// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import org.junit.Assert;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.util.RenderTestRule;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;

/**
 * Class containing utility functions for using RenderTests with VR UI.
 */
public class RenderTestUtils {
    // Creating a temporary directory doesn't seem to work, so instead use a fixed location that
    // we know we can write to.
    private static final String IMAGE_DUMP_DIR = "chrome/test/data/vr/framebuffer_dumps";

    /**
     * Helper function for running the general dumpAndCompare when only one image needs to be
     * compared.
     *
     * @param suffix the framebuffer suffix from NativeUiUtils to use.
     * @param id the RenderTest image ID to use when comparing the produced imaged to the golden.
     * @param rule the RenderTestRule to use for comparing images.
     */
    public static void dumpAndCompare(String suffix, String id, RenderTestRule rule)
            throws IOException, InterruptedException {
        HashMap<String, String> suffixToId = new HashMap<String, String>();
        suffixToId.put(suffix, id);
        dumpAndCompare(suffixToId, rule);
    }

    /**
     * Dumps all framebuffers on the next frame and compares the specified images using the provided
     * RenderTestRule.
     *
     * @param suffixToIds a map from framebuffer suffixes from NativeUiUtils to RenderTest image
     *        IDs.
     * @param rule the RenderTestRule to use for comparing images.
     */
    public static void dumpAndCompare(HashMap<String, String> suffixToIds, RenderTestRule rule)
            throws IOException, InterruptedException {
        File dumpDirectory = new File(UrlUtils.getIsolatedTestFilePath(IMAGE_DUMP_DIR));
        if (!dumpDirectory.exists() && !dumpDirectory.isDirectory()) {
            Assert.assertTrue("Failed to make framebuffer dump directory", dumpDirectory.mkdirs());
        }
        // Properly joining these with Paths.get() would be great, except that Paths.get() requires
        // API level 26.
        File baseImagePath = new File(dumpDirectory, "dump");
        // Dump the next frame's frame buffers to disk.
        NativeUiUtils.dumpNextFramesFrameBuffers(baseImagePath.getPath());

        for (String suffix : suffixToIds.keySet()) {
            String id = suffixToIds.get(suffix);
            String filepath = baseImagePath.getPath() + suffix + ".png";
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap bitmap = BitmapFactory.decodeFile(filepath, options);

            // The browser UI dump contains both eyes rendered, which is unnecessary for comparing
            // since any difference in one should show up in the other. So, take the left half of
            // the image to get the left eye, which reduces the amount of space the image takes up.
            if (suffix.equals(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI)) {
                bitmap = Bitmap.createBitmap(
                        bitmap, 0, 0, bitmap.getWidth() / 2, bitmap.getHeight());
            }
            rule.compareForResult(bitmap, id);
        }
    }
}