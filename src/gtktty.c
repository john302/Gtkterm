/*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*********************************************************************
* Description: Gtk Terminal.
* Author: John Cartwright <>
* Created at: Mon Mar 17 12:02:04 AEDT 2025
* Computer: buddhism
* System: Linux 5.14.0-503.29.1.el9_5.x86_64 on x86_64
*
* Copyright (c) 2025 John Cartwright  All rights reserved.
*
********************************************************************/

#define SCROLLBACK 500 /* Scrollback */

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdlib.h>
#include <string.h>

// Function declarations for copy-paste operations
static void copy_text(GtkWidget *widget, gpointer data);
static void paste_text(GtkWidget *widget, gpointer data);
static void select_all(GtkWidget *widget, gpointer data);
static void clear_terminal(GtkWidget *widget, gpointer data);
static void show_context_menu(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void show_command_builder(GtkWidget *widget, gpointer data);
static void on_command_type_changed(GtkComboBox *combo, gpointer data);
static void on_build_command(GtkWidget *widget, gpointer data);
static void update_preview(GtkWidget *widget, gpointer data);

// Structure to hold command builder dialog widgets
typedef struct {
    GtkWidget *dialog;
    GtkWidget *command_type_combo;
    GtkWidget *options_box;
    GtkWidget *preview_entry;
    GtkWidget *input_entry;
    GtkWidget *pattern_entry;
    GtkWidget *output_format_entry;
    VteTerminal *terminal;
} CommandBuilderData;

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

static void on_child_exited(VteTerminal *terminal, gint status, gpointer data) {
    gtk_main_quit();
}

static void copy_text(GtkWidget *widget, gpointer data) {
    VteTerminal *terminal = VTE_TERMINAL(data);
    vte_terminal_copy_clipboard(terminal);
}

static void paste_text(GtkWidget *widget, gpointer data) {
    VteTerminal *terminal = VTE_TERMINAL(data);
    vte_terminal_paste_clipboard(terminal);
}

static void select_all(GtkWidget *widget, gpointer data) {
    VteTerminal *terminal = VTE_TERMINAL(data);
    vte_terminal_select_all(terminal);
}

static void clear_terminal(GtkWidget *widget, gpointer data) {
    VteTerminal *terminal = VTE_TERMINAL(data);
    vte_terminal_reset(terminal, TRUE, TRUE);
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    VteTerminal *terminal = VTE_TERMINAL(data);
    
    // Check for Ctrl+A (Select All)
    if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_a) {
        select_all(NULL, terminal);
        return TRUE;
    }
    
    // Check for Ctrl+L (Clear)
    if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_l) {
        clear_terminal(NULL, terminal);
        return TRUE;
    }
    
    return FALSE;
}

static void show_context_menu(GtkWidget *widget, GdkEventButton *event, gpointer data) { /* Context menu */
    if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY) {
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *copy_item = gtk_menu_item_new_with_label("Copy");
        GtkWidget *paste_item = gtk_menu_item_new_with_label("Paste");
        GtkWidget *select_all_item = gtk_menu_item_new_with_label("Select All");
        GtkWidget *clear_item = gtk_menu_item_new_with_label("Clear");
        
        g_signal_connect(copy_item, "activate", G_CALLBACK(copy_text), data);
        g_signal_connect(paste_item, "activate", G_CALLBACK(paste_text), data);
        g_signal_connect(select_all_item, "activate", G_CALLBACK(select_all), data);
        g_signal_connect(clear_item, "activate", G_CALLBACK(clear_terminal), data);
        
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), select_all_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), clear_item);
        
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
    }
}

static void setup_environment() {
    // Set PROMPT_COMMAND to display its output before each prompt
    g_setenv("PROMPT_COMMAND", "printf \"\\033]0;${USER}@${HOSTNAME}:${PWD}\\007\"", TRUE);
    
    // Alternatively, to both keep existing PROMPT_COMMAND and display its output:
    // g_setenv("PROMPT_COMMAND", "existing_command; printf \"\\nPROMPT_COMMAND OUTPUT: $(existing_command)\\n\"", TRUE);
}

