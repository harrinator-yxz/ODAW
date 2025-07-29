// main.c
#include <gtk/gtk.h>
#include <stdio.h>
#include "triangle.h"

// Declare the triangle widget factory function
GtkWidget* triangle_widget_new(void);




int settempo = 1;
int ticks = 0;


static int offset = 0;
static GtkWidget *global_triangle = NULL;




static gboolean main_loop(gpointer user_data) {
    ticks += 1;

    if (!global_triangle)
        return TRUE;
    
    offset -= 5;
    g_print("ticks: %d\n", ticks, offset);

    g_print("offset: %d\n", offset);
    
    gtk_widget_set_margin_end(global_triangle, offset);

    gtk_widget_queue_draw(global_triangle);
    
    return TRUE;
}

static gboolean on_window_close(GtkWindow *window, gpointer user_data) {
    GtkSpinButton *spin = GTK_SPIN_BUTTON(user_data);
    settempo = gtk_spin_button_get_value_as_int(spin);
    g_print("Set tempo: %d\n", settempo);
    fflush(stdout);

    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "odaw.ui", NULL);

    // ---- MAIN CONTAINER IS A GTKBOX ----
    GtkWidget *main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    GtkWidget *container = GTK_WIDGET(gtk_builder_get_object(builder, "main_container"));
    gtk_widget_add_css_class(container, "window-bg");

    // ---- GtkOverlay to stack (main) grid and triangle ----
    GtkWidget *scroller = GTK_WIDGET(gtk_builder_get_object(builder, "scroller"));
    // Remove scroller from its original parent (the box)
    gtk_widget_unparent(scroller);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), scroller); // "main content"

    // ---- Make the grid ----
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 0);

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 80; col++) { 
            // change this     ^^^^ to set desirer length in beats. 80 beats in 4/4 is 20 bars.
            GtkWidget *frame = gtk_frame_new(NULL);
            gtk_widget_set_size_request(frame, 20, 100);
            gtk_widget_add_css_class(frame, "grid-cell");
            gtk_grid_attach(GTK_GRID(grid), frame, col, row, 1, 1);
        }
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), grid);

    // ---- CSS ----
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        ".grid-cell { border: 1px solid #000; border-radius: 0; padding: 0; background-color: #363636; } .window-bg {background:  #363636;}",
        -1);

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // ---- Create and position triangle ----
    GtkWidget *triangle = drawtriangle();
    gtk_widget_set_size_request(triangle, 25, 25);
    gtk_widget_set_halign(triangle, GTK_ALIGN_START);   // right
    gtk_widget_set_valign(triangle, GTK_ALIGN_START);   // bottom
    gtk_widget_set_margin_end(triangle, 8);
    gtk_widget_set_margin_bottom(triangle, 8);
    
    global_triangle = triangle;

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), triangle);

    // ---- Insert overlay into UI ----
    // Remove all children from container (to avoid stacking, only overlay needed)
    // GTK4 way to remove all children from a GtkBox container:
while (gtk_widget_get_first_child(container)) {
    gtk_box_remove(GTK_BOX(container), gtk_widget_get_first_child(container));
}


    gtk_box_append(GTK_BOX(container), overlay);

    gtk_window_set_application(GTK_WINDOW(main_window), gtk_window_get_application(window));
    gtk_window_present(GTK_WINDOW(main_window));

    g_timeout_add(50, main_loop, NULL);
    // THIS IS FPS ^^^^^^ 50ms Reload for everytick. 60fps is 16ms
//      
        // 50ms tick time = 20 Frame refresh a second.  /// FPS
    //
    // EQUATION = FPS = 1000 / time = FPS
//
    //
    g_object_unref(builder);
    return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "odaw.ui", NULL);

    GtkWidget *tempo = GTK_WIDGET(gtk_builder_get_object(builder, "set_tempo"));
    GtkWidget *spinbutton = GTK_WIDGET(gtk_builder_get_object(builder, "spin_button"));

    g_signal_connect(tempo, "close-request", G_CALLBACK(on_window_close), spinbutton);

    gtk_window_set_application(GTK_WINDOW(tempo), app);
    gtk_window_present(GTK_WINDOW(tempo));

    g_object_unref(builder);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.GridLines", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
