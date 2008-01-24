/*  $Id$
 *
 *  Copyright (c) 2006-2007 Nick Schermer <nick@xfce.org>
 *  Copyright (c)      2007 Mike Massonnet <mmassonnet@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-convenience.h>

#include "clipman.h"
#include "clipman-dialogs.h"



static void                     clipman_plugin_register             (XfcePanelPlugin *panel_plugin);

static ClipmanPlugin *          clipman_plugin_new                  (XfcePanelPlugin *panel_plugin);

static void                     clipman_plugin_load_data            (ClipmanPlugin *clipman_plugin);

static void                     clipman_plugin_save_data            (ClipmanPlugin *clipman_plugin);

static void                     clipman_plugin_free                 (ClipmanPlugin *clipman_plugin);

static gboolean                 clipman_plugin_set_size             (ClipmanPlugin *clipman_plugin,
                                                                     gint size);
static gboolean                 clipman_plugin_button_pressed       (ClipmanPlugin *clipman_plugin,
                                                                     GdkEventButton *event);
static gchar *                  clipman_plugin_get_short_text       (ClipmanPlugin *clipman_plugin,
                                                                     const gchar *text);
static void                     clipman_plugin_menu_new             (ClipmanPlugin *clipman_plugin);

static void                     clipman_plugin_menu_insert_clips    (ClipmanClip *clip,
                                                                     ClipmanPlugin *clipman_plugin);
static void                     clipman_plugin_menu_popup           (ClipmanPlugin *clipman_plugin);

static void                     clipman_plugin_menu_position        (GtkMenu *menu,
                                                                     gint *x,
                                                                     gint *y,
                                                                     gboolean *push_in,
                                                                     gpointer user_data);
static void                     clipman_plugin_menu_destroy         (ClipmanPlugin *clipman_plugin);

static void                     clipman_plugin_menu_item_activate   (GtkWidget *widget,
                                                                     ClipmanPlugin *clipman_plugin);
static gboolean                 clipman_plugin_menu_item_pressed    (GtkWidget *widget,
                                                                     GdkEventButton *event,
                                                                     ClipmanPlugin *clipman_plugin);



static ClipmanClips *           clipman_clips_new                   ();

static void                     clipman_clips_load_data             (ClipmanClips *clipman_clips);

static void                     clipman_clips_save_data             (ClipmanClips *clipman_clips);

static void                     clipman_clips_clear_history         (ClipmanClips *clipman_clips);

static void                     clipman_clips_restore_empty         (ClipmanClips *clipman_clips,
                                                                     ClipboardType type);
static gboolean                 clipman_clips_check_clipboard       (ClipmanClips *clipman_clips);

static inline void              clipman_clips_delete                (ClipmanClips *clipman_clips,
                                                                     ClipmanClip *clip);
static void                     clipman_clips_add                   (ClipmanClips *clipman_clips,
                                                                     gchar *text,
                                                                     ClipboardType type);
static gint                     clipman_clips_compare               (ClipmanClip *one,
                                                                     ClipmanClip *two);
static gint                     clipman_clips_compare_with_type     (ClipmanClip *one,
                                                                     ClipmanClip *two);




XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (clipman_plugin_register);

static void
clipman_plugin_register (XfcePanelPlugin *panel_plugin)
{
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  ClipmanPlugin *clipman_plugin = clipman_plugin_new (panel_plugin);
  g_return_if_fail (G_LIKELY (NULL != clipman_plugin));

  g_signal_connect_swapped (panel_plugin,
                            "save",
                            G_CALLBACK (clipman_plugin_save_data),
                            clipman_plugin);
  g_signal_connect_swapped (panel_plugin,
                            "free-data",
                            G_CALLBACK (clipman_plugin_free),
                            clipman_plugin);
  g_signal_connect_swapped (panel_plugin,
                            "size-changed",
                            G_CALLBACK (clipman_plugin_set_size),
                            clipman_plugin);
  g_signal_connect_swapped (panel_plugin,
                            "configure-plugin",
                            G_CALLBACK (clipman_configure_new),
                            clipman_plugin);

  xfce_panel_plugin_menu_show_configure (panel_plugin);
  xfce_panel_plugin_add_action_widget (panel_plugin, clipman_plugin->button);

  gtk_widget_show_all (clipman_plugin->button);
}

static ClipmanPlugin *
clipman_plugin_new (XfcePanelPlugin *panel_plugin)
{
  ClipmanPlugin *clipman_plugin = g_slice_new0 (ClipmanPlugin);
  clipman_plugin->panel_plugin = panel_plugin;
  clipman_plugin->clipman_clips = clipman_clips_new ();

  clipman_plugin->button = xfce_create_panel_toggle_button ();
  clipman_plugin->icon = gtk_image_new ();

  gtk_container_add (GTK_CONTAINER (clipman_plugin->button), clipman_plugin->icon);
  gtk_container_add (GTK_CONTAINER (panel_plugin), clipman_plugin->button);

  g_signal_connect_swapped (clipman_plugin->button,
                            "button_press_event",
                            G_CALLBACK (clipman_plugin_button_pressed),
                            clipman_plugin);

  clipman_plugin_load_data (clipman_plugin);

  return clipman_plugin;
}

static void
clipman_plugin_load_data (ClipmanPlugin *clipman_plugin)
{
  XfceRc               *rc;
  gchar                *file;

  file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/panel/clipman.rc", TRUE);
  g_return_if_fail (G_LIKELY (NULL != file));
  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  xfce_rc_set_group (rc, "Properties");

  clipman_plugin->menu_item_show_number     = xfce_rc_read_bool_entry   (rc, "ItemNumbers", DEFITEMNUMBERS);
  clipman_plugin->menu_item_max_chars       = xfce_rc_read_int_entry    (rc, "MenuCharacters", DEFCHARS);

  if (clipman_plugin->menu_item_max_chars > MAXCHARS)
    clipman_plugin->menu_item_max_chars = MAXCHARS;
  else if (clipman_plugin->menu_item_max_chars < MINCHARS)
    clipman_plugin->menu_item_max_chars = MINCHARS;

  xfce_rc_close (rc);
}

static void
clipman_plugin_save_data (ClipmanPlugin *clipman_plugin)
{
  XfceRc               *rc;
  gchar                *file;

  file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/panel/clipman.rc", TRUE);
  g_return_if_fail (G_LIKELY (NULL != file));
  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  xfce_rc_set_group (rc, "Properties");

  xfce_rc_write_bool_entry  (rc, "ItemNumbers",     clipman_plugin->menu_item_show_number);
  xfce_rc_write_int_entry   (rc, "MenuCharacters",  clipman_plugin->menu_item_max_chars);

  xfce_rc_close (rc);

  clipman_clips_save_data (clipman_plugin->clipman_clips);
}

static void
clipman_plugin_free (ClipmanPlugin *clipman_plugin)
{
  /* There are no resources to free that won't be after we quit the plugin */
  gtk_main_quit ();
}

