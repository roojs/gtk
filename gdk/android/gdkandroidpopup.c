/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <math.h>

#include "gdkseatprivate.h"

#include "gdkandroidinit-private.h"
#include "gdkandroidtoplevel-private.h"

#include "gdkandroidpopup-private.h"

static gboolean
gdk_android_popup_present (GdkPopup       *popup,
                           int             width,
                           int             height,
                           GdkPopupLayout *layout)
{
  GdkAndroidPopup *self = (GdkAndroidPopup *) popup;
  GdkAndroidSurface *surface_impl = (GdkAndroidSurface *) self;

  GdkAndroidToplevel *activity_toplevel =
      gdk_android_surface_get_toplevel (surface_impl);
  GdkAndroidSurface *activity_impl = (GdkAndroidSurface *) activity_toplevel;
  GdkSurface *activity_surface = (GdkSurface *) activity_toplevel;

  GdkSurface *popup_parent_surface = gdk_popup_get_parent (popup);
  if (popup_parent_surface == NULL)
    popup_parent_surface = activity_surface;
  GdkAndroidSurface *popup_parent_impl = (GdkAndroidSurface *) popup_parent_surface;

  surface_impl->visible = TRUE;

  gdk_popup_layout_ref (layout);
  g_clear_pointer (&self->layout, gdk_popup_layout_unref);
  self->layout = layout;

  gint shadow_left, shadow_right, shadow_top, shadow_bottom;
  gdk_popup_layout_get_shadow_width (layout,
                                     &shadow_left,
                                     &shadow_right,
                                     &shadow_top,
                                     &shadow_bottom);

  /* Parent position from Java layout may lag behind GTK layout for nested
   * surfaces (e.g. touch selection bubbles inside Adw dialogs). */
  if (popup_parent_impl->cfg.scale > 0)
    {
      GdkSurface *parent_surf = GDK_SURFACE (popup_parent_surface);

      parent_surf->x =
          (gint) (popup_parent_impl->cfg.x / popup_parent_impl->cfg.scale);
      parent_surf->y =
          (gint) (popup_parent_impl->cfg.y / popup_parent_impl->cfg.scale);
    }


  GdkRectangle bounds;
  bounds.x = popup_parent_surface->x;
  bounds.y = popup_parent_surface->y;
  bounds.width = popup_parent_surface->width;
  bounds.height = popup_parent_surface->height;

  memset (&self->popup_bounds, 0, sizeof (GdkRectangle));
  gdk_surface_layout_popup_helper (GDK_SURFACE (self),
                                   width,
                                   height,
                                   shadow_left,
                                   shadow_right,
                                   shadow_top,
                                   shadow_bottom,
                                   NULL,
                                   &bounds,
                                   self->layout,
                                   GDK_SURFACE_LAYOUT_POPUP_HELPER_ROOT_OUT,
                                   &self->popup_bounds);
  /* Keep native geometry in sync with the layout result. Child popups
   * (e.g. touch selection bubbles inside a dialog) consult the parent
   * surface position during layout, but Java only reports position
   * asynchronously via notifyLayoutPosition. */
  {
    GdkSurface *surface = GDK_SURFACE (self);

    surface->x = self->popup_bounds.x;
    surface->y = self->popup_bounds.y;
    surface->width = self->popup_bounds.width;
    surface->height = self->popup_bounds.height;
    surface_impl->cfg.x =
        (gint) (self->popup_bounds.x * activity_impl->cfg.scale);
    surface_impl->cfg.y =
        (gint) (self->popup_bounds.y * activity_impl->cfg.scale);
    surface_impl->cfg.width =
        (gint) ceilf (self->popup_bounds.width * activity_impl->cfg.scale);
    surface_impl->cfg.height =
        (gint) ceilf (self->popup_bounds.height * activity_impl->cfg.scale);
  }

  g_message ("OLLMchat.Popup: present popup=%p parent=%p activity=%p "
             "parent_xywh=%d,%d %dx%d bounds=%d,%d %dx%d "
             "popup_bounds=%d,%d %dx%d java_cfg=%d,%d %dx%d scale=%d new_surface=%d",
             popup, popup_parent_surface, activity_surface,
             popup_parent_surface->x, popup_parent_surface->y,
             popup_parent_surface->width, popup_parent_surface->height,
             bounds.x, bounds.y, bounds.width, bounds.height,
             self->popup_bounds.x, self->popup_bounds.y,
             self->popup_bounds.width, self->popup_bounds.height,
             surface_impl->cfg.x, surface_impl->cfg.y,
             surface_impl->cfg.width, surface_impl->cfg.height,
             activity_impl->cfg.scale,
             surface_impl->surface == NULL);

  JNIEnv *env = gdk_android_get_env ();
  if (!surface_impl->surface)
    {
      jobject view = (*env)->GetObjectField (env, activity_toplevel->activity,
                                             gdk_android_get_java_cache ()->toplevel.toplevel_view);
      (*env)->CallVoidMethod (env, view, gdk_android_get_java_cache ()->toplevel_view.push_popup, (jlong) self,
                              (jint) (self->popup_bounds.x * activity_impl->cfg.scale),
                              (jint) (self->popup_bounds.y * activity_impl->cfg.scale),
                              (jint) ceilf (self->popup_bounds.width * activity_impl->cfg.scale),
                              (jint) ceilf (self->popup_bounds.height * activity_impl->cfg.scale));
      (*env)->DeleteLocalRef (env, view);
    }
  else
    {
      (*env)->CallVoidMethod (env, surface_impl->surface, gdk_android_get_java_cache ()->surface.reposition,
                              (jint) (self->popup_bounds.x * activity_impl->cfg.scale),
                              (jint) (self->popup_bounds.y * activity_impl->cfg.scale),
                              (jint) ceilf (self->popup_bounds.width * activity_impl->cfg.scale),
                              (jint) ceilf (self->popup_bounds.height * activity_impl->cfg.scale));
      (*env)->CallVoidMethod(env, surface_impl->surface, gdk_android_get_java_cache ()->surface.set_visibility, TRUE);
    }

  return TRUE;
}

