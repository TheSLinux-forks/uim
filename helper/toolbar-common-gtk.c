/*

  Copyright (c) 2003-2006 uim Project http://uim.freedesktop.org/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#include "config.h"

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

#include <uim/uim.h>
#include "uim/uim-helper.h"
#include "uim/uim-compat-scm.h"
#include "uim/gettext.h"

#define OBJECT_DATA_PROP_BUTTONS "PROP_BUTTONS"
#define OBJECT_DATA_TOOL_BUTTONS "TOOL_BUTTONS"
#define OBJECT_DATA_SIZE_GROUP "SIZE_GROUP"
#define OBJECT_DATA_TOOLBAR_TYPE "TOOLBAR_TYPE"
#define OBJECT_DATA_COMMAND "COMMAND"

/* exported functions */
GtkWidget *uim_toolbar_standalone_new(void);
GtkWidget *uim_toolbar_trayicon_new(void);
GtkWidget *uim_toolbar_applet_new(void);
void uim_toolbar_check_helper_connection(GtkWidget *widget);


/* FIXME! command menu and buttons should be customizable. */
static struct _CommandEntry {
  const gchar *desc;
  const gchar *label;
  const gchar *icon;
  const gchar *command;
  const gchar *pref_button_show_symbol;
} command_entry[] = {
  {N_("Switch input method"),
   NULL,
   "switcher-icon",
   "uim-im-switcher-gtk &",
   "toolbar-show-switcher-button?"},

  {N_("Preference"),
   NULL,
   GTK_STOCK_PREFERENCES,
   "uim-pref-gtk &",
   "toolbar-show-pref-button?"},

  {N_("Japanese dictionary editor"),
   "Dic",
   NULL,
   "uim-dict-gtk &",
   "toolbar-show-dict-button?"},

  {N_("Input pad"),
   "Pad",
   NULL,
   "uim-input-pad-ja &",
   "toolbar-show-input-pad-button?"},

  {N_("Handwriting input pad"),
   "Hand",
   NULL,
   "uim-tomoe-gtk &",
   "toolbar-show-handwriting-input-pad-button?"},

  {N_("Help"),
   NULL,
   GTK_STOCK_HELP,
   "uim-help &",
   "toolbar-show-help-button?"}
};
static gint command_entry_len = sizeof(command_entry) / sizeof(struct _CommandEntry);

static GtkWidget *prop_menu;
static GtkWidget *right_click_menu;
static unsigned int read_tag;
static int uim_fd;

enum {
  TYPE_STANDALONE,
  TYPE_APPLET,
  TYPE_ICON
};


static void
calc_menu_position(GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
		   GtkWidget *button)
{
  gint sc_height, sc_width, menu_width, menu_height, button_height;
  GtkRequisition requisition;
  
  g_return_if_fail(x && y);
  g_return_if_fail(GTK_IS_BUTTON(button));
  
  gdk_window_get_origin(button->window, x, y);
  gdk_drawable_get_size(button->window, NULL, &button_height);
  
  sc_height = gdk_screen_get_height(gdk_screen_get_default());
  sc_width = gdk_screen_get_width(gdk_screen_get_default());
  
  gtk_widget_size_request(GTK_WIDGET(menu), &requisition);
  
  menu_width = requisition.width;
  menu_height = requisition.height;
  
  if (*y + button_height + menu_height < sc_height)
    *y = *y + button_height;
  else
    *y = *y - menu_height;
  
  if (*x + menu_width > sc_width)
    *x = sc_width - menu_width;
}

static void
right_click_menu_quit_activated(GtkMenu *menu_item, gpointer data)
{
  gtk_main_quit();
}

static void
right_click_menu_activated(GtkMenu *menu_item, gpointer data)
{
  const char *command = data;

  if (command)
    system(command);
}

static gboolean
right_button_pressed(GtkButton *button, GdkEventButton *event, gpointer data)
{
  gtk_menu_popup(GTK_MENU(right_click_menu), NULL, NULL, 
		 (GtkMenuPositionFunc)calc_menu_position,
		 (gpointer)button, event->button, 
		 gtk_get_current_event_time());

  return FALSE;
}