static gboolean
clipman_plugin_set_size (ClipmanPlugin *clipman_plugin,
                         gint size)
{
  gtk_widget_set_size_request (GTK_WIDGET (clipman_plugin->panel_plugin), size, size);
  size = size - (2 * MAX (clipman_plugin->button->style->xthickness,
                          clipman_plugin->button->style->xthickness));
  GdkPixbuf *pixbuf = xfce_themed_icon_load ("gtk-paste", size);
  gtk_image_set_from_pixbuf (GTK_IMAGE (clipman_plugin->icon), pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  return TRUE;
}

static gboolean
clipman_plugin_button_pressed (ClipmanPlugin *clipman_plugin,
                               GdkEventButton *event)
{
  if (G_LIKELY (event->button != 1 || event->state & GDK_CONTROL_MASK))
    return FALSE;

  clipman_plugin_menu_popup (clipman_plugin);

  return TRUE;
}

static gchar *
clipman_plugin_get_short_text (ClipmanPlugin *clipman_plugin,
                               const gchar *text)
{
  gchar                *short_text, *tmp = NULL;
  const gchar          *offset;
  gint                  max_length;

  g_return_val_if_fail (G_LIKELY (NULL != text), NULL);
  g_return_val_if_fail (G_LIKELY (g_utf8_validate (text, -1, NULL)), NULL);

  short_text = g_strstrip (g_strdup (text));

  /* Shorten */
  max_length = clipman_plugin->menu_item_max_chars;
  if (g_utf8_strlen (short_text, -1) > max_length)
    {
      offset = g_utf8_offset_to_pointer (short_text, max_length);
      tmp = g_strndup (short_text, offset - short_text);
      g_free (short_text);

      short_text = g_strconcat (tmp, "\342\200\246", NULL); /* Ellipsis U+2026 */
      g_free (tmp);
    }

  /* Cleanup */
  tmp = g_strdelimit (short_text, "\n\r\t", ' ');
  short_text = g_markup_escape_text (tmp, -1);
  g_free (tmp);

  return short_text;
}

static void
clipman_plugin_menu_new (ClipmanPlugin *clipman_plugin)
{
  GtkWidget            *mi = NULL;

  clipman_plugin->menu = gtk_menu_new ();

  /* Add clipboard entries */
  if (g_slist_length (clipman_plugin->clipman_clips->history) > 0)
    g_slist_foreach (clipman_plugin->clipman_clips->history,
                     (GFunc)clipman_plugin_menu_insert_clips,
                     clipman_plugin);
  else
    {
      mi = gtk_menu_item_new_with_label (_("< Clipboard Empty >"));
      gtk_widget_set_sensitive (mi, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (clipman_plugin->menu), mi);
    }

  /* Prepend title */
  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_prepend (GTK_MENU_SHELL (clipman_plugin->menu), mi);

  mi = gtk_menu_item_new_with_label  ("");
  gtk_misc_set_alignment (GTK_MISC (GTK_BIN (mi)->child), 0.5, 0.5);
  gchar *title = g_strdup_printf ("<b>%s</b>", _("Clipman History"));
  gtk_label_set_markup (GTK_LABEL (GTK_BIN (mi)->child), title);
  gtk_widget_set_sensitive (mi, FALSE);
  gtk_menu_shell_prepend (GTK_MENU_SHELL (clipman_plugin->menu), mi);

  /* Append clear history */
  mi = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (clipman_plugin->menu), mi);

  mi = gtk_menu_item_new_with_label (_("Clear History"));
  gtk_menu_shell_append (GTK_MENU_SHELL (clipman_plugin->menu), mi);
  g_signal_connect_swapped (mi,
                            "activate",
                            G_CALLBACK (clipman_clips_clear_history),
                            clipman_plugin->clipman_clips);

  /* Do the rest */
  g_signal_connect_swapped (clipman_plugin->menu,
                            "deactivate",
                            G_CALLBACK (clipman_plugin_menu_destroy),
                            clipman_plugin);

  gtk_menu_attach_to_widget (GTK_MENU (clipman_plugin->menu),
                             clipman_plugin->button,
                             NULL);
  xfce_panel_plugin_register_menu (clipman_plugin->panel_plugin,
                                   GTK_MENU (clipman_plugin->menu));

  gtk_widget_show_all (clipman_plugin->menu);
}

