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
#include "gtkimcontextdbus.h"

#include <pango/pango.h>

#define DBUS_NAME "org.freedesktop.InputMethod"
#define DBUS_INTERFACE "org.freedesktop.InputMethod"
#define DBUS_OBJECT_PATH "/org/freedesktop/InputMethod"

#define DBUS_ENGINE_INTERFACE "org.freedesktop.InputMethod.Engine"

/* A special modifier mask to prevent infinite recursion in
   filter_keypress and gdk_event_put.  */
#define DBUS_IGNORED_MASK (1 << 31)

static GType gtk_im_context_dbus_type = 0;
static GObjectClass *parent_class;

static void gtk_im_context_dbus_class_init
                                     (GtkIMContextDBusClass *klass);
static void gtk_im_context_dbus_init (GtkIMContextDBus      *dbus_context);

void
gtk_im_context_dbus_register_type (GTypeModule *type_module)
{
  const GTypeInfo gtk_im_context_dbus_info =
    {
      sizeof (GtkIMContextDBusClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) &gtk_im_context_dbus_class_init,
      NULL,
      NULL,
      sizeof (GtkIMContextDBus),
      0,
      (GInstanceInitFunc) &gtk_im_context_dbus_init,
      0,
    };

  gtk_im_context_dbus_type =
    g_type_module_register_type (type_module,
                                 GTK_TYPE_IM_CONTEXT_SIMPLE,
                                 "GtkIMContextDBus",
                                 &gtk_im_context_dbus_info, 0);
}

GType
gtk_im_context_dbus_get_type (void)
{
  g_assert (gtk_im_context_dbus_type != 0);

  return gtk_im_context_dbus_type;
}

static void
gtk_im_context_dbus_real_get_preedit_string (GtkIMContext   *context,
                                             gchar         **str,
                                             PangoAttrList **attrs,
                                             gint           *cursor_pos)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (context);

  g_return_if_fail (str);

  if (!dbus_context->proxy)
    {
      GTK_IM_CONTEXT_CLASS (parent_class)
	->get_preedit_string (context, str, attrs, cursor_pos);
      return;
    }

  *str = g_strdup (dbus_context->preedit_string);
  if (attrs)
    *attrs = pango_attr_list_copy (dbus_context->preedit_attrs);
  if (cursor_pos)
    *cursor_pos = dbus_context->preedit_cursor_pos;
}

static void
filter_keypress_ready (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GdkEvent *event = user_data;
  GError *error = NULL;
  GVariant *value;
  gboolean handled = FALSE;

  value = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (!value)
    {
      gdk_event_free (event);
      g_warning ("Failed to process key event: %s", error->message);
      g_error_free (error);
      return;
    }

  g_variant_get (value, "(b)", &handled);
  g_variant_unref (value);
  if (!handled)
    {
      ((GdkEventKey *) event)->state |= DBUS_IGNORED_MASK;
      gdk_event_put (event);
    }
  gdk_event_free (event);
}

static gboolean
gtk_im_context_dbus_real_filter_keypress (GtkIMContext *context,
                                          GdkEventKey  *event)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (context);

  if ((event->state & DBUS_IGNORED_MASK) != 0
      || !dbus_context->proxy)
    return GTK_IM_CONTEXT_CLASS (parent_class)
      ->filter_keypress (context, event);

  g_dbus_proxy_call (dbus_context->proxy,
                     "KeyEvent",
                     g_variant_new ("(ub)",
                                    event->hardware_keycode,
                                    event->type == GDK_KEY_PRESS),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     dbus_context->cancellable,
                     filter_keypress_ready,
                     gdk_event_copy ((const GdkEvent *) event));
  return TRUE;
}

static void
call_focus (GtkIMContextDBus *dbus_context,
            gboolean          focused)
{
  g_dbus_proxy_call (dbus_context->proxy,
                     "Focus",
                     g_variant_new ("(b)", focused),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     dbus_context->cancellable,
                     NULL,
                     NULL);
}