static void
prop_menu_activate(GtkMenu *menu_item, gpointer data)
{
  GString *msg;
  
  msg = g_string_new((gchar*)g_object_get_data(G_OBJECT(menu_item),
		     "prop_action"));
  g_string_prepend(msg, "prop_activate\n");
  g_string_append(msg, "\n");
  uim_helper_send_message(uim_fd, msg->str);
  g_string_free(msg, TRUE);
}

static void
popup_prop_menu(GtkButton *prop_button, GdkEventButton *event,
		GtkWidget *widget)
{
  GtkWidget *menu_item;
  GtkTooltips *tooltip;
  GList *menu_item_list, *label_list, *tooltip_list, *action_list, *state_list;
  gchar *label, *flag;
  int i;
  gboolean has_state = FALSE;

  uim_toolbar_check_helper_connection(widget);

  menu_item_list = gtk_container_get_children(GTK_CONTAINER(prop_menu));
  label_list = g_object_get_data(G_OBJECT(prop_button), "prop_label");
  tooltip_list = g_object_get_data(G_OBJECT(prop_button), "prop_tooltip");
  action_list = g_object_get_data(G_OBJECT(prop_button), "prop_action");
  state_list = g_object_get_data(G_OBJECT(prop_button), "prop_state");

  i = 0;
  while ((menu_item = g_list_nth_data(menu_item_list, i)) != NULL) {    
    gtk_widget_destroy(menu_item);
    i++;
  }

  /* check if state_list contains state data */
  i = 0;
  while ((flag = g_list_nth_data(state_list, i)) != NULL) {
    if (!strcmp("*", flag)) {
      has_state = TRUE;
      break;
    }
    i++;
  }

  i = 0;
  while ((label = g_list_nth_data(label_list, i)) != NULL) {
    if (has_state) {
      menu_item = gtk_check_menu_item_new_with_label(label);
#if GTK_CHECK_VERSION(2, 4, 0)
      gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(menu_item),
					    TRUE);
#endif
      flag = g_list_nth_data(state_list, i);
      if (flag && !strcmp("*", flag))
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
    } else {
      menu_item = gtk_menu_item_new_with_label(label);
    }

    /* tooltips */
    tooltip = gtk_tooltips_new();
    gtk_tooltips_set_tip(tooltip, menu_item, g_list_nth_data(tooltip_list, i),
			 NULL);
    
    /* add to the menu */
    gtk_menu_shell_append(GTK_MENU_SHELL(prop_menu), menu_item);

    gtk_widget_show(menu_item);
    g_signal_connect(G_OBJECT(menu_item), "activate", 
		     G_CALLBACK(prop_menu_activate), prop_menu);
    g_object_set_data(G_OBJECT(menu_item), "prop_action",
		      g_list_nth_data(action_list, i));
    i++;
  }

  gtk_menu_popup(GTK_MENU(prop_menu), NULL, NULL, 
		 (GtkMenuPositionFunc)calc_menu_position,
		 (gpointer)prop_button, event->button, 
		 gtk_get_current_event_time());
}

static gboolean
prop_button_pressed(GtkButton *prop_button, GdkEventButton *event,
		    GtkWidget *widget)
{ 
  gint type;

  switch (event->button) {
  case 3:
    type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
			   OBJECT_DATA_TOOLBAR_TYPE));
    if (type == TYPE_APPLET)
      gtk_propagate_event(gtk_widget_get_parent(GTK_WIDGET(prop_button)),
		      				(GdkEvent *)event);
    else
      right_button_pressed(prop_button, event, widget);
    break;
  case 2:
    gtk_propagate_event(gtk_widget_get_parent(GTK_WIDGET(prop_button)),
			(GdkEvent *) event);
    break;
  default:
    popup_prop_menu(prop_button, event, widget);
    break;
  }

  return FALSE;
}