static void
clipman_plugin_menu_insert_clips (ClipmanClip *clip,
                                  ClipmanPlugin *clipman_plugin)
{
  static gint           i = 0, j = 0;
  static ClipmanClip   *clip_default = NULL, *clip_primary = NULL, *clip_last = NULL;
  static gboolean       clip_default_found = FALSE, clip_primary_found = FALSE;
  gchar                *text = NULL, *bold = NULL;
  gint                  a = -1, b = -1;
  GtkWidget            *mi;

  g_return_if_fail (G_LIKELY (NULL != clip));

  /* Generate a short text */
  if (G_UNLIKELY (NULL == clip->short_text))
    clip->short_text = clipman_plugin_get_short_text (clipman_plugin, clip->text);

  /* Generate the menu item text */
  if (clipman_plugin->menu_item_show_number && clipman_plugin->clipman_clips->behavior == STRICTLY)
    text = g_strdup_printf ("%d. %s", (clip->type == PRIMARY) ? i+1 : j+1, clip->short_text);
  else if (clipman_plugin->menu_item_show_number)
    text = g_strdup_printf ("%d. %s", i+1, clip->short_text);
  else
    text = g_strdup (clip->short_text);

  /* Get current clipboards */
  if (G_UNLIKELY (NULL == clip_default))
    {
      clip_default = g_slice_new0 (ClipmanClip);
      clip_primary = g_slice_new0 (ClipmanClip);
      clip_default->text = gtk_clipboard_wait_for_text (clipman_plugin->clipman_clips->default_clipboard);
      clip_primary->text = gtk_clipboard_wait_for_text (clipman_plugin->clipman_clips->primary_clipboard);
      clip_default->type = DEFAULT;
      clip_primary->type = PRIMARY;
      if (G_UNLIKELY (NULL == clip_default->text))
        clip_default->text = g_strdup ("");
      if (G_UNLIKELY (NULL == clip_primary->text))
        clip_primary->text = g_strdup ("");
    }

  /* Check if the clip matches the clipboards */
  if (!clip_default_found || !clip_primary_found)
    {
      switch (clipman_plugin->clipman_clips->behavior)
        {
        case STRICTLY:
          /* Check with default clipboard */
          a = clipman_clips_compare_with_type (clip, clip_default);
          if (a == 0)
            clip_default_found = TRUE;
          /* Check with primary clipboard */
          else
            {
              b = clipman_clips_compare_with_type (clip, clip_primary);
              if (b == 0)
                clip_primary_found = TRUE;
            }
          break;

        default:
        case NORMAL:
          a = clipman_clips_compare (clip, clip_default);
          if (a != 0)
            b = clipman_clips_compare (clip, clip_primary);
          if (a == 0 || b == 0)
            clip_default_found = clip_primary_found = TRUE;
          break;
        }

      /* Set text in bold */
      if (a == 0 || b == 0)
        {
          bold = g_strconcat ("<b>", text, "</b>", NULL);
          g_free (text);
          text = bold;
        }
    }

  /* Create the menu item */
  mi = gtk_menu_item_new_with_label ("");
  g_object_set_data (G_OBJECT (mi), "index", GINT_TO_POINTER (i+j));
  gtk_label_set_markup (GTK_LABEL (GTK_BIN (mi)->child), text);
  g_free (text);

  /* Connect signals */
  g_signal_connect (mi,
                    "activate",
                    G_CALLBACK (clipman_plugin_menu_item_activate),
                    clipman_plugin);
  g_signal_connect (mi,
                    "button_press_event",
                    G_CALLBACK (clipman_plugin_menu_item_pressed),
                    clipman_plugin);

  /* Insert in the menu */
  switch (clipman_plugin->clipman_clips->behavior)
    {
    case STRICTLY:
      /* Insert primary clipboard before default clipboard */
      if (clip->type == PRIMARY && !clipman_plugin->clipman_clips->ignore_primary)
        {
          gtk_menu_shell_insert (GTK_MENU_SHELL (clipman_plugin->menu), mi, i);
          i++;
        }
      /* Append default clipboard */
      else
        {
          gtk_menu_shell_append (GTK_MENU_SHELL (clipman_plugin->menu), mi);
          j++;
        }
      break;

    default:
    case NORMAL:
      /* Append */
      gtk_menu_shell_append (GTK_MENU_SHELL (clipman_plugin->menu), mi);
      i++;
      break;
    }

  /* Get the last clip in the history */
  if (G_UNLIKELY (NULL == clip_last))
    clip_last = (ClipmanClip *)(g_slist_last (clipman_plugin->clipman_clips->history)->data);

  /* Check if the clip matches the last clip */
  if (clip == clip_last)
    {
      if (clipman_plugin->clipman_clips->behavior == STRICTLY)
        {
          /* Check if a clipboard was empty */
          if (!clipman_plugin->clipman_clips->ignore_primary)
            {
              if (i == 0)
                {
                  mi = gtk_menu_item_new_with_label (_("< Selection History Empty >"));
                  gtk_widget_set_sensitive (mi, FALSE);
                  gtk_menu_shell_insert (GTK_MENU_SHELL (clipman_plugin->menu), mi, i++);
                }

              mi = gtk_separator_menu_item_new ();
              gtk_menu_shell_insert (GTK_MENU_SHELL (clipman_plugin->menu), mi, i);
            }

          if (j == 0)
            {
              mi = gtk_menu_item_new_with_label (_("< Default History Empty >"));
              gtk_widget_set_sensitive (mi, FALSE);
              gtk_menu_shell_append (GTK_MENU_SHELL (clipman_plugin->menu), mi);
            }
        }

      /* Free memory */
      g_free (clip_default->text);
      g_free (clip_primary->text);
      g_slice_free (ClipmanClip, clip_default);
      g_slice_free (ClipmanClip, clip_primary);

      /* Unset static variables */
      clip_default = clip_primary = clip_last = NULL;
      clip_default_found = clip_primary_found = FALSE;
      i = j = 0;
    }
}