static void
gtk_im_context_dbus_real_focus_in (GtkIMContext *context)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (context);

  dbus_context->has_focus = TRUE;

  if (!dbus_context->proxy)
    return;

  call_focus (GTK_IM_CONTEXT_DBUS (context), TRUE);
}

static void
gtk_im_context_dbus_real_focus_out (GtkIMContext *context)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (context);

  dbus_context->has_focus = FALSE;

  if (!dbus_context->proxy)
    return;

  call_focus (GTK_IM_CONTEXT_DBUS (context), FALSE);
}

static void
gtk_im_context_dbus_real_reset (GtkIMContext *context)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (context);

  if (!dbus_context->proxy)
    {
      GTK_IM_CONTEXT_CLASS (parent_class)->reset (context);
      return;
    }

  g_dbus_proxy_call (dbus_context->proxy,
		     "Reset",
		     NULL,
		     G_DBUS_CALL_FLAGS_NONE,
		     -1,
		     dbus_context->cancellable,
		     NULL,
		     NULL);
}

static void
gtk_im_context_dbus_real_dispose (GObject *object)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (object);

  g_clear_object (&dbus_context->cancellable);
  g_clear_object (&dbus_context->connection);
  g_clear_object (&dbus_context->engine_connection);
  g_clear_object (&dbus_context->proxy);
  g_clear_pointer (&dbus_context->preedit_string, g_free);
  g_clear_pointer (&dbus_context->preedit_attrs, pango_attr_list_unref);
}

static void
gtk_im_context_dbus_class_init (GtkIMContextDBusClass *klass)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  im_context_class->get_preedit_string
    = gtk_im_context_dbus_real_get_preedit_string;
  im_context_class->filter_keypress = gtk_im_context_dbus_real_filter_keypress;
  im_context_class->focus_in = gtk_im_context_dbus_real_focus_in;
  im_context_class->focus_out = gtk_im_context_dbus_real_focus_out;
  im_context_class->reset = gtk_im_context_dbus_real_reset;

  /* FIXME: support surrounding */

  object_class->dispose = gtk_im_context_dbus_real_dispose;
}

static void
destroy_ready (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
  GError *error = NULL;
  GVariant *value;

  value = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (!value)
    {
      g_warning ("Cannot destroy engine: %s", error->message);
      g_error_free (error);
      return;
    }

  g_variant_unref (value);
}

static void
call_destroy (GtkIMContextDBus *dbus_context)
{
  g_dbus_proxy_call (dbus_context->proxy,
                     "Destroy",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     destroy_ready,
                     NULL);
}

