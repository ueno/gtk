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

#ifndef __GTK_IM_CONTEXT_DBUS_H__
#define __GTK_IM_CONTEXT_DBUS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_IM_CONTEXT_DBUS (gtk_im_context_dbus_get_type())
#define GTK_IM_CONTEXT_DBUS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IM_CONTEXT_DBUS, GtkIMContextDBus))
#define GTK_IM_CONTEXT_DBUS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_IM_CONTEXT_DBUS, GtkIMContextDBusClass))
#define GTK_IS_IM_CONTEXT_DBUS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_IM_CONTEXT_DBUS))
#define GTK_IS_IM_CONTEXT_DBUS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IM_CONTEXT_DBUS))
#define GTK_IM_CONTEXT_DBUS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_IM_CONTEXT_DBUS, GtkIMContextDBusClass))

typedef struct _GtkIMContextDBusClass GtkIMContextDBusClass;
typedef struct _GtkIMContextDBus GtkIMContextDBus;

struct _GtkIMContextDBus
{
  /*< private >*/
  GtkIMContextSimple parent_instance;

  gboolean has_focus;

  GCancellable *cancellable;
  GDBusConnection *connection;
  GDBusConnection *engine_connection;
  GDBusProxy *proxy;

  gchar *preedit_string;
  PangoAttrList *preedit_attrs;
  gint preedit_cursor_pos;
};

struct _GtkIMContextDBusClass
{
  /*< private >*/
  GtkIMContextSimpleClass parent_class;
};

void          gtk_im_context_dbus_register_type (GTypeModule* type_module);
GType         gtk_im_context_dbus_get_type      (void);

G_END_DECLS

#endif	/* __GTK_IM_CONTEXT_DBUS_H__ */