static void
clipman_plugin_menu_popup (ClipmanPlugin *clipman_plugin)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (clipman_plugin->button)) == TRUE)
    return;

  TRACE ("Set button active");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (clipman_plugin->button), TRUE);
  TRACE ("Create new menu");
  clipman_plugin_menu_new (clipman_plugin);
  TRACE ("Display menu");
  /* if the menu doesn't popup, the next popup will be a bit of b0rked */
  gtk_menu_popup (GTK_MENU (clipman_plugin->menu),
                  NULL,
                  NULL,
                  (GtkMenuPositionFunc)clipman_plugin_menu_position,
                  clipman_plugin->panel_plugin,
                  0,
                  gtk_get_current_event_time ());
}

static void
clipman_plugin_menu_position (GtkMenu *menu,
                              gint *x,
                              gint *y,
                              gboolean *push_in,
                              gpointer user_data)
{
  XfcePanelPlugin      *panel_plugin = user_data;
  GtkWidget            *button;
  GtkRequisition        requisition;
  GtkOrientation        orientation;
     
  g_return_if_fail (GTK_IS_MENU (menu));
  button = gtk_menu_get_attach_widget (menu);
  g_return_if_fail (GTK_IS_WIDGET (button));

  orientation = xfce_panel_plugin_get_orientation (panel_plugin);
  gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
  gdk_window_get_origin (button->window, x, y);

  switch (orientation)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      if (*y + button->allocation.height + requisition.height > gdk_screen_height ())
        /* Show menu above */
        *y -= requisition.height;
      else
        /* Show menu below */
        *y += button->allocation.height;

      if (*x + requisition.width > gdk_screen_width ())
        /* Adjust horizontal position */
        *x = gdk_screen_width () - requisition.width;
      break;
  
    case GTK_ORIENTATION_VERTICAL:
      if (*x + button->allocation.width + requisition.width > gdk_screen_width ())
        /* Show menu on the right */ 
        *x -= requisition.width;
      else
        /* Show menu on the left */
        *x += button->allocation.width;
          
      if (*y + requisition.height > gdk_screen_height ())
        /* Adjust vertical position */
        *y = gdk_screen_height () - requisition.height;
      break;
          
    default:
      break;
    }
}

