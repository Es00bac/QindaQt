// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import java.util.List;

/**
 * Resolves a package's launcher activity and starts it on the mirror display.
 *
 * <p>AGENT-NOTE: launching uses IActivityManager as the shell user with
 * ActivityOptions.setLaunchDisplayId. If the app already has a task on the phone's own
 * screen, Android moves that task to the mirror display; DESTROY_CONTENT_ON_REMOVAL then ends
 * it when the window closes. The server never force-stops a package.
 */
final class AppLauncher {
    /** Activity start results below zero are ActivityManager.START_* errors. */
    private static final int START_SUCCESS = 0;

    private final HiddenApi api;
    private final int userId;

    AppLauncher(HiddenApi api, int userId) {
        this.api = api;
        this.userId = userId;
    }

    ComponentName resolveLauncher(String packageName) throws Exception {
        Intent query = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
                .setPackage(packageName);
        List<ResolveInfo> found = queryActivities(api, query, userId);
        if (found.isEmpty()) {
            return null;
        }
        ResolveInfo first = found.get(0);
        return new ComponentName(first.activityInfo.packageName, first.activityInfo.name);
    }

    void launch(ComponentName component, int displayId) throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
                .setComponent(component)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Bundle options = ActivityOptions.makeBasic().setLaunchDisplayId(displayId).toBundle();
        Object result;
        if (api.startActivityTakesFeature) {
            result = HiddenApi.call(api.startActivity, api.activityService, null, HiddenApi.SHELL_PACKAGE,
                    null, intent, null, null, null, 0, 0, null, options, userId);
        } else {
            result = HiddenApi.call(api.startActivity, api.activityService, null, HiddenApi.SHELL_PACKAGE,
                    intent, null, null, null, 0, 0, null, options, userId);
        }
        int code = (Integer) result;
        if (code < START_SUCCESS) {
            throw new IllegalStateException("startActivity returned " + code + " for "
                    + component.flattenToShortString());
        }
        Ln.i("launched " + component.flattenToShortString() + " on display " + displayId + " (" + code + ")");
    }

    @SuppressWarnings("unchecked")
    static List<ResolveInfo> queryActivities(HiddenApi api, Intent query, int userId) throws Exception {
        Object flags = api.queryFlagsAreLong ? (Object) 0L : (Object) 0;
        Object slice = HiddenApi.call(api.queryIntentActivities, api.packageManager, query, null, flags, userId);
        if (slice == null) {
            throw new IllegalStateException("queryIntentActivities returned null");
        }
        return (List<ResolveInfo>) slice.getClass().getMethod("getList").invoke(slice);
    }
}