static gboolean
prop_button_released(GtkButton *prop_button, GdkEventButton *event,
		     GtkWidget *widget)
{
  switch (event->button) {
  case 2:
  case 3:
    gtk_propagate_event(gtk_widget_get_parent(GTK_WIDGET(prop_button)),
			(GdkEvent *)event);
    break;
  default:
    break;
  }

  return FALSE;
}

static gboolean
tool_button_pressed_cb(GtkButton *tool_button, GdkEventButton *event,
		       GtkWidget *widget)
{ 
  gint type;
  
  switch (event->button) {
  case 3:
    type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
			   OBJECT_DATA_TOOLBAR_TYPE));
    if (type == TYPE_APPLET)
      gtk_propagate_event(gtk_widget_get_parent(GTK_WIDGET(tool_button)),
			  (GdkEvent *)event);
    else
      right_button_pressed(tool_button, event, NULL);
    break;
  case 2:
    gtk_propagate_event(gtk_widget_get_parent(GTK_WIDGET(tool_button)),
			(GdkEvent *)event);
    break;
  default:
    break;
  }

  return FALSE;
}

static void
tool_button_clicked_cb(GtkButton *tool_button, GtkWidget *widget)
{ 
  const gchar *command;
  
  command = g_object_get_data(G_OBJECT(tool_button), OBJECT_DATA_COMMAND);
  if (command)
    system(command);
}


static void
list_data_free(GList *list)
{
  g_list_foreach(list, (GFunc)g_free, NULL);
  g_list_free(list);
}

static void
prop_data_flush(gpointer data)
{
  GList *list;
  list = g_object_get_data(data, "prop_label");
  list_data_free(list);
  list = g_object_get_data(data, "prop_tooltip");
  list_data_free(list);
  list = g_object_get_data(data, "prop_action");
  list_data_free(list);
  list = g_object_get_data(data, "prop_state");
  list_data_free(list);

  g_object_set_data(G_OBJECT(data), "prop_label", NULL);
  g_object_set_data(G_OBJECT(data), "prop_tooltip", NULL);
  g_object_set_data(G_OBJECT(data), "prop_action", NULL);
  g_object_set_data(G_OBJECT(data), "prop_state", NULL);  
}

static void
prop_button_destroy(gpointer data, gpointer user_data)
{
  prop_data_flush(data);
  gtk_widget_destroy(GTK_WIDGET(data));
}

static void
tool_button_destroy(gpointer data, gpointer user_data)
{
  gtk_widget_destroy(GTK_WIDGET(data));
}

static GtkWidget *
prop_button_create(GtkWidget *widget, const gchar *label,
		   const gchar *tip_text)
{
  GtkWidget *button;
  GtkTooltips *tooltip;
  GtkSizeGroup *sg;
  
  sg = g_object_get_data(G_OBJECT(widget), OBJECT_DATA_SIZE_GROUP);
  button = gtk_button_new_with_label(label);
  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_size_group_add_widget(sg, button);
  tooltip = gtk_tooltips_new();
  gtk_tooltips_set_tip(tooltip, button, tip_text, NULL);

  g_signal_connect(G_OBJECT(button), "button-press-event",
		   G_CALLBACK(prop_button_pressed), widget);
  g_signal_connect(G_OBJECT(button), "button-release-event",
		   G_CALLBACK(prop_button_released), widget);

  return button;
}

static void
prop_button_append_menu(GtkWidget *button,
			const gchar *label, const gchar *tooltip,
			const gchar *action, const gchar *state)
{
  GList *label_list = g_object_get_data(G_OBJECT(button), "prop_label");
  GList *tooltip_list = g_object_get_data(G_OBJECT(button), "prop_tooltip");
  GList *action_list = g_object_get_data(G_OBJECT(button), "prop_action");
  
  label_list = g_list_append(label_list, g_strdup(label));
  tooltip_list = g_list_append(tooltip_list, g_strdup(tooltip));
  action_list = g_list_append(action_list, g_strdup(action));
  
  g_object_set_data(G_OBJECT(button), "prop_label", label_list);
  g_object_set_data(G_OBJECT(button), "prop_tooltip", tooltip_list);
  g_object_set_data(G_OBJECT(button), "prop_action", action_list);

  if (state) {
    GList *state_list = g_object_get_data(G_OBJECT(button), "prop_state");
    state_list = g_list_append(state_list, g_strdup(state));
    g_object_set_data(G_OBJECT(button), "prop_state", state_list);
  }
}

