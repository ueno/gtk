/*
 * Copyright © 2014 Daiki Ueno
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Daiki Ueno <dueno@src.gnome.org>
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include "gtk/gtkintl.h"
#include "gtk/gtkimmodule.h"
#include "gtkimcontextdbus.h"

#define CONTEXT_ID "dbus"

static const GtkIMContextInfo info = {
  CONTEXT_ID,
  N_("DBus"),
  GETTEXT_PACKAGE,
  GTK_LOCALEDIR,
  ""
};

static const GtkIMContextInfo *info_list[] = {
  &info
};

#ifndef INCLUDE_IM_dbus
#define MODULE_ENTRY(type, function) G_MODULE_EXPORT type im_module_ ## function
#else
#define MODULE_ENTRY(type, function) type _gtk_immodule_dbus_ ## function
#endif

MODULE_ENTRY (void, init) (GTypeModule * module)
{
  gtk_im_context_dbus_register_type (module);
}

MODULE_ENTRY (void, exit) (void)
{
}

MODULE_ENTRY (void, list) (const GtkIMContextInfo *** contexts, int *n_contexts)
{
  *contexts = info_list;
  *n_contexts = G_N_ELEMENTS (info_list);
}

MODULE_ENTRY (GtkIMContext *, create) (const gchar * context_id)
{
  if (!strcmp (context_id, CONTEXT_ID))
    return g_object_new (GTK_TYPE_IM_CONTEXT_DBUS, NULL);
  else
    return NULL;
}