static void show_command_builder(GtkWidget *widget, gpointer data) { /* Command builder dialog */
    VteTerminal *terminal = VTE_TERMINAL(data);
    CommandBuilderData *builder_data = g_malloc(sizeof(CommandBuilderData));
    builder_data->terminal = terminal;

    // Create dialog
    builder_data->dialog = gtk_dialog_new_with_buttons("Command Builder", /* Dialog */
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(terminal))),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Build Command", GTK_RESPONSE_ACCEPT,
        "Cancel", GTK_RESPONSE_CANCEL,
        NULL);

    // Create main content box
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); /* Content box */
    gtk_container_set_border_width(GTK_CONTAINER(content_box), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(builder_data->dialog))),
        content_box, TRUE, TRUE, 0);

    // Command type selection
    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), type_box, FALSE, FALSE, 0);
    
    GtkWidget *type_label = gtk_label_new("Command Type:");
    gtk_box_pack_start(GTK_BOX(type_box), type_label, FALSE, FALSE, 0);
    
    builder_data->command_type_combo = gtk_combo_box_text_new(); /* Command type combo */
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(builder_data->command_type_combo), "grep");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(builder_data->command_type_combo), "awk");
    gtk_combo_box_set_active(GTK_COMBO_BOX(builder_data->command_type_combo), 0);
    gtk_box_pack_start(GTK_BOX(type_box), builder_data->command_type_combo, TRUE, TRUE, 0);

    // Options box (will be updated based on command type)
    builder_data->options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), builder_data->options_box, TRUE, TRUE, 0);

    // Input file/command
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), input_box, FALSE, FALSE, 0);
    
    GtkWidget *input_label = gtk_label_new("Input:");
    gtk_box_pack_start(GTK_BOX(input_box), input_label, FALSE, FALSE, 0);
    
    builder_data->input_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(input_box), builder_data->input_entry, TRUE, TRUE, 0);

    // Pattern/Expression
    GtkWidget *pattern_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), pattern_box, FALSE, FALSE, 0);
    
    GtkWidget *pattern_label = gtk_label_new("Pattern/Expression:");
    gtk_box_pack_start(GTK_BOX(pattern_box), pattern_label, FALSE, FALSE, 0);
    
    builder_data->pattern_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(pattern_box), builder_data->pattern_entry, TRUE, TRUE, 0);

    // Output format (for awk)
    GtkWidget *format_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), format_box, FALSE, FALSE, 0);
    
    GtkWidget *format_label = gtk_label_new("Output Format:");
    gtk_box_pack_start(GTK_BOX(format_box), format_label, FALSE, FALSE, 0);
    
    builder_data->output_format_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(format_box), builder_data->output_format_entry, TRUE, TRUE, 0);

    // Preview
    GtkWidget *preview_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(content_box), preview_box, FALSE, FALSE, 0);
    
    GtkWidget *preview_label = gtk_label_new("Preview:");
    gtk_box_pack_start(GTK_BOX(preview_box), preview_label, FALSE, FALSE, 0);
    
    builder_data->preview_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(builder_data->preview_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(preview_box), builder_data->preview_entry, TRUE, TRUE, 0);

    // Connect signals
    g_signal_connect(builder_data->command_type_combo, "changed",
        G_CALLBACK(on_command_type_changed), builder_data);
    g_signal_connect(builder_data->input_entry, "changed",
        G_CALLBACK(update_preview), builder_data);
    g_signal_connect(builder_data->pattern_entry, "changed",
        G_CALLBACK(update_preview), builder_data);
    g_signal_connect(builder_data->output_format_entry, "changed",
        G_CALLBACK(update_preview), builder_data);

    // Show dialog
    gtk_widget_show_all(builder_data->dialog);
    gint response = gtk_dialog_run(GTK_DIALOG(builder_data->dialog));

    if (response == GTK_RESPONSE_ACCEPT) {
        const char *command = gtk_entry_get_text(GTK_ENTRY(builder_data->preview_entry));
        vte_terminal_feed_child(VTE_TERMINAL(terminal), command, strlen(command));
        vte_terminal_feed_child(VTE_TERMINAL(terminal), "\n", 1);
    }

    gtk_widget_destroy(builder_data->dialog);
    g_free(builder_data);
}