static void
clipman_plugin_menu_destroy (ClipmanPlugin *clipman_plugin)
{
  if (G_LIKELY (GTK_IS_MENU (clipman_plugin->menu)))
    gtk_menu_detach (GTK_MENU (clipman_plugin->menu));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (clipman_plugin->button), FALSE);
}

static void
clipman_plugin_menu_item_activate (GtkWidget *widget,
                                   ClipmanPlugin *clipman_plugin)
{
  gint                  i;
  ClipmanClip          *clip = NULL;

  g_return_if_fail (G_LIKELY (GTK_IS_WIDGET (widget)));
  i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "index"));
  clip = (ClipmanClip *)g_slist_nth_data (clipman_plugin->clipman_clips->history, i);
  g_return_if_fail (G_LIKELY (NULL != clip));

  DBG ("Copy `%s' to clipboard", clip->text);

  switch (clipman_plugin->clipman_clips->behavior)
    {
    default:
    case NORMAL:
      gtk_clipboard_set_text (clipman_plugin->clipman_clips->default_clipboard, clip->text, -1);
      if (!clipman_plugin->clipman_clips->ignore_primary)
        gtk_clipboard_set_text (clipman_plugin->clipman_clips->primary_clipboard, clip->text, -1);
      break;

    case STRICTLY:
      if (clip->type == PRIMARY)
        gtk_clipboard_set_text (clipman_plugin->clipman_clips->primary_clipboard, clip->text, -1);
      else
        gtk_clipboard_set_text (clipman_plugin->clipman_clips->default_clipboard, clip->text, -1);
      break;
    }
}