static void
on_g_signal (GDBusProxy *proxy,
             gchar      *sender_name,
             gchar      *signal_name,
             GVariant   *parameters,
             gpointer    user_data)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);

  if (strcmp (signal_name, "Commit") == 0)
    {
      const gchar *text;

      g_variant_get (parameters, "(&s)", &text);

      g_signal_emit_by_name (dbus_context, "commit", text);
    }
  else if (strcmp (signal_name, "PreeditChanged") == 0)
    {
      const gchar *text = NULL;
      GVariant *styling = NULL;
      GVariantIter iter;
      gint cursor_pos = 0;
      PangoAttrList *attrs;
      guint start_pos, end_pos;
      GInputMethodStylingType styling_type;
      gboolean was_preediting;

      g_variant_get (parameters, "(&s@a(uuu)i)", &text, &styling, &cursor_pos);

      attrs = pango_attr_list_new ();
      g_variant_iter_init (&iter, styling);
      while (g_variant_iter_next (&iter, "(uuu)",
				  &start_pos, &end_pos, &styling_type))
	{
	  switch (styling_type)
	    {
	    case G_INPUT_METHOD_STYLING_NORMAL:
	      continue;

	    case G_INPUT_METHOD_STYLING_UNDERLINE:
	      {
		PangoAttribute *attr;

		attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
		attr->start_index
		  = g_utf8_offset_to_pointer (text, start_pos) - text;
		attr->end_index
		  = g_utf8_offset_to_pointer (text, end_pos) - text;
		pango_attr_list_insert (attrs, attr);
	      }
	      break;

	    case G_INPUT_METHOD_STYLING_SELECTED:
	      {
		PangoAttribute *attr;

		attr = pango_attr_background_new (0xc800, 0xc800, 0xf000);
		attr->start_index
		  = g_utf8_offset_to_pointer (text, start_pos) - text;
		attr->end_index
		  = g_utf8_offset_to_pointer (text, end_pos) - text;
		pango_attr_list_insert (attrs, attr);
		attr = pango_attr_foreground_new (0, 0, 0);
		attr->start_index
		  = g_utf8_offset_to_pointer (text, start_pos) - text;
		attr->end_index
		  = g_utf8_offset_to_pointer (text, end_pos) - text;
		pango_attr_list_insert (attrs, attr);
	      }
	      break;

	    case G_INPUT_METHOD_STYLING_SECONDARY:
	      {
		PangoAttribute *attr;

		attr = pango_attr_background_new (0, 0, 0);
		attr->start_index
		  = g_utf8_offset_to_pointer (text, start_pos) - text;
		attr->end_index
		  = g_utf8_offset_to_pointer (text, end_pos) - text;
		pango_attr_list_insert (attrs, attr);
		attr = pango_attr_foreground_new (0xffff, 0xffff, 0xffff);
		attr->start_index
		  = g_utf8_offset_to_pointer (text, start_pos) - text;
		attr->end_index
		  = g_utf8_offset_to_pointer (text, end_pos) - text;
		pango_attr_list_insert (attrs, attr);
	      }
	      break;

	    default:
	      g_warning ("Unknown styling type: %u", styling_type);
	    }
	}

      was_preediting = dbus_context->preedit_string
	&& *dbus_context->preedit_string == '\0';

      g_clear_pointer (&dbus_context->preedit_string, g_free);
      dbus_context->preedit_string = g_strdup (text);
      g_clear_pointer (&dbus_context->preedit_attrs, pango_attr_list_unref);
      dbus_context->preedit_attrs = attrs;
      dbus_context->preedit_cursor_pos = cursor_pos;

      if (!was_preediting)
	g_signal_emit_by_name (dbus_context, "preedit-start");

      g_signal_emit_by_name (dbus_context, "preedit-changed");

      if (dbus_context->preedit_string
	  && *dbus_context->preedit_string == '\0')
	g_signal_emit_by_name (dbus_context, "preedit-end");
    }
  else if (strcmp (signal_name, "DeleteSurroundingText") == 0)
    {
      gint offset, n_chars;
      gboolean handled;

      g_variant_get (parameters, "(uu)", &offset, &n_chars);
      g_signal_emit_by_name (dbus_context, "delete-surrounding",
			     offset, n_chars, &handled);
    }
}

static void
create_engine_proxy_ready (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);
  GError *error = NULL;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_finish (res, &error);
  if (proxy)
    {
      dbus_context->proxy = proxy;
      g_signal_connect (dbus_context->proxy, "g-signal",
                        G_CALLBACK (on_g_signal), dbus_context);
    }
  else
    {
      g_warning ("Cannot create engine proxy: %s", error->message);
      g_error_free (error);
    }
}

static void
create_engine_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);
  GError *error = NULL;
  GVariant *value;
  const gchar *object_path;
  gchar *name, *p;

  value = g_dbus_connection_call_finish (connection, res, &error);
  if (!value)
    {
      g_warning ("Cannot create engine: %s", error->message);
      g_error_free (error);
      return;
    }

  g_variant_get (value, "(&o)", &object_path, NULL);
  name = g_strdup (object_path + 1);

  /* Deduce the D-Bus name from the object path.  */
  p = strrchr (name, '/');
  *p = '\0';
  for (p = name; *p != '\0'; p++)
    if (*p == '/')
      *p = '.';

  g_dbus_proxy_new (connection,
		    G_DBUS_PROXY_FLAGS_NONE,
		    NULL,
		    name,
		    object_path,
		    DBUS_ENGINE_INTERFACE,
		    dbus_context->cancellable,
		    create_engine_proxy_ready,
		    dbus_context);
  g_free (name);
  g_variant_unref (value);
}