static void on_command_type_changed(GtkComboBox *combo, gpointer data) {
    CommandBuilderData *builder_data = (CommandBuilderData *)data;
    GtkWidget *options_box = builder_data->options_box;
    
    // Clear existing options
    GList *children = gtk_container_get_children(GTK_CONTAINER(options_box));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    const char *command_type = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(builder_data->command_type_combo));

    if (strcmp(command_type, "grep") == 0) {
        // Add grep-specific options
        GtkWidget *case_sensitive = gtk_check_button_new_with_label("Case sensitive");
        gtk_box_pack_start(GTK_BOX(options_box), case_sensitive, FALSE, FALSE, 0);
        
        GtkWidget *invert_match = gtk_check_button_new_with_label("Invert match");
        gtk_box_pack_start(GTK_BOX(options_box), invert_match, FALSE, FALSE, 0);
        
        GtkWidget *line_number = gtk_check_button_new_with_label("Show line numbers");
        gtk_box_pack_start(GTK_BOX(options_box), line_number, FALSE, FALSE, 0);
    } else if (strcmp(command_type, "awk") == 0) {
        // Add awk-specific options
        GtkWidget *field_separator = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(field_separator), "Field separator (default: space)");
        gtk_box_pack_start(GTK_BOX(options_box), field_separator, FALSE, FALSE, 0);
        
        GtkWidget *output_separator = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(output_separator), "Output separator (default: space)");
        gtk_box_pack_start(GTK_BOX(options_box), output_separator, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(options_box);
    update_preview(NULL, data);
}

