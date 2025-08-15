#include <gtk/gtk.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>
#include "triangle.h"
#include "recording.h"

// ---- Tempo & animation ----
static int settempo = 120;      // BPM
static int ticks = 0;           // tick counter
static double offset = 0.0;     // triangle x-offset

// ---- Grid layout constants ----
#define CELL_W   20   // px per beat column
#define ROW_H    100  // px per grid row
#define NUM_COLS 80
#define NUM_ROWS 3

// ---- Globals ----
static GtkWidget *global_triangle = NULL; // moving playhead triangle
static GtkWidget *global_overlay  = NULL; // overlay above grid
static GtkWidget *global_grid     = NULL;

static bool recording_started  = false;
static bool recording_finished = false;

// ---- Blue box drawing ----
static void draw_solid_blue(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    cairo_set_source_rgba(cr, 0.1, 0.4, 1.0, 0.75);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
}

typedef struct {
    GtkWidget *overlay;
    int start_px;   // exact start in pixels
    int width_px;   // exact width in pixels
    int row;
} BoxRequest;

static gboolean idle_add_box_cb(gpointer data) {
    BoxRequest *req = (BoxRequest*)data;
    if (!req || !GTK_IS_OVERLAY(req->overlay)) { g_free(req); return FALSE; }

    GtkWidget *box = gtk_drawing_area_new();
    gtk_widget_set_size_request(box, req->width_px, ROW_H);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(box), draw_solid_blue, NULL, NULL);

    gtk_overlay_add_overlay(GTK_OVERLAY(req->overlay), box);
    gtk_widget_set_halign(box, GTK_ALIGN_START);
    gtk_widget_set_valign(box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(box, req->row * ROW_H);
    gtk_widget_set_margin_start(box, req->start_px);
    gtk_widget_show(box);

    g_free(req);
    return FALSE;
}

// ---- Worker threads ----
static void* record_thread(void* arg) {
    const double duration_sec = 10.0; // Replace with actual measured duration
    record_audio("output.wav", duration_sec);
    recording_finished = true;

    double beats_per_sec = (double)settempo / 60.0;
    double px_per_sec = beats_per_sec * CELL_W;
    int width_px = (int)(duration_sec * px_per_sec + 0.5);

    BoxRequest *req = g_new0(BoxRequest, 1);
    req->overlay  = global_overlay;
    req->start_px = 0;  // start position in pixels
    req->width_px = width_px;
    req->row      = 0;

    g_idle_add(idle_add_box_cb, req);
    return NULL;
}

static void* play_thread(void* arg) {
    play_audio("output.wav");
    return NULL;
}

// ---- Animation tick ----
static gboolean main_loop(gpointer user_data) {
    ticks++;
    if (!global_triangle) return TRUE;

    const double interval_ms   = 50.0;
    const double beats_per_sec = (double)settempo / 60.0;
    const double ticks_per_sec = 1000.0 / interval_ms;
    const double px_per_tick = (beats_per_sec / ticks_per_sec) * CELL_W;

    offset += px_per_tick;
    const double loop_px = NUM_COLS * CELL_W;
    if (offset > loop_px) offset -= loop_px;

    gtk_widget_set_margin_start(global_triangle, (int)offset);
    gtk_widget_queue_draw(global_triangle);

    if (!recording_started) {
        recording_started = true;
        pthread_t tid;
        pthread_create(&tid, NULL, record_thread, NULL);
        pthread_detach(tid);
    }

    if (recording_finished) {
        recording_finished = false;
        pthread_t tid2;
        pthread_create(&tid2, NULL, play_thread, NULL);
        pthread_detach(tid2);
    }

    return TRUE;
}

// ---- Window build ----
static gboolean on_window_close(GtkWindow *window, gpointer user_data) {
    GtkSpinButton *spin = GTK_SPIN_BUTTON(user_data);
    if (GTK_IS_SPIN_BUTTON(spin)) {
        settempo = gtk_spin_button_get_value_as_int(spin);
        g_print("Set tempo: %d\n", settempo);
    }

    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "odaw.ui", NULL);

    GtkWidget *main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    GtkWidget *scroller    = GTK_WIDGET(gtk_builder_get_object(builder, "scroller"));
    GtkWidget *container   = GTK_WIDGET(gtk_builder_get_object(builder, "main_container"));
    gtk_widget_add_css_class(container, "window-bg");

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 0);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 0);

    for (int row = 0; row < NUM_ROWS; row++) {
        for (int col = 0; col < NUM_COLS; col++) {
            GtkWidget *frame = gtk_frame_new(NULL);
            gtk_widget_set_size_request(frame, CELL_W, ROW_H);
            gtk_widget_add_css_class(frame, "grid-cell");
            gtk_grid_attach(GTK_GRID(grid), frame, col, row, 1, 1);
        }
    }

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        ".grid-cell { border: 1px solid #000; background-color: #363636; }"
        ".window-bg { background: #363636; }", -1);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), grid);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), overlay);

    global_overlay = overlay;
    global_grid    = grid;

    GtkWidget *triangle = drawtriangle();
    gtk_widget_set_size_request(triangle, 25, 25);
    gtk_widget_set_halign(triangle, GTK_ALIGN_START);
    gtk_widget_set_valign(triangle, GTK_ALIGN_START);
    gtk_widget_set_margin_top(triangle, 0);
    gtk_widget_set_margin_start(triangle, 0);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), triangle);
    global_triangle = triangle;

    gtk_window_set_application(GTK_WINDOW(main_window), gtk_window_get_application(window));
    gtk_window_present(GTK_WINDOW(main_window));

    g_timeout_add(50, main_loop, NULL);

    g_object_unref(builder);
    return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "odaw.ui", NULL);

    GtkWidget *tempo      = GTK_WIDGET(gtk_builder_get_object(builder, "set_tempo"));
    GtkWidget *spinbutton = GTK_WIDGET(gtk_builder_get_object(builder, "spin_button"));
    GtkWidget *tempo_box  = GTK_WIDGET(gtk_builder_get_object(builder, "tempo_box"));

    if (GTK_IS_BOX(tempo_box)) {
        GtkWidget *label = gtk_label_new("ODAW Ready – Select Input Device");
        gtk_box_append(GTK_BOX(tempo_box), label);

        char **devices = get_input_device_names();
        GtkWidget *listbox = gtk_list_box_new();
        gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(listbox), TRUE);

        for (int i = 0; devices && devices[i]; ++i) {
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *dev_label = gtk_label_new(devices[i]);
            gtk_widget_set_margin_start(dev_label, 4);
            gtk_widget_set_margin_end(dev_label, 4);
            gtk_widget_set_margin_top(dev_label, 2);
            gtk_widget_set_margin_bottom(dev_label, 2);
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), dev_label);
            gtk_list_box_insert(GTK_LIST_BOX(listbox), row, -1);
            g_signal_connect(row, "activate", G_CALLBACK(set_recording_device), GINT_TO_POINTER(i));
        }
        gtk_box_append(GTK_BOX(tempo_box), listbox);
        g_strfreev(devices);
    }

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