static GdkGravity
gdk_android_popup_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_android_popup_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_android_popup_get_position_x (GdkPopup *popup)
{
  GdkSurface *surface = (GdkSurface *)popup;
  return surface->x - (surface->parent ? surface->parent->x : 0);
}

static int
gdk_android_popup_get_position_y (GdkPopup *popup)
{
  GdkSurface *surface = (GdkSurface *)popup;
  return surface->y - (surface->parent ? surface->parent->y : 0);
}

static void
gdk_android_popup_iface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_android_popup_present;
  iface->get_surface_anchor = gdk_android_popup_get_surface_anchor;
  iface->get_rect_anchor = gdk_android_popup_get_rect_anchor;
  iface->get_position_x = gdk_android_popup_get_position_x;
  iface->get_position_y = gdk_android_popup_get_position_y;
}

/**
 * GdkAndroidPopup:
 *
 * The Android implementation of [iface@Gdk.Popup].
 *
 * Since: 4.18
 */
G_DEFINE_TYPE_WITH_CODE (GdkAndroidPopup, gdk_android_popup, GDK_TYPE_ANDROID_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP, gdk_android_popup_iface_init))

enum
{
  N_PROPERTIES = 1,
};

static void
gdk_android_popup_finalize (GObject *object)
{
  GdkAndroidPopup *self = (GdkAndroidPopup *) object;
  GdkSurface *parent = ((GdkSurface *) self)->parent;

  if (parent != NULL)
    parent->children = g_list_remove (parent->children, self);

  g_clear_object (&GDK_SURFACE (self)->parent);
  g_clear_pointer (&self->layout, gdk_popup_layout_unref);

  G_OBJECT_CLASS (gdk_android_popup_parent_class)->finalize (object);
}

static void
gdk_android_popup_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case N_PROPERTIES + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case N_PROPERTIES + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_android_popup_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  GdkSurface *surface = (GdkSurface *) object;

  switch (prop_id)
    {
    case N_PROPERTIES + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case N_PROPERTIES + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_android_popup_constructed (GObject *object)
{
  // gdk_surface_set_frame_clock (surface, gdk_surface_get_frame_clock (surface->parent));

  G_OBJECT_CLASS (gdk_android_popup_parent_class)->constructed (object);
}

static void
gdk_android_popup_reposition (GdkAndroidSurface *surface_impl)
{
  GdkAndroidPopup *self = (GdkAndroidPopup *) surface_impl;
  GdkSurface *surface = (GdkSurface *) self;

  if (self->layout == NULL || surface->parent == NULL || !surface_impl->visible)
    return;

  gdk_android_popup_present ((GdkPopup *) self,
                             surface->width, surface->height,
                             self->layout);
}

static void
gdk_android_popup_class_init (GdkAndroidPopupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkAndroidSurfaceClass *surface_impl_class = GDK_ANDROID_SURFACE_CLASS (klass);

  object_class->constructed = gdk_android_popup_constructed;
  object_class->finalize = gdk_android_popup_finalize;
  object_class->get_property = gdk_android_popup_get_property;
  object_class->set_property = gdk_android_popup_set_property;

  surface_impl_class->reposition = gdk_android_popup_reposition;

  gdk_popup_install_properties (object_class, N_PROPERTIES);
}

static void
gdk_android_popup_init (GdkAndroidPopup *self)
{}
