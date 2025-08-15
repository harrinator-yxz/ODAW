// triangle.c
#include "triangle.h"
#include <cairo.h>
#include <math.h>

static double g_recording_length_px = 0.0; // exact pixel length of blue box
static gboolean g_show_box = FALSE;

struct _TriangleWidget {
    GtkWidget parent_instance;
};

G_DEFINE_TYPE(TriangleWidget, triangle_widget, GTK_TYPE_WIDGET)

// Set the recording box length in pixels and enable it
void triangle_set_recording_length(double length_px) {
    g_recording_length_px = length_px;
    g_show_box = TRUE;
}

// Hide the box
void triangle_clear_recording_box(void) {
    g_show_box = FALSE;
}

static void triangle_measure(GtkWidget *widget,
                             GtkOrientation orientation,
                             int for_size,
                             int *minimum,
                             int *natural,
                             int *minimum_baseline,
                             int *natural_baseline)
{
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = 20; *natural = 33;
    } else {
        *minimum = 17; *natural = 33;
    }
}

static void triangle_snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    float width = alloc.width;
    float height = alloc.height;

    cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &(graphene_rect_t){
        .origin = { 0, 0 },
        .size = { width, height }
    });

    // Draw blue box if active
    if (g_show_box && g_recording_length_px > 0) {
        cairo_set_source_rgba(cr, 0.0, 0.4, 1.0, 0.5); // semi-transparent blue
        cairo_rectangle(cr, 0, 0, g_recording_length_px, height);
        cairo_fill(cr);
    }

    // Draw red triangle
    float side = fmin(width, height * 2 / sqrt(3)) * 0.95f;
    float h = (sqrt(3) / 2) * side;
    float cx = width / 2.0f;
    float cy = height / 2.0f;

    graphene_point_t p1 = { cx - side / 2, cy - h / 2 };
    graphene_point_t p2 = { cx + side / 2, cy - h / 2 };
    graphene_point_t p3 = { cx,            cy + h / 2 };

    cairo_move_to(cr, p1.x, p1.y);
    cairo_line_to(cr, p2.x, p2.y);
    cairo_line_to(cr, p3.x, p3.y);
    cairo_close_path(cr);

    cairo_set_source_rgb(cr, 1, 0.2, 0.2); // red
    cairo_fill(cr);
}

static void triangle_widget_class_init(TriangleWidgetClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = triangle_snapshot;
    widget_class->measure = triangle_measure;
}

static void triangle_widget_init(TriangleWidget *self) {}

GtkWidget *drawtriangle(void) {
    return g_object_new(TYPE_TRIANGLE_WIDGET, NULL);
}