static void update_preview(GtkWidget *widget, gpointer data) { /* Command builder preview */
    CommandBuilderData *builder_data = (CommandBuilderData *)data;
    const char *command_type = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(builder_data->command_type_combo));
    const char *input = gtk_entry_get_text(GTK_ENTRY(builder_data->input_entry));
    const char *pattern = gtk_entry_get_text(GTK_ENTRY(builder_data->pattern_entry));
    const char *format = gtk_entry_get_text(GTK_ENTRY(builder_data->output_format_entry));

    GString *preview = g_string_new("");
    
    if (strcmp(command_type, "grep") == 0) { /* Grep command */
        g_string_append(preview, "grep ");
        
        // Add options from checkboxes
        GList *children = gtk_container_get_children(GTK_CONTAINER(builder_data->options_box));
        for (GList *l = children; l != NULL; l = l->next) {
            if (GTK_IS_CHECK_BUTTON(l->data) && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(l->data))) {
                const char *label = gtk_button_get_label(GTK_BUTTON(l->data));
                if (strcmp(label, "Case sensitive") == 0) {
                    g_string_append(preview, "-i ");
                } else if (strcmp(label, "Invert match") == 0) {
                    g_string_append(preview, "-v ");
                } else if (strcmp(label, "Show line numbers") == 0) {
                    g_string_append(preview, "-n ");
                }
            }
        }
        g_list_free(children);
        
        if (pattern[0] != '\0') {
            g_string_append_printf(preview, "'%s' ", pattern);
        }
        if (input[0] != '\0') {
            g_string_append(preview, input);
        }
    } else if (strcmp(command_type, "awk") == 0) { /* Awk command */
        g_string_append(preview, "awk ");
        
        // Add options from entries
        GList *children = gtk_container_get_children(GTK_CONTAINER(builder_data->options_box));
        for (GList *l = children; l != NULL; l = l->next) {
            if (GTK_IS_ENTRY(l->data)) {
                const char *text = gtk_entry_get_text(GTK_ENTRY(l->data));
                if (text[0] != '\0') {
                    const char *placeholder = gtk_entry_get_placeholder_text(GTK_ENTRY(l->data));
                    if (strstr(placeholder, "Field separator")) {
                        g_string_append_printf(preview, "-F'%s' ", text);
                    } else if (strstr(placeholder, "Output separator")) {
                        g_string_append_printf(preview, "-v OFS='%s' ", text);
                    }
                }
            }
        }
        g_list_free(children);
        
        if (pattern[0] != '\0' || format[0] != '\0') { /* Pattern and format */
            g_string_append(preview, "'");
            if (pattern[0] != '\0') {
                g_string_append(preview, pattern);
            }
            if (format[0] != '\0') {
                if (pattern[0] != '\0') {
                    g_string_append(preview, " ");
                }
                g_string_append(preview, format);
            }
            g_string_append(preview, "' ");
        }
        if (input[0] != '\0') {
            g_string_append(preview, input);
        }
    }

    gtk_entry_set_text(GTK_ENTRY(builder_data->preview_entry), preview->str); /* Set preview text */
    g_string_free(preview, TRUE);
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *terminal;
    GtkWidget *menu_bar;
    GtkWidget *menu;
    GtkWidget *menu_item;
    char **command;

    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gterm | v0.1 ");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    gtk_window_set_icon_from_file(GTK_WINDOW(window), "/usr/share/icons/hicolor/24x24/apps/gtk3-demo.png", NULL);

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    // Create VTE terminal widget
    terminal = vte_terminal_new();

    // Add right-click context menu
    g_signal_connect(terminal, "button-press-event", G_CALLBACK(show_context_menu), terminal);

    // Add keyboard shortcuts
    g_signal_connect(terminal, "key-press-event", G_CALLBACK(on_key_press), terminal);

    // Create menu bar
    menu_bar = gtk_menu_bar_new();
    
    // Create Edit menu
    menu = gtk_menu_new();
    menu_item = gtk_menu_item_new_with_label("Edit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item);

    // Add Copy menu item
    menu_item = gtk_menu_item_new_with_label("Copy");
    g_signal_connect(menu_item, "activate", G_CALLBACK(copy_text), terminal);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    // Add Paste menu item
    menu_item = gtk_menu_item_new_with_label("Paste");
    g_signal_connect(menu_item, "activate", G_CALLBACK(paste_text), terminal);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    // Add separator
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    // Add Select All menu item
    menu_item = gtk_menu_item_new_with_label("Select All");
    g_signal_connect(menu_item, "activate", G_CALLBACK(select_all), terminal);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    // Add Clear menu item
    menu_item = gtk_menu_item_new_with_label("Clear");
    g_signal_connect(menu_item, "activate", G_CALLBACK(clear_terminal), terminal);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    // Add Command Builder menu item
    menu_item = gtk_menu_item_new_with_label("Command Builder");
    g_signal_connect(menu_item, "activate", G_CALLBACK(show_command_builder), terminal);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    // Create vertical box to hold menu bar and terminal
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Add menu bar to vbox
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
    
    // Add terminal to vbox
    gtk_box_pack_start(GTK_BOX(vbox), terminal, TRUE, TRUE, 0);

    // Set fixed font (using Monospace as an example)
    PangoFontDescription *font_desc;
    font_desc = pango_font_description_from_string("Monospace 9");
    vte_terminal_set_font(VTE_TERMINAL(terminal), font_desc);
    pango_font_description_free(font_desc);

    // Configure terminal basics
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), SCROLLBACK);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(terminal), VTE_CURSOR_BLINK_SYSTEM);

    // Set some Xterm-like colors (black background, white text)
    GdkRGBA bg_color = {0.2, 0.1, 0.2, 0.8};  // Black
    GdkRGBA fg_color = {1.0, 1.0, 1.0, 1.0};  // White
    vte_terminal_set_colors(VTE_TERMINAL(terminal), &fg_color, &bg_color, NULL, 0);

    // Spawn a shell
    command = (char *[]){g_getenv("SHELL") ? g_getenv("SHELL") : "/bin/bash", NULL};
    vte_terminal_spawn_async(
        VTE_TERMINAL(terminal),
        VTE_PTY_DEFAULT,
        NULL,          // Working directory
        command,    // Command
        NULL,          // Environment
        0,             // Spawn flags
        NULL, NULL,    // Child setup
        NULL,          // Child pid
        -1,            // Timeout
        NULL,          // Cancellable
        NULL, NULL     // Callback
    );

    // Connect child-exited signal
    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_child_exited), NULL);

    // Show all widgets
    gtk_widget_show_all(window);

    // Set up the environment before spawning the shell
    setup_environment();

    // Start GTK main loop
    gtk_main();

    return 0;
}

/* Function declarations for copy-paste operations
static void copy_text(GtkWidget *widget, gpointer data);
static void paste_text(GtkWidget *widget, gpointer data);
static void select_all(GtkWidget *widget, gpointer data);
static void clear_terminal(GtkWidget *widget, gpointer data);
static void show_context_menu(GtkWidget *widget, GdkEventButton *event, gpointer data); */