static gboolean
clipman_plugin_menu_item_pressed (GtkWidget *widget,
                                  GdkEventButton *event,
                                  ClipmanPlugin *clipman_plugin)
{
  g_return_val_if_fail (G_LIKELY (GTK_IS_WIDGET (widget)), FALSE);

  /* Delete item */
  if (event->button == 3)
    {
      ClipmanClips *clipman_clips = clipman_plugin->clipman_clips;
      gint i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "index"));
      ClipmanClip *clip = (ClipmanClip *)g_slist_nth_data (clipman_clips->history, i);
      g_return_val_if_fail (G_LIKELY (NULL != clip), TRUE);

      GtkWidget *dialog =
        gtk_message_dialog_new (NULL,
                                GTK_DIALOG_MODAL,
                                GTK_MESSAGE_QUESTION,
                                GTK_BUTTONS_YES_NO,
                                _("Are you sure you want to remove this clip from the history?"));
      gint result = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      if (G_UNLIKELY (result != GTK_RESPONSE_YES))
        return TRUE;

      DBG ("Delete `%s' from clipboard (%d)", clip->text, clip->type);

      /* TODO Check current clipboard (write _get_current()? / set widget data "current" = TRUE?) */
      switch (clipman_clips->behavior)
        {
        case STRICTLY:
          if (clip->type == PRIMARY)
            gtk_clipboard_set_text (clipman_clips->primary_clipboard, "", -1);
          else
            gtk_clipboard_set_text (clipman_clips->default_clipboard, "", -1);

        default:
        case NORMAL:
          gtk_clipboard_set_text (clipman_clips->default_clipboard, "", -1);
          gtk_clipboard_set_text (clipman_clips->primary_clipboard, "", -1);
          break;
        }

	  clipman_clips_delete (clipman_clips, clip);

      return TRUE;
    }

  return FALSE;
}




static ClipmanClips *
clipman_clips_new ()
{
  ClipmanClips *clipman_clips = g_slice_new0 (ClipmanClips);

  clipman_clips->default_clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  clipman_clips->primary_clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

  clipman_clips->history = NULL;

  clipman_clips_load_data (clipman_clips);

  clipman_clips->timeout =
    g_timeout_add_full (G_PRIORITY_LOW,
                        TIMER_INTERVAL,
                        (GSourceFunc)clipman_clips_check_clipboard,
                        clipman_clips,
                        (GDestroyNotify)NULL);

  return clipman_clips;
}

static void
clipman_clips_load_data (ClipmanClips *clipman_clips)
{
  XfceRc               *rc;
  gchar                *file;

  file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/panel/clipman.rc", TRUE);
  g_return_if_fail (G_LIKELY (NULL != file));
  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  xfce_rc_set_group (rc, "Properties");

  clipman_clips->behavior       = xfce_rc_read_int_entry    (rc, "Behaviour", NORMAL);
  clipman_clips->history_length = xfce_rc_read_int_entry    (rc, "HistoryItems", DEFHISTORY);
  clipman_clips->save_on_exit   = xfce_rc_read_bool_entry   (rc, "ExitSave", DEFEXITSAVE);
  clipman_clips->ignore_primary = xfce_rc_read_bool_entry   (rc, "IgnoreSelect", DEFIGNORESELECT);
  clipman_clips->prevent_empty  = xfce_rc_read_bool_entry   (rc, "PreventEmpty", DEFPREVENTEMPTY);

  if (clipman_clips->history_length > MAXHISTORY)
    clipman_clips->history_length = MAXHISTORY;
  if (clipman_clips->history_length < MINHISTORY)
    clipman_clips->history_length = MINHISTORY;

  xfce_rc_set_group (rc, "Clips");

  gint clips_length = xfce_rc_read_int_entry (rc, "ClipsLen", 0);
  if (clips_length > MAXHISTORY)
    clips_length = MAXHISTORY;

  if (clipman_clips->save_on_exit && clips_length > 0)
    {
      DBG ("Restoring the clipboard");

      gint              i;
      gchar             name[13];
      gchar            *text = NULL; /* We use this allocation so no free() */
      ClipboardType     type;

      for (i = clips_length - 1; i >= 0; i--)
        {
          g_snprintf (name, 13, "clip_%d_text", i);
          text = g_strdup (xfce_rc_read_entry (rc, name, ""));
          g_snprintf (name, 13, "clip_%d_from", i);
          type = (ClipboardType)xfce_rc_read_int_entry (rc, name, 0);
          clipman_clips_add (clipman_clips, text, type);
        }
    }

  xfce_rc_close (rc);
}