static void
append_prop_button(GtkWidget *hbox, GtkWidget *button)
{
  GList *prop_buttons;

  if (button) {
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

    prop_buttons = g_object_get_data(G_OBJECT(hbox), OBJECT_DATA_PROP_BUTTONS);
    prop_buttons = g_list_append(prop_buttons, button);
    g_object_set_data(G_OBJECT(hbox), OBJECT_DATA_PROP_BUTTONS, prop_buttons);
  }
}

static void
append_tool_button(GtkWidget *hbox, GtkWidget *button)
{
  GList *tool_buttons;

  if (button) {
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

    tool_buttons = g_object_get_data(G_OBJECT(hbox), OBJECT_DATA_TOOL_BUTTONS);
    tool_buttons = g_list_append(tool_buttons, button);
    g_object_set_data(G_OBJECT(hbox), OBJECT_DATA_TOOL_BUTTONS, tool_buttons);
  }
}


static gchar *
get_charset(gchar *line)
{
  gchar **tokens;
  gchar *charset = NULL;

  tokens = g_strsplit(line, "=", 0);
  if (tokens && tokens[0] && tokens[1] && !strcmp("charset", tokens[0]))
    charset = g_strdup(tokens[1]);
  g_strfreev(tokens);
  
  return charset;
}

static gchar *
convert_charset(const gchar *charset, const gchar *str)
{
  if (!charset)
    return NULL;

  return g_convert(str, strlen(str),
		   "UTF-8", charset, 
		   NULL, /* gsize *bytes_read */
		   NULL, /* size *bytes_written */
		   NULL); /* GError **error */
}

static void
helper_toolbar_prop_list_update(GtkWidget *widget, gchar **lines)
{
  GtkWidget *button = NULL;
  int i;
  gchar **cols;
  gchar *charset = NULL;
  GList *prop_buttons, *tool_buttons;
  GtkSizeGroup *sg;

  charset = get_charset(lines[1]);

  prop_buttons = g_object_get_data(G_OBJECT(widget), OBJECT_DATA_PROP_BUTTONS);
  tool_buttons = g_object_get_data(G_OBJECT(widget), OBJECT_DATA_TOOL_BUTTONS);
  sg  = g_object_get_data(G_OBJECT(widget), OBJECT_DATA_SIZE_GROUP);

  if (prop_buttons) {
    g_list_foreach(prop_buttons, prop_button_destroy, NULL);
    g_list_free(prop_buttons);
    g_object_set_data(G_OBJECT(widget), OBJECT_DATA_PROP_BUTTONS, NULL);
  }
  if (tool_buttons) {
    g_list_foreach(tool_buttons, tool_button_destroy, NULL);
    g_list_free(tool_buttons);
    g_object_set_data(G_OBJECT(widget), OBJECT_DATA_TOOL_BUTTONS, NULL);
  }

  i = 0;
  while (lines[i] && strcmp("", lines[i])) {
    gchar *utf8_str = convert_charset(charset, lines[i]);

    if (utf8_str != NULL) {
      cols = g_strsplit(utf8_str, "\t", 0);
      g_free(utf8_str);
    } else {
      cols = g_strsplit(lines[i], "\t", 0);
    }
    
    if (cols && cols[0]) {
      if (!strcmp("branch", cols[0])) {
	button = prop_button_create(widget, cols[1], cols[2]);
	append_prop_button(widget, button);
      } else if (!strcmp("leaf", cols[0])) {
	prop_button_append_menu(button, cols[2], cols[3], cols[4], cols[5]);
      }
      g_strfreev(cols);
    }
    i++;
  }
  
  /* create tool buttons */
  /* FIXME! command menu and buttons should be customizable. */
  for (i = 0; i < command_entry_len; i++) {
    GtkWidget *tool_button;
    GtkTooltips *tooltip;
    GtkWidget *img;
    uim_bool show_pref;

    show_pref = uim_scm_symbol_value_bool(command_entry[i].pref_button_show_symbol);
    if (show_pref == UIM_FALSE)
      continue;

    tool_button = gtk_button_new();
    g_object_set_data(G_OBJECT(tool_button), OBJECT_DATA_COMMAND,
		      (gpointer)command_entry[i].command);
    if (command_entry[i].icon)
      img = gtk_image_new_from_stock(command_entry[i].icon,
				     GTK_ICON_SIZE_MENU);
    else {
      img = gtk_label_new("");
      gtk_label_set_markup(GTK_LABEL(img), command_entry[i].label);
    }
    if (img)
      gtk_container_add(GTK_CONTAINER(tool_button), img);
    gtk_button_set_relief(GTK_BUTTON(tool_button), GTK_RELIEF_NONE);
    gtk_size_group_add_widget(sg, tool_button);
    g_signal_connect(G_OBJECT(tool_button), "button-press-event",
		     G_CALLBACK(tool_button_pressed_cb), widget);
    g_signal_connect(G_OBJECT(tool_button), "clicked",
		     G_CALLBACK(tool_button_clicked_cb), widget);

    /* tooltip */
    tooltip = gtk_tooltips_new();
    gtk_tooltips_set_tip(tooltip, tool_button, _(command_entry[i].desc), NULL);

    append_tool_button(widget, tool_button);
  }

  gtk_widget_show_all(widget);
  g_free(charset);
}

