/*
 * OLLMchat android-bugs.patch compile marker.
 * CI/build logs should contain: gdkandroidollmchatpatch.c.o
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"
#include <glib.h>

/* Survives in libgtk-4.so; verify-apk.sh greps for these tags. */
const char gdk_android_ollmchat_bugs_tag[] = "ollmchat-android-bugs-v4";
const char gdk_android_ollmchat_popup_tag[] = "ollmchat-android-popup-v4";
const char gdk_android_ollmchat_tls_tag[] = "ollmchat-android-tls-v4";

static void
gdk_android_ollmchat_patch_marker (void)
{
  /* Keep the tags referenced so release builds do not drop them. */
  g_debug ("%s %s %s",
           gdk_android_ollmchat_bugs_tag,
           gdk_android_ollmchat_popup_tag,
           gdk_android_ollmchat_tls_tag);
}