static void
clipman_clips_save_data (ClipmanClips *clipman_clips)
{
  XfceRc               *rc;
  gchar                *file;

  file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/panel/clipman.rc", TRUE);
  g_return_if_fail (G_LIKELY (NULL != file));
  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  xfce_rc_set_group (rc, "Properties");

  xfce_rc_write_bool_entry  (rc, "ExitSave",        clipman_clips->save_on_exit);
  xfce_rc_write_int_entry   (rc, "Behaviour",       clipman_clips->behavior);
  xfce_rc_write_int_entry   (rc, "HistoryItems",    clipman_clips->history_length);
  xfce_rc_write_bool_entry  (rc, "IgnoreSelect",    clipman_clips->ignore_primary);
  xfce_rc_write_bool_entry  (rc, "PreventEmpty",    clipman_clips->prevent_empty);

  xfce_rc_delete_group (rc, "Clips", TRUE);

  if (clipman_clips->save_on_exit && g_slist_length (clipman_clips->history) > 0)
    {
      DBG ("Saving the clipboard history");

      ClipmanClip      *clip;
      gint              i = 0;
      gchar             name[13];

      xfce_rc_set_group (rc, "Clips");
      xfce_rc_write_int_entry (rc, "ClipsLen", g_slist_length (clipman_clips->history));

      while (NULL != (clip = g_slist_nth_data (clipman_clips->history, i)))
        {
          g_snprintf (name, 13, "clip_%d_text", i);
          xfce_rc_write_entry (rc, name, clip->text);
          g_snprintf (name, 13, "clip_%d_from", i);
          xfce_rc_write_int_entry (rc, name, clip->type);
		  i++;
        }
    }

  xfce_rc_close (rc);
}

static void
clipman_clips_clear_history (ClipmanClips *clipman_clips)
{
  gint                  i = 0;
  ClipmanClip          *clip;

  GtkWidget *dialog =
    gtk_message_dialog_new (NULL,
                            GTK_DIALOG_MODAL,
                            GTK_MESSAGE_QUESTION,
                            GTK_BUTTONS_YES_NO,
                            _("Are you sure you want to clear the history?"));
  gint result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  if (G_UNLIKELY (result != GTK_RESPONSE_YES))
    return;

  while (NULL != (clip = (ClipmanClip *)g_slist_nth_data (clipman_clips->history, i++)))
    {
      g_free (clip->text);
      g_free (clip->short_text);
      g_slice_free (ClipmanClip, clip);
    }

  g_slist_free (clipman_clips->history);
  clipman_clips->history = NULL;

  gtk_clipboard_set_text (clipman_clips->default_clipboard, "", -1);
  gtk_clipboard_set_text (clipman_clips->primary_clipboard, "", -1);
}

static void
clipman_clips_restore_empty (ClipmanClips *clipman_clips,
                             ClipboardType type)
{
  if (G_UNLIKELY (g_slist_length (clipman_clips->history) == 0))
    return;

  gint i = 0;
  ClipmanClip *clip = (ClipmanClip *)g_slist_nth_data (clipman_clips->history, i++);
  g_return_if_fail (G_LIKELY (NULL != clip));

  switch (clipman_clips->behavior)
    {
    case STRICTLY:
      while (G_LIKELY (NULL != clip && clip->type != type))
        clip = (ClipmanClip *)g_slist_nth_data (clipman_clips->history, i++);

      if (G_UNLIKELY (NULL == clip))
        return;

      if (type == DEFAULT)
        gtk_clipboard_set_text (clipman_clips->default_clipboard, clip->text, -1);
      else if (!clipman_clips->ignore_primary)
        gtk_clipboard_set_text (clipman_clips->primary_clipboard, clip->text, -1);
      break;

    default:
    case NORMAL:
      gtk_clipboard_set_text (clipman_clips->default_clipboard, clip->text, -1);
      if (!clipman_clips->ignore_primary)
        gtk_clipboard_set_text (clipman_clips->primary_clipboard, clip->text, -1);
      break;
    }
}