static void
helper_toolbar_prop_label_update(GtkWidget *widget, gchar **lines)
{
  GtkWidget *button;
  unsigned int i;
  gchar **pair;
  gchar *charset = NULL;
  GList *prop_buttons;

  i = 0;
  while (lines[i] && strcmp("", lines[i]))
    i++;

  prop_buttons = g_object_get_data(G_OBJECT(widget), OBJECT_DATA_PROP_BUTTONS);
  if (!prop_buttons || (i - 2) != g_list_length(prop_buttons)) {
    uim_helper_client_get_prop_list();
    return;
  }
  
  charset = get_charset(lines[1]);

  i = 2;
  while (lines[i] && strcmp("", lines[i])) {
    if (charset) {
      gchar *utf8_str;
      utf8_str = g_convert(lines[i], strlen(lines[i]),
			   "UTF-8", charset,
			   NULL, /* gsize *bytes_read */
			   NULL, /*size *bytes_written */
			   NULL); /* GError **error*/
      pair = g_strsplit(utf8_str, "\t", 0);
      g_free(utf8_str);
    } else {
      pair = g_strsplit(lines[i], "\t", 0);
    }
    
    if (pair && pair[0] && pair[1]) {
      button = g_list_nth_data(prop_buttons, i - 2 );
      gtk_button_set_label(GTK_BUTTON(button), pair[0]);
    }
    g_strfreev(pair);
    i++;
  }

  g_free(charset);
}

static void
helper_toolbar_parse_helper_str(GtkWidget *widget, gchar *str)
{
  gchar **lines;
  lines = g_strsplit(str, "\n", 0);

  if (lines && lines[0]) {
    if (!strcmp("prop_list_update", lines[0]))
      helper_toolbar_prop_list_update(widget, lines);
    else if (!strcmp("prop_label_update", lines[0]))
      helper_toolbar_prop_label_update(widget, lines);
    g_strfreev(lines);
  }
}

static gboolean
fd_read_cb(GIOChannel *channel, GIOCondition c, gpointer p)
{
  gchar *msg;
  int fd = g_io_channel_unix_get_fd(channel);
  GtkWidget *widget = GTK_WIDGET(p);

  uim_helper_read_proc(fd);

  while ((msg = uim_helper_get_message())) {
    helper_toolbar_parse_helper_str(widget, msg);
    free(msg);
  }

  return TRUE;
}