static void
call_create_engine (GtkIMContextDBus *dbus_context)
{
  GVariantBuilder *builder;

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
  g_dbus_connection_call (dbus_context->connection,
			  DBUS_NAME,
			  DBUS_OBJECT_PATH,
			  DBUS_INTERFACE,
			  "CreateEngine",
			  g_variant_new ("(a{sv})", builder),
			  G_VARIANT_TYPE ("(o)"),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  dbus_context->cancellable,
			  create_engine_ready,
			  dbus_context);
}

static void
new_connection_ready (GObject      *source_object,
		      GAsyncResult *res,
		      gpointer      user_data)
{
  GDBusConnection *connection;
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);
  GError *error = NULL;

  connection = g_dbus_connection_new_for_address_finish (res, &error);
  if (!connection)
    {
      g_warning ("Cannot open D-Bus connection: %s",
		 error->message);
      g_error_free (error);
      return;
    }

  dbus_context->engine_connection = connection;
  call_create_engine (dbus_context);
}

static void
get_address_ready (GObject      *source_object,
		   GAsyncResult *res,
		   gpointer      user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);
  GError *error = NULL;
  GVariant *value;
  const gchar *address;

  value = g_dbus_connection_call_finish (connection, res, &error);
  if (!value)
    {
      g_warning ("Cannot get the D-Bus address for engine: %s", error->message);
      g_error_free (error);
      return;
    }

  g_variant_get (value, "(&s)", &address);
  if (strcmp (address, "") == 0)
    {
      dbus_context->engine_connection = g_object_ref (connection);
      call_create_engine (dbus_context);
    }
  else
    g_dbus_connection_new_for_address (address,
				       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT
				       | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
				       NULL,
				       dbus_context->cancellable,
				       new_connection_ready,
				       dbus_context);
  g_variant_unref (value);
}

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);

  /* Ignore input method change if we don't have a focus.  */
  if (dbus_context->proxy && !dbus_context->has_focus)
    return;

  /* Cancel all pending requests.  */
  g_cancellable_cancel (dbus_context->cancellable);

  if (dbus_context->proxy)
    {
      call_destroy (dbus_context);
      dbus_context->proxy = NULL;
    }

  g_cancellable_reset (dbus_context->cancellable);

  g_dbus_connection_call (connection,
			  DBUS_NAME,
			  DBUS_OBJECT_PATH,
			  DBUS_INTERFACE,
			  "GetAddress",
			  NULL,
			  G_VARIANT_TYPE ("(s)"),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  dbus_context->cancellable,
			  get_address_ready,
			  dbus_context);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);

  /* Ignore input method change if we don't have a focus.  */
  if (dbus_context->proxy && !dbus_context->has_focus)
    return;

  /* Cancel all pending requests.  */
  g_cancellable_cancel (dbus_context->cancellable);

  if (dbus_context->proxy)
    {
      call_destroy (dbus_context);
      dbus_context->proxy = NULL;
    }
}

static void
session_bus_get_ready (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GtkIMContextDBus *dbus_context = GTK_IM_CONTEXT_DBUS (user_data);
  GDBusConnection *connection;
  GError *error = NULL;

  connection = g_bus_get_finish (res, &error);
  if (!connection)
    {
      g_warning ("Cannot connect to the session bus: %s", error->message);
      g_error_free (error);
      return;
    }

  g_bus_watch_name_on_connection (connection,
                                  DBUS_NAME,
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  on_name_appeared,
                                  on_name_vanished,
                                  g_object_ref (user_data),
                                  (GDestroyNotify) g_object_unref);

  dbus_context->connection = connection;
}

static void
gtk_im_context_dbus_init (GtkIMContextDBus *dbus_context)
{
  dbus_context->cancellable = g_cancellable_new ();
  dbus_context->preedit_string = g_strdup ("");
  dbus_context->preedit_attrs = pango_attr_list_new ();

  g_bus_get (G_BUS_TYPE_SESSION,
             dbus_context->cancellable,
             session_bus_get_ready,
             dbus_context);
}