static gboolean
clipman_clips_check_clipboard (ClipmanClips *clipman_clips)
{
  GdkModifierType       state;
  gchar                *text;	/* We use the new allocation of
                                 * gtk_clipboard_wait_for_text,
                                 * so no free() on that beotche */

  text = gtk_clipboard_wait_for_text (clipman_clips->default_clipboard);
  if (G_LIKELY (NULL != text))
    clipman_clips_add (clipman_clips, text, DEFAULT);
  else if (clipman_clips->prevent_empty == TRUE)
    clipman_clips_restore_empty (clipman_clips, DEFAULT);

  if (!clipman_clips->ignore_primary)
    {
      gdk_window_get_pointer (NULL, NULL, NULL, &state);
      if (state & GDK_BUTTON1_MASK)
        return TRUE;

      text = gtk_clipboard_wait_for_text (clipman_clips->primary_clipboard);
      if (G_LIKELY (NULL != text))
        clipman_clips_add (clipman_clips, text, PRIMARY);
      else if (clipman_clips->prevent_empty == TRUE)
        clipman_clips_restore_empty (clipman_clips, PRIMARY);
    }

  return TRUE;
}

static inline void
clipman_clips_delete (ClipmanClips *clipman_clips,
					  ClipmanClip *clip)
{
  g_return_if_fail (G_LIKELY (NULL != clip));

  g_free (clip->text);
  g_free (clip->short_text);

  clipman_clips->history = g_slist_remove (clipman_clips->history, clip);
  g_slice_free (ClipmanClip, clip);
}

static void
clipman_clips_add (ClipmanClips *clipman_clips,
                   gchar *text,
                   ClipboardType type)
{
  ClipmanClip          *clip;
  GSList               *foobar = NULL;

  g_return_if_fail (G_LIKELY (NULL != text));

  clip              = g_slice_new (ClipmanClip);
  clip->type        = type;
  clip->text        = text;
  clip->short_text  = NULL;

  if (clipman_clips->behavior == STRICTLY)
    foobar = g_slist_find_custom (clipman_clips->history, clip, (GCompareFunc)clipman_clips_compare_with_type);
  else
    foobar = g_slist_find_custom (clipman_clips->history, clip, (GCompareFunc)clipman_clips_compare);

  if (G_UNLIKELY (NULL != foobar))
    {
      /* Push the existing clip to the top */
      clip = (ClipmanClip *)foobar->data;
      clipman_clips->history = g_slist_remove (clipman_clips->history, clip);
      clipman_clips->history = g_slist_prepend (clipman_clips->history, clip);

      /* Free this allocation that had should be used in the new clip */
      g_free (text);

      return;
    }

  DBG ("Add to clipboard (%d): `%s'", type, text);
  clipman_clips->history = g_slist_prepend (clipman_clips->history, clip);

  if (g_slist_length (clipman_clips->history) > clipman_clips->history_length)
    clipman_clips_delete (clipman_clips, (ClipmanClip *)(g_slist_last (clipman_clips->history)->data));
}

static gint
clipman_clips_compare (ClipmanClip *one,
                       ClipmanClip *two)
{
  g_return_val_if_fail (G_LIKELY (NULL != one && NULL != two), -1);
  return g_ascii_strcasecmp (one->text, two->text);
}

static gint
clipman_clips_compare_with_type (ClipmanClip *one,
                                 ClipmanClip *two)
{
  g_return_val_if_fail (G_LIKELY (NULL != one && NULL != two), -1);
  if (one->type == two->type)
    return g_ascii_strcasecmp (one->text, two->text);
  return -1;
}