static void
helper_disconnect_cb(void)
{
  uim_fd = -1;
  g_source_remove(read_tag);
}

void
uim_toolbar_check_helper_connection(GtkWidget *widget)
{
  if (uim_fd < 0) {
    uim_fd = uim_helper_init_client_fd(helper_disconnect_cb);
    if (uim_fd > 0) {
      GIOChannel *channel;
      channel = g_io_channel_unix_new(uim_fd);
      read_tag = g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR,
				fd_read_cb, (gpointer)widget);
      g_io_channel_unref(channel);
    }
  }
}

static GtkWidget *
right_click_menu_create(void)
{
  GtkWidget *menu;
  GtkWidget *menu_item;
  GtkWidget *img;
  gint i;

  menu = gtk_menu_new();

  /* FIXME! command menu and buttons should be customizable. */
  for (i = 0; i < command_entry_len; i++) {
    menu_item = gtk_image_menu_item_new_with_label(_(command_entry[i].desc));

    if (command_entry[i].icon) {
      img = gtk_image_new_from_stock(command_entry[i].icon,
		      		     GTK_ICON_SIZE_MENU);
      if (img)
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), img);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect(G_OBJECT(menu_item), "activate", 
		     G_CALLBACK(right_click_menu_activated),
		     (gpointer)command_entry[i].command);
  }

  /* Add quit item */
  img = gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU);
  menu_item = gtk_image_menu_item_new_with_label(_("Quit this toolbar"));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), img);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  g_signal_connect(G_OBJECT(menu_item), "activate", 
		   G_CALLBACK(right_click_menu_quit_activated), NULL);

  gtk_widget_show_all(menu);

  return menu;
}

static void
regist_icon(void)
{
  GtkIconFactory *factory;
  GtkIconSet *icon_set;
  GdkPixbuf *pixbuf;

  factory = gtk_icon_factory_new();
  gtk_icon_factory_add_default(factory);
  pixbuf = gdk_pixbuf_new_from_file(UIM_PIXMAPSDIR "/switcher-icon.png", NULL);
  icon_set = gtk_icon_set_new_from_pixbuf(pixbuf);
  gtk_icon_factory_add(factory, "switcher-icon", icon_set);

  gtk_icon_set_unref(icon_set);
  g_object_unref(G_OBJECT(pixbuf));
  g_object_unref(G_OBJECT (factory));
}

static GtkWidget *
toolbar_new(gint type)
{
  GtkWidget *button;
  GtkWidget *hbox;
  GList *prop_buttons = NULL;
  GtkSizeGroup *sg;

  regist_icon();

  /* create widgets */
  hbox = gtk_hbox_new(FALSE, 0);
  prop_menu = gtk_menu_new();
  right_click_menu = right_click_menu_create();
  sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  
  button = gtk_button_new_with_label(" x");
  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect(G_OBJECT(button), "button-press-event",
		   G_CALLBACK(prop_button_pressed), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  prop_buttons = g_list_append(prop_buttons, button);

  g_object_set_data(G_OBJECT(hbox), OBJECT_DATA_PROP_BUTTONS, prop_buttons);
  g_object_set_data(G_OBJECT(hbox), OBJECT_DATA_SIZE_GROUP, sg);
  g_object_set_data(G_OBJECT(hbox), OBJECT_DATA_TOOLBAR_TYPE,
		    GINT_TO_POINTER(type));
  
  uim_fd = -1;

  if (type != TYPE_ICON) {
    /* delay initialization until getting "embedded" signal */
    uim_toolbar_check_helper_connection(hbox);
    uim_helper_client_get_prop_list();
  }

  return hbox; 
}

GtkWidget *
uim_toolbar_standalone_new(void)
{
  return toolbar_new(TYPE_STANDALONE);
}

GtkWidget *
uim_toolbar_applet_new(void)
{
  return toolbar_new(TYPE_APPLET);
}

GtkWidget *
uim_toolbar_trayicon_new(void)
{
  return toolbar_new(TYPE_ICON);
}
