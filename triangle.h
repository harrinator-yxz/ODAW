// triangle.h
#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TYPE_TRIANGLE_WIDGET (triangle_widget_get_type())
G_DECLARE_FINAL_TYPE(TriangleWidget, triangle_widget, TRIANGLE, WIDGET, GtkWidget)

GtkWidget *drawtriangle(void);

G_END_DECLS

#endif // TRIANGLE_H
