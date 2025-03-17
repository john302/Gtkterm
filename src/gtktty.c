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

#include <gtk/gtk.h>
#include <vte/vte.h>

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

static void on_child_exited(VteTerminal *terminal, gint status, gpointer data) {
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *terminal;

    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Gterm");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_icon_from_file(GTK_WINDOW(window), "/usr/share/icons/hicolor/24x24/apps/gtk3-demo.png", NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    // Create VTE terminal widget
    terminal = vte_terminal_new();

    // Set fixed font (using Monospace as an example)
    PangoFontDescription *font_desc;
    font_desc = pango_font_description_from_string("Monospace 9");
    vte_terminal_set_font(VTE_TERMINAL(terminal), font_desc);
    pango_font_description_free(font_desc);

    // Configure terminal basics
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), 500);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(terminal), VTE_CURSOR_BLINK_SYSTEM);
    

    // Set some Xterm-like colors (black background, white text)
    GdkRGBA bg_color = {0.2, 0.1, 0.2, 0.8};  // Black
    GdkRGBA fg_color = {1.0, 1.0, 1.0, 1.0};  // White
    vte_terminal_set_colors(VTE_TERMINAL(terminal), &fg_color, &bg_color, NULL, 0);

    // Spawn a shell
    char *shell = vte_get_user_shell();
    char *argv_shell[] = {shell, NULL};
    vte_terminal_spawn_async(
        VTE_TERMINAL(terminal),
        VTE_PTY_DEFAULT,
        NULL,          // Working directory
        argv_shell,    // Command
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

    // Add terminal to window
    gtk_container_add(GTK_CONTAINER(window), terminal);

    // Show all widgets
    gtk_widget_show_all(window);

    // Start GTK main loop
    gtk_main();

    return 0;
}

