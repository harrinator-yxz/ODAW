#include <gtk/gtk.h>
#include <stdio.h>
#include "triangle.h"
#include "recording.h"

int settempo = 120; // default BPM
int ticks = 0;
int have_recorded = 0;
int have_played = 0;

static double offset = 0.0;
static GtkWidget *global_triangle = NULL;

// Forward declaration for device change handler (now for GtkDropDown)
static void on_device_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data);

static gboolean main_loop(gpointer user_data) {
    ticks++;

    if (!global_triangle)
        return TRUE;

    double beats_per_sec = (double)settempo / 60.0;
    double ticks_per_sec = 1000.0 / 50.0; // 50ms timer interval
    double pixels_per_tick = (beats_per_sec / ticks_per_sec) * 20.0; // 20 px per beat (one box width)
    
    offset += pixels_per_tick;

    // Loop offset after full grid width (80 beats * 20 px)
    if (offset > 1600.0)
        offset -= 1600.0;

    gtk_widget_set_margin_start(global_triangle, (int)offset);
    gtk_widget_queue_draw(global_triangle);

    // Record audio once
    if (have_recorded == 0) {
        record_audio("output.wav", 10);
        have_recorded = 1;
        g_print("Audio recorded to output.wav\n");
    }

    // Play audio once after recording
    if (have_recorded == 1 && have_played == 0) {
        play_audio("output.wav");
        have_played = 1;
        g_print("Audio played from output.wav\n");
    }

    g_print("ticks: %d, offset: %.2f, tempo: %d\n", ticks, offset, settempo);

    return TRUE;
}

static gboolean on_window_close(GtkWindow *window, gpointer user_data) {
    GtkSpinButton *spin = GTK_SPIN_BUTTON(user_data);
    settempo = gtk_spin_button_get_value_as_int(spin);
    g_print("Set tempo: %d\n", settempo);
    fflush(stdout);

    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "odaw.ui", NULL);

    GtkWidget *main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    GtkWidget *container = GTK_WIDGET(gtk_builder_get_object(builder, "main_container"));
    gtk_widget_add_css_class(container, "window-bg");

    GtkWidget *scroller = GTK_WIDGET(gtk_builder_get_object(builder, "scroller"));

    // Build the grid (content)
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 0);

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 80; col++) {
            GtkWidget *frame = gtk_frame_new(NULL);
            gtk_widget_set_size_request(frame, 20, 100);
            gtk_widget_add_css_class(frame, "grid-cell");
            gtk_grid_attach(GTK_GRID(grid), frame, col, row, 1, 1);
        }
    }

    // CSS
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".grid-cell { border: 1px solid #000; border-radius: 0; padding: 0; background-color: #363636; } "
        ".window-bg { background: #363636; }");

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Overlay becomes the scrolled child, grid is the overlay's main child
    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), grid);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), overlay);

    // Triangle is an overlay child (not in the grid) and will scroll with content
    GtkWidget *triangle = drawtriangle();
    gtk_widget_set_size_request(triangle, 25, 25);
    gtk_widget_set_halign(triangle, GTK_ALIGN_START);
    gtk_widget_set_valign(triangle, GTK_ALIGN_START);
    gtk_widget_set_margin_top(triangle, 0);      // top row
    gtk_widget_set_margin_start(triangle, 0);    // will be animated by main_loop
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), triangle);

    global_triangle = triangle;

    // Keep the UI from the .ui file (scroller already inside container), just present the window
    gtk_window_set_application(GTK_WINDOW(main_window), gtk_window_get_application(window));
    gtk_window_present(GTK_WINDOW(main_window));

    g_timeout_add(50, main_loop, NULL);

    g_object_unref(builder);
    return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "odaw.ui", NULL);

    GtkWidget *tempo = GTK_WIDGET(gtk_builder_get_object(builder, "set_tempo"));
    GtkWidget *spinbutton = GTK_WIDGET(gtk_builder_get_object(builder, "spin_button"));
    // Use the box inside the tempo window, not main_container!
    GtkWidget *tempo_box = GTK_WIDGET(gtk_builder_get_object(builder, "tempo_box"));

    // Check if tempo_box is a GtkBox
    if (!GTK_IS_BOX(tempo_box)) {
        g_warning("tempo_box is not a GtkBox! The device dropdown will not be shown.");
    } else {
        // Device selection dropdown using GtkDropDown and GtkStringList (GTK4)
        char **devices = get_input_device_names();
        GtkStringList *strlist = gtk_string_list_new((const char * const *)devices);
        GtkWidget *dropdown = gtk_drop_down_new(G_LIST_MODEL(strlist), NULL);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(dropdown), 0);
        gtk_box_append(GTK_BOX(tempo_box), dropdown);
        g_signal_connect(dropdown, "notify::selected", G_CALLBACK(on_device_changed), NULL);
        g_strfreev(devices);
    }

    g_signal_connect(tempo, "close-request", G_CALLBACK(on_window_close), spinbutton);

    gtk_window_set_application(GTK_WINDOW(tempo), app);
    gtk_window_present(GTK_WINDOW(tempo));

    g_object_unref(builder);
}

static void on_device_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data) {
    int idx = gtk_drop_down_get_selected(dropdown);
    set_recording_device(idx);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.GridLines", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}