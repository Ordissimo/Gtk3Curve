/* Copyright (C) 2016 Benoit Touchette
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation version
 * 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* Portions of this code Copyright (C) 1997 David Mosberger and
 * Copyright (C) 1997 - 2000 GTK+ Team.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>

#include "gtk3curve.h"

#ifdef DEBUG
#define DEBUG_INFO g_print
#define DEBUG_ERROR g_printerr
#else
#define DEBUG_INFO(...)
#define DEBUG_ERROR(...)
#endif

static guint                curve_type_changed_signal = 0;
static gint                 Gtk3Curve_private_offset = 0;
static GtkDrawingAreaClass *gtk3_curve_parent_class = NULL;

#define _gtk3_marshal_VOID__VOID  g_cclosure_marshal_VOID__VOID

#define GTK3_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK3_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK3_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

#define RADIUS            3 /* radius of the control points */
#define MIN_DISTANCE      8 /* min distance between control points */
#define GRAPH_MASK       (GDK_EXPOSURE_MASK | \
                          GDK_POINTER_MOTION_MASK | \
                          GDK_POINTER_MOTION_HINT_MASK | \
                          GDK_ENTER_NOTIFY_MASK | \
                          GDK_LEAVE_NOTIFY_MASK | \
                          GDK_BUTTON_PRESS_MASK | \
                          GDK_BUTTON_RELEASE_MASK | \
                          GDK_BUTTON1_MOTION_MASK)

typedef struct _CurvePoint CurvePoint;

struct _CurvePoint
{
  gint x;
  gint y;
};

struct _Gtk3CurvePrivate
{
  GdkWindow *event_window;
  GList *children;

  gint cursor_type;

  gfloat min_x;
  gfloat max_x;
  gfloat min_y;
  gfloat max_y;

  gfloat bg_r;
  gfloat bg_g;
  gfloat bg_b;
  gfloat bg_a;

  gfloat ln_r;
  gfloat ln_g;
  gfloat ln_b;
  gfloat ln_a;

  gfloat dt_r;
  gfloat dt_g;
  gfloat dt_b;
  gfloat dt_a;

  Gtk3CurveType curve_type;

  gint width;
  gint height;
  gint grab_point;
  gint last;

  Gtk3CurveGridSize grid_size;

  gboolean use_bg_theme;

  gint num_points;
  CurvePoint *point;
  gint num_ctlpoints;
  gfloat (*ctlpoint)[2];

  guint state                 : 1;
  guint in_curve              : 1;
};

enum
{
  PROP_0,
  PROP_CURVE_TYPE,
  PROP_MIN_X,
  PROP_MAX_X,
  PROP_MIN_Y,
  PROP_MAX_Y
};

static void gtk3_curve_realize              (GtkWidget            *widget);
static void gtk3_curve_unrealize            (GtkWidget            *widget);
static void gtk3_curve_style_updated        (GtkWidget            *widget);
static void gtk3_curve_size_allocate        (GtkWidget            *widget,
                                             GtkAllocation        *allocation);
static void gtk3_curve_configure            (Gtk3Curve            *curve);
static gboolean gtk3_curve_draw             (GtkWidget            *widget,
                                             cairo_t              *cr);
static void gtk3_curve_map                  (GtkWidget            *widget);
static void gtk3_curve_unmap                (GtkWidget            *widget);
static gboolean gtk3_curve_enter            (GtkWidget            *widget,
                                             GdkEventCrossing     *event);
static gboolean gtk3_curve_leave            (GtkWidget            *widget,
                                             GdkEventCrossing     *event);
static gboolean gtk3_curve_button_press     (GtkWidget            *widget,
                                             GdkEventButton       *event);
static gboolean gtk3_curve_button_release   (GtkWidget            *widget,
                                             GdkEventButton       *event);
static gboolean gtk3_curve_motion_notify    (GtkWidget            *widget,
                                             GdkEventMotion       *event);
static void gtk3_curve_screen_changed       (GtkWidget            *widget,
                                             GdkScreen            *prev_screen);
static void gtk3_curve_style_updated        (GtkWidget            *widget);
static void gtk3_curve_finalize             (GObject              *object);
static void gtk3_curve_dispose              (GObject              *object);
static void gtk3_curve_get_property         (GObject              *object,
                                             guint                 param_id,
                                             GValue               *value,
                                             GParamSpec           *pspec);
static void gtk3_curve_set_property         (GObject              *object,
                                             guint                 param_id,
                                             const GValue         *value,
                                             GParamSpec           *pspec);
static void gtk3_curve_size_graph           (Gtk3Curve            *curve);
static void gtk3_curve_create_layouts       (GtkWidget            *widget);
static void gtk3_curve_reset_vector         (GtkWidget            *widget);
static void gtk3_curve_interpolate          (GtkWidget            *widget,
                                             gint                  width,
                                             gint                  height);
static int project                          (gfloat                value,
                                             gfloat                min,
                                             gfloat                max,
                                             int                   norm);
static gfloat unproject                     (gint                  value,
                                             gfloat                min,
                                             gfloat                max,
                                             int                   norm);
static void spline_solve                    (int                   n,
                                             gfloat                x[],
                                             gfloat                y[],
                                             gfloat                y2[]);
static gfloat spline_eval                   (int                   n,
                                             gfloat                x[],
                                             gfloat                y[],
                                             gfloat                y2[],
                                             gfloat                val);
static void gtk3_curve_class_init           (Gtk3CurveClass   *klass);
static void gtk3_curve_init                 (Gtk3Curve        *self);

static inline gpointer
gtk3_curve_get_instance_private (Gtk3Curve *self)
{
  return (G_STRUCT_MEMBER_P (self, Gtk3Curve_private_offset));
}

static void gtk_gadget_class_intern_init (gpointer klass)
{
  g_type_class_adjust_private_offset (klass, &Gtk3Curve_private_offset);
  gtk3_curve_parent_class = g_type_class_peek_parent (klass);
  gtk3_curve_class_init ((Gtk3CurveClass*) klass);
}

GType
gtk3_curve_get_type (void)
{
  static GType curve_type = 0;

  if (G_UNLIKELY (curve_type == 0))
    {
      const GTypeInfo curve_info =
      {
        sizeof (Gtk3CurveClass),
        NULL,   // base_init
        NULL,   // base_finalize
        (GClassInitFunc) gtk3_curve_class_init,
        NULL,   // class_finalize
        NULL,   // class_data
        sizeof (Gtk3Curve),
        0,    // n_preallocs
        (GInstanceInitFunc) gtk3_curve_init,
      };

      curve_type = g_type_register_static (GTK_TYPE_WIDGET, "Gtk3Curve",
                                           &curve_info, 0);

      Gtk3Curve_private_offset =
        g_type_add_instance_private (curve_type, sizeof (Gtk3CurvePrivate));
    }
  return curve_type;
}

GType
gtk3_curve_type_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY(etype == 0))
    {
      static const GEnumValue values[] =
      {
        { GTK3_CURVE_TYPE_LINEAR, "GTK3_CURVE_TYPE_LINEAR", "linear" },
        { GTK3_CURVE_TYPE_SPLINE, "GTK3_CURVE_TYPE_SPLINE", "spline" },
        { GTK3_CURVE_TYPE_FREE, "GTK3_CURVE_TYPE_FREE", "free" },
        { 0, NULL, NULL }
      };
      etype = g_enum_register_static (g_intern_static_string ("Gtk3CurveType"),
                                      values);
    }
  return etype;
}

static void
gtk3_curve_class_init (Gtk3CurveClass* klass)
{
  GtkContainerClass *container_class;
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  DEBUG_INFO("class_init [S]\n");

  widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->realize = gtk3_curve_realize;
  widget_class->unrealize = gtk3_curve_unrealize;

  widget_class->draw = gtk3_curve_draw;
  widget_class->motion_notify_event = gtk3_curve_motion_notify;

  widget_class->map = gtk3_curve_map;
  widget_class->unmap = gtk3_curve_unmap;

  widget_class->enter_notify_event = gtk3_curve_enter;
  widget_class->leave_notify_event = gtk3_curve_leave;

  widget_class->button_press_event = gtk3_curve_button_press;
  widget_class->button_release_event = gtk3_curve_button_release;

  widget_class->size_allocate = gtk3_curve_size_allocate;
  widget_class->screen_changed = gtk3_curve_screen_changed;
  widget_class->style_updated = gtk3_curve_style_updated;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_DRAWING_AREA);

  gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_adjust_private_offset (klass, &Gtk3Curve_private_offset);

  gtk3_curve_parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gtk3_curve_finalize;
  gobject_class->dispose = gtk3_curve_dispose;
  gobject_class->set_property = gtk3_curve_set_property;
  gobject_class->get_property = gtk3_curve_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_CURVE_TYPE,
                                   g_param_spec_enum ("curve-type",
                                       "Curve type",
                                       "Is this curve linear, spline interpolated, or free-form",
                                       GTK3_TYPE_CURVE_TYPE,
                                       GTK3_CURVE_TYPE_SPLINE,
                                       GTK3_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MIN_X,
                                   g_param_spec_float ("min-x",
                                       "Minimum X",
                                       "Minimum possible value for X",
                                       -G_MAXFLOAT,
                                       G_MAXFLOAT,
                                       0.0,
                                       GTK3_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_X,
                                   g_param_spec_float ("max-x",
                                       "Maximum X",
                                       "Maximum possible X value",
                                       -G_MAXFLOAT,
                                       G_MAXFLOAT,
                                       1.0,
                                       GTK3_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MIN_Y,
                                   g_param_spec_float ("min-y",
                                       "Minimum Y",
                                       "Minimum possible value for Y",
                                       -G_MAXFLOAT,
                                       G_MAXFLOAT,
                                       0.0,
                                       GTK3_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_Y,
                                   g_param_spec_float ("max-y",
                                       "Maximum Y",
                                       "Maximum possible value for Y",
                                       -G_MAXFLOAT,
                                       G_MAXFLOAT,
                                       1.0,
                                       GTK3_PARAM_READWRITE));

  curve_type_changed_signal =
    g_signal_new ("curve-type-changed",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (Gtk3CurveClass, curve_type_changed),
                  NULL, NULL,
                  _gtk3_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  DEBUG_INFO("class_init [E]\n");
}

static void
gtk3_curve_init (Gtk3Curve* self)
{
  Gtk3CurvePrivate *priv;

  DEBUG_INFO("init [S]\n");

  gtk_widget_set_has_window(GTK_WIDGET(self), TRUE);
  gtk_widget_set_redraw_on_allocate(GTK_WIDGET(self), TRUE);

  self->priv = gtk3_curve_get_instance_private (self);
  priv = self->priv;

  priv->cursor_type = GDK_TOP_LEFT_ARROW;
  priv->curve_type = GTK3_CURVE_TYPE_SPLINE;

  priv->height = 0;
  priv->grab_point = -1;

  priv->num_points = 0;
  priv->point = NULL;

  priv->num_ctlpoints = 0;
  priv->ctlpoint = NULL;

  priv->use_bg_theme = TRUE;

  priv->grid_size = GTK3_CURVE_GRID_LARGE;

  priv->min_x = 0.0;
  priv->max_x = 1.0;
  priv->min_y = 0.0;
  priv->max_y = 1.0;

  priv->bg_r = 1.0;
  priv->bg_g = 1.0;
  priv->bg_b = 1.0;
  priv->bg_a = 1.0;

  priv->ln_r = 0.0;
  priv->ln_g = 0.0;
  priv->ln_b = 0.0;
  priv->ln_a = 1.0;

  priv->dt_r = 0.0;
  priv->dt_g = 0.0;
  priv->dt_b = 0.0;
  priv->dt_a = 1.0;

  gtk3_curve_size_graph (self);

  DEBUG_INFO("init [E]\n");
}

GtkWidget *
gtk3_curve_new()
{
  return g_object_new (GTK3_TYPE_CURVE, NULL);
}

static void
gtk3_curve_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk3_curve_parent_class)->style_updated (widget);

  DEBUG_INFO("style_updated [S]\n");
  if (gtk_widget_get_realized (widget) &&
      gtk_widget_get_has_window (widget))
    {
      gtk_widget_queue_draw (widget);
    }
  DEBUG_INFO("style_updated [E]\n");
}

static void
gtk3_curve_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  Gtk3CurvePrivate *priv;

  priv = GTK3_CURVE (widget)->priv;

  DEBUG_INFO("realize [S]\n");
  if (!gtk_widget_get_has_window (widget))
    {
      GTK_WIDGET_CLASS (gtk3_curve_parent_class)->realize (widget);
    }
  else
    {
      gtk_widget_set_realized (widget, TRUE);
      parent_window = gtk_widget_get_parent_window (widget);
      gtk_widget_set_window (widget, parent_window);
      g_object_ref (parent_window);

      gtk_widget_get_allocation (widget, &allocation);
      DEBUG_INFO("allocation [%d,%d] [%dx%d]\n",
              allocation.x,
              allocation.y,
              allocation.width,
              allocation.height);

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = allocation.x;
      attributes.y = allocation.y;
      attributes.width = allocation.width;
      attributes.height = allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.event_mask = gtk_widget_get_events (widget) |
                              GRAPH_MASK;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

      priv->event_window = gdk_window_new (parent_window,
                                           &attributes,
                                           attributes_mask);

      gtk_widget_register_window (widget, priv->event_window);
      gtk_widget_set_window (widget, priv->event_window);
    }

  gtk3_curve_configure (GTK3_CURVE (widget));

  DEBUG_INFO("realize [E]\n");
}

static void
gtk3_curve_unrealize (GtkWidget *widget)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;

  DEBUG_INFO("unrealize [S]\n");
  if (priv->event_window != NULL)
    {
      DEBUG_INFO("unregister/destroy\n");
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  DEBUG_INFO("unrealize [E]\n");
}

static void
gtk3_curve_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  g_return_if_fail (GTK3_IS_CURVE (widget));
  g_return_if_fail (allocation != NULL);

  DEBUG_INFO("size_allocate [S]\n");
  gtk_widget_set_allocation (widget, allocation);

  DEBUG_INFO("allocation [%d,%d] [%dx%d]\n",
          allocation->x,
          allocation->y,
          allocation->width,
          allocation->height);

  if (gtk_widget_get_realized (widget))
    {
      if (gtk_widget_get_has_window (widget))
        {
          gdk_window_move_resize (gtk_widget_get_window (widget),
                                  allocation->x, allocation->y,
                                  allocation->width, allocation->height);
        }

      gtk3_curve_configure (GTK3_CURVE (widget));
    }
  DEBUG_INFO("size_allocate [E]\n");
}

static void
gtk3_curve_configure (Gtk3Curve *curve)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

  DEBUG_INFO("configure [S]\n");

  widget = GTK_WIDGET (curve);
  gtk_widget_get_allocation (widget, &allocation);

  DEBUG_INFO("allocation [%d,%d] [%dx%d]\n",
          allocation.x,
          allocation.y,
          allocation.width,
          allocation.height);

  event->configure.window = g_object_ref (gtk_widget_get_window (widget));
  event->configure.send_event = TRUE;
  event->configure.x = allocation.x;
  event->configure.y = allocation.y;
  event->configure.width = allocation.width;
  event->configure.height = allocation.height;

  gtk_widget_event (widget, event);
  gtk_widget_queue_draw (widget);

  gdk_event_free (event);

  DEBUG_INFO("configure [E]\n");
}

static gboolean
gtk3_curve_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  Gtk3CurvePrivate *priv;
  GtkStyleContext  *style_context;
  GtkStyle         *style;
  GdkRGBA           color;
  gint              last_x, last_y;
  gint              i;
  GtkAllocation     allocation;
  Gtk3Curve        *curve;
  gfloat            grid;

  curve = GTK3_CURVE (widget);
  priv = curve->priv;

  DEBUG_INFO("draw [S]\n");

  if (!cr)
    {
      DEBUG_ERROR("cairo == null\n");
      return TRUE;
    }

  gtk_widget_get_allocation (widget, &allocation);

  DEBUG_INFO("%d x %d\n", allocation.width, allocation.height);

  if (priv->height != allocation.width ||
      priv->num_points != allocation.height)
    gtk3_curve_interpolate (widget, allocation.width, allocation.height);

  if (priv->use_bg_theme)
    {
      style_context = gtk_widget_get_style_context(GTK_WIDGET (curve));
      gtk_render_background(style_context, cr, 0, 0, allocation.width + RADIUS * 2,
                            allocation.height + RADIUS * 2);
      gtk_style_context_get_color (style_context,
                                   gtk_style_context_get_state (style_context),
                                   &color);
      gdk_cairo_set_source_rgba (cr, &color);
    }
  else
    {
      cairo_set_source_rgba (cr, priv->bg_r, priv->bg_g, priv->bg_b, priv->bg_a);
    }
  cairo_paint(cr);

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  switch (priv->grid_size)
    {
      default:
      case GTK3_CURVE_GRID_SMALL:
        grid = 8.0;
        break;
      case GTK3_CURVE_GRID_MEDIUM:
        grid = 6.0;
        break;
      case GTK3_CURVE_GRID_LARGE:
        grid = 4.0;
      case GTK3_CURVE_GRID_XLARGE:
        grid = 2.0;
    }

  /* Draw grid */
  for (i = 0; i < (int)grid+1; i++)
    {
      gint hx1 = RADIUS;
      gint hy1 = i * ((allocation.height - RADIUS * 2) / grid) + RADIUS;
      gint hx2 = allocation.width - RADIUS;
      gint hy2 = i * ((allocation.height - RADIUS * 2) / grid) + RADIUS;

      cairo_set_line_width (cr, 0.5);
      cairo_set_source_rgba (cr, priv->ln_r, priv->ln_g,
                             priv->ln_b, priv->ln_a);
      cairo_move_to (cr, hx1, hy1);
      cairo_line_to (cr, hx2, hy2);
      cairo_stroke (cr);

      gint vx1 = i * ((allocation.width - RADIUS * 2) / grid) + RADIUS;
      gint vy1 = RADIUS;
      gint vx2 = i * ((allocation.width - RADIUS * 2) / grid) + RADIUS;
      gint vy2 = allocation.height - RADIUS;

      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, priv->ln_r, priv->ln_g,
                             priv->ln_b, priv->ln_a);
      cairo_move_to (cr, vx1, vy1);
      cairo_line_to (cr, vx2, vy2);
      cairo_stroke (cr);
    }

  /* Draw a curve or line or set of lines */
  gfloat wm = (allocation.width - RADIUS * 2 - (RADIUS / 2));
  gfloat wt = allocation.width;
  gfloat hm = (allocation.height - RADIUS * 2 - (RADIUS / 2));
  gfloat ht = allocation.height;
  gfloat ratio_x = wm / wt;
  gfloat ratio_y = hm / ht;
  for (int i=0; i<priv->num_points; i++)
    {
      gint x = priv->point[i].x * ratio_x + RADIUS;
      gint y = priv->point[i].y * ratio_y + RADIUS;

      if (i == 0)
        cairo_move_to (cr, x, y);
      else
        {
          cairo_set_source_rgba (cr, priv->ln_r, priv->ln_g,
                                 priv->ln_b, priv->ln_a);
          cairo_move_to (cr, last_x, last_y);
          cairo_line_to (cr, x, y);
          cairo_stroke (cr);
        }

      if (priv->num_points > 1)
        {
          last_x = x;
          last_y = y;
        }
    }

  if (priv->curve_type != GTK3_CURVE_TYPE_FREE)
    for (i = 0; i < priv->num_ctlpoints; ++i)
      {
        gint x, y;

        if (priv->ctlpoint[i][0] < priv->min_x)
          continue;

        x = project (priv->ctlpoint[i][0],
                     priv->min_x, priv->max_x,
                     allocation.width);
        y = allocation.height - project (priv->ctlpoint[i][1],
                                         priv->min_y,
                                         priv->max_y,
                                         allocation.height);

        /* draw a bullet */
        cairo_arc(cr,
                  x * ratio_x + RADIUS * 2,
                  y * ratio_y + RADIUS * 2,
                  RADIUS,
                  0,
                  2 * M_PI);
        cairo_set_source_rgba (cr, priv->dt_r, priv->dt_g,
                               priv->dt_b, priv->dt_a);
        cairo_fill (cr);
      }

  DEBUG_INFO("draw [E]\n");
  return FALSE;
}

static void
gtk3_curve_map (GtkWidget *widget)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;

  GTK_WIDGET_CLASS (gtk3_curve_parent_class)->map (widget);

  DEBUG_INFO("map [S]\n");
  if (priv->event_window)
    {
      DEBUG_INFO("show\n");
      gdk_window_show (priv->event_window);
    }
  DEBUG_INFO("map [E]\n");
}

static void
gtk3_curve_unmap (GtkWidget *widget)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;

  DEBUG_INFO("unmap [S]\n");
  if (priv->event_window)
    {
      DEBUG_INFO("hide\n");
      gdk_window_hide (priv->event_window);
    }

  GTK_WIDGET_CLASS (gtk3_curve_parent_class)->unmap (widget);
  DEBUG_INFO("unmap [E]\n");
}

static gboolean
gtk3_curve_enter (GtkWidget        *widget,
                  GdkEventCrossing *event)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;

  DEBUG_INFO("enter [S]\n");
  if (event->window == priv->event_window)
    {
      priv->in_curve = TRUE;
    }
  DEBUG_INFO("enter [E]\n");

  return FALSE;
}

static gboolean
gtk3_curve_leave (GtkWidget        *widget,
                  GdkEventCrossing *event)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;

  DEBUG_INFO("leave [S]\n");
  if (event->window == priv->event_window)
    {
      priv->in_curve = FALSE;
    }
  DEBUG_INFO("leave [E]\n");

  return FALSE;
}


static gboolean
gtk3_curve_button_press (GtkWidget        *widget,
                         GdkEventButton   *event)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;
  GdkCursorType new_type = priv->cursor_type;
  GdkDeviceManager *device_manager;
  GdkDevice        *device_pointer;
  GtkAllocation  allocation;
  gint cx, x, y, width, height, i;
  gint closest_point = 0;
  gfloat rx, ry, min_x;
  gint tx, ty;
  guint distance;

  DEBUG_INFO("button press [S]\n");

  gtk_grab_add (widget);
  new_type = GDK_TCROSS;

  gtk_widget_get_allocation (widget, &allocation);

  width = allocation.width - RADIUS * 2;
  height = allocation.height - RADIUS * 2;

  if ((width < 0) || (height < 0))
    return FALSE;

  /*  get the pointer position  */
  device_manager = gdk_display_get_device_manager (
                     gtk_widget_get_display (widget));
  device_pointer = gdk_device_manager_get_client_pointer (device_manager);
  gdk_window_get_device_position (gtk_widget_get_window (widget),
                                  device_pointer,
                                  &tx, &ty, NULL);
  x = CLAMP ((tx - RADIUS), 0, width-1);
  y = CLAMP ((ty - RADIUS), 0, height-1);

  min_x = priv->min_x;

  distance = ~0U;
  for (i = 0; i < priv->num_ctlpoints; ++i)
    {
      cx = project (priv->ctlpoint[i][0], min_x, priv->max_x, width);
      if ((guint) abs (x - cx) < distance)
        {
          distance = abs (x - cx);
          closest_point = i;
        }
    }

  switch (priv->curve_type)
    {
    case GTK3_CURVE_TYPE_LINEAR:
    case GTK3_CURVE_TYPE_SPLINE:
      if (distance > MIN_DISTANCE)
        {
          /* insert a new control point */
          if (priv->num_ctlpoints > 0)
            {
              cx = project (priv->ctlpoint[closest_point][0], min_x,
                            priv->max_x, width);
              if (x > cx)
                ++closest_point;
            }
          ++priv->num_ctlpoints;
          priv->ctlpoint =
            g_realloc (priv->ctlpoint,
                       priv->num_ctlpoints * sizeof (*priv->ctlpoint));
          for (i = priv->num_ctlpoints - 1; i > closest_point; --i)
            memcpy (priv->ctlpoint + i, priv->ctlpoint + i - 1,
                    sizeof (*priv->ctlpoint));
        }
      priv->grab_point = closest_point;
      priv->ctlpoint[priv->grab_point][0] =
        unproject (x, min_x, priv->max_x, width);
      priv->ctlpoint[priv->grab_point][1] =
        unproject (height - y, priv->min_y, priv->max_y, height);
      gtk3_curve_interpolate (widget, width, height);
      break;

    case GTK3_CURVE_TYPE_FREE:
      priv->point[x].x = RADIUS + x;
      priv->point[x].y = RADIUS + y;
      priv->grab_point = x;
      priv->last = y;
      break;
    }

  if (gtk_widget_is_visible (widget))
    {
      DEBUG_INFO("queue draw\n");
      gtk_widget_queue_draw (widget);
    }

  DEBUG_INFO("button press [E]\n");

  return FALSE;
}

static gboolean
gtk3_curve_button_release (GtkWidget        *widget,
                           GdkEventButton   *event)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;
  GdkDeviceManager *device_manager;
  GdkDevice        *device_pointer;
  GtkAllocation  allocation;
  GdkCursorType new_type = priv->cursor_type;
  gint  src, dst, cx, x, y, width, height, i;
  gfloat rx, ry, min_x;
  gint closest_point = 0;
  guint distance;
  gint tx, ty;

  DEBUG_INFO("button release [S]\n");

  gtk_grab_remove (widget);

  gtk_widget_get_allocation (widget, &allocation);

  width = allocation.width - RADIUS * 2;
  height = allocation.height - RADIUS * 2;

  if ((width < 0) || (height < 0))
    return FALSE;

  /*  get the pointer position  */
  device_manager = gdk_display_get_device_manager (
                     gtk_widget_get_display (widget));
  device_pointer = gdk_device_manager_get_client_pointer (device_manager);
  gdk_window_get_device_position (gtk_widget_get_window (widget),
                                  device_pointer,
                                  &tx, &ty, NULL);
  x = CLAMP ((tx - RADIUS), 0, width-1);
  y = CLAMP ((ty - RADIUS), 0, height-1);

  min_x = priv->min_x;

  distance = ~0U;
  for (i = 0; i < priv->num_ctlpoints; ++i)
    {
      cx = project (priv->ctlpoint[i][0], min_x, priv->max_x, width);
      if ((guint) abs (x - cx) < distance)
        {
          distance = abs (x - cx);
          closest_point = i;
        }
    }

  /* delete inactive points: */
  if (priv->curve_type != GTK3_CURVE_TYPE_FREE)
    {
      for (src = dst = 0; src < priv->num_ctlpoints; ++src)
        {
          if (priv->ctlpoint[src][0] >= min_x)
            {
              memcpy (priv->ctlpoint + dst, priv->ctlpoint + src, sizeof (*priv->ctlpoint));
              ++dst;
            }
        }
      if (dst < src)
        {
          priv->num_ctlpoints -= (src - dst);
          if (priv->num_ctlpoints <= 0)
            {
              priv->num_ctlpoints = 1;
              priv->ctlpoint[0][0] = min_x;
              priv->ctlpoint[0][1] = priv->min_y;
              gtk3_curve_interpolate (widget, width, height);
              if (gtk_widget_is_visible (widget))
                {
                  DEBUG_INFO("queue draw\n");
                  gtk_widget_queue_draw (widget);
                }
            }
          priv->ctlpoint = g_realloc (priv->ctlpoint,
                                      priv->num_ctlpoints * sizeof (*priv->ctlpoint));
        }
    }

  new_type = GDK_FLEUR;
  priv->grab_point = -1;

  DEBUG_INFO("button release [E]\n");

  return FALSE;
}

static gboolean
gtk3_curve_motion_notify (GtkWidget        *widget,
                          GdkEventMotion   *event)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (widget)->priv;
  GdkDeviceManager *device_manager;
  GdkDevice        *device_pointer;
  GtkAllocation  allocation;
  GdkCursorType new_type = priv->cursor_type;
  gint i, src, dst, leftbound, rightbound;
  GdkEventMotion *mevent;
  gint tx, ty;
  gint cx, x, y, width, height;
  gint closest_point = 0;
  gfloat rx, ry, min_x;
  guint distance;
  gint x1, x2, y1, y2;
  gint retval = FALSE;
  gint h,w;

  DEBUG_INFO("motion_notify [S]\n");
  mevent = (GdkEventMotion *) event;


  gtk_widget_get_allocation (widget, &allocation);

  width = allocation.width - RADIUS * 2;
  height = allocation.height - RADIUS * 2;

  if ((width < 0) || (height < 0))
    return FALSE;

  /*  get the pointer position  */
  device_manager = gdk_display_get_device_manager (
                     gtk_widget_get_display (widget));
  device_pointer = gdk_device_manager_get_client_pointer (device_manager);
  gdk_window_get_device_position (gtk_widget_get_window (widget),
                                  device_pointer,
                                  &tx, &ty, NULL);
  x = CLAMP ((tx - RADIUS), 0, width-1);
  y = CLAMP ((ty - RADIUS), 0, height-1);

  min_x = priv->min_x;

  distance = ~0U;
  for (i = 0; i < priv->num_ctlpoints; ++i)
    {
      cx = project (priv->ctlpoint[i][0], min_x, priv->max_x, width);
      if ((guint) abs (x - cx) < distance)
        {
          distance = abs (x - cx);
          closest_point = i;
        }
    }

  switch (priv->curve_type)
    {
    case GTK3_CURVE_TYPE_LINEAR:
    case GTK3_CURVE_TYPE_SPLINE:
      if (priv->grab_point == -1)
        {
          /* if no point is grabbed...  */
          if (distance <= MIN_DISTANCE)
            new_type = GDK_FLEUR;
          else
            new_type = GDK_TCROSS;
        }
      else
        {
          /* drag the grabbed point  */
          new_type = GDK_TCROSS;

          leftbound = -MIN_DISTANCE;
          if (priv->grab_point > 0)
            leftbound = project (priv->ctlpoint[priv->grab_point - 1][0],
                                 min_x, priv->max_x, width);

          rightbound = width + RADIUS * 2 + MIN_DISTANCE;
          if (priv->grab_point + 1 < priv->num_ctlpoints)
            rightbound = project (priv->ctlpoint[priv->grab_point + 1][0],
                                  min_x, priv->max_x, width);

          if (tx <= leftbound || tx >= rightbound
              || ty > height + RADIUS * 2 + MIN_DISTANCE
              || ty < -MIN_DISTANCE)
            priv->ctlpoint[priv->grab_point][0] = min_x - 1.0;
          else
            {
              rx = unproject (x, min_x, priv->max_x, width);
              ry = unproject (height - y, priv->min_y, priv->max_y, height);
              priv->ctlpoint[priv->grab_point][0] = rx;
              priv->ctlpoint[priv->grab_point][1] = ry;
            }
          gtk3_curve_interpolate (widget, width, height);
          if (gtk_widget_is_visible (widget))
            {
              DEBUG_INFO("queue draw\n");
              gtk_widget_queue_draw (widget);
            }
        }
      break;

    case GTK3_CURVE_TYPE_FREE:
      if (priv->grab_point != -1)
        {
          if (priv->grab_point > x)
            {
              x1 = x;
              x2 = priv->grab_point;
              y1 = y;
              y2 = priv->last;
            }
          else
            {
              x1 = priv->grab_point;
              x2 = x;
              y1 = priv->last;
              y2 = y;
            }

          if (x2 != x1)
            for (i = x1; i <= x2; i++)
              {
                priv->point[i].x = RADIUS + i;
                priv->point[i].y = RADIUS +
                                   (y1 + ((y2 - y1) * (i - x1)) / (x2 - x1));
              }
          else
            {
              priv->point[x].x = RADIUS + x;
              priv->point[x].y = RADIUS + y;
            }
          priv->grab_point = x;
          priv->last = y;
          if (gtk_widget_is_visible (widget))
            {
              DEBUG_INFO("queue draw\n");
              gtk_widget_queue_draw (widget);
            }
        }
      if (mevent->state & GDK_BUTTON1_MASK)
        new_type = GDK_TCROSS;
      else
        new_type = GDK_PENCIL;
      break;
    }

  if (new_type != (GdkCursorType) priv->cursor_type)
    {
      GdkCursor *cursor;
      priv->cursor_type = new_type;
      cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
                                           priv->cursor_type);
      gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
      g_object_unref (cursor);
    }

  if (gtk_widget_is_visible (widget))
    {
      DEBUG_INFO("queue draw\n");
      gtk_widget_queue_draw (widget);
    }

  DEBUG_INFO("motion_notify [E]\n");
}

static void
gtk3_curve_screen_changed (GtkWidget *widget,
                           GdkScreen *prev_screen)
{
  DEBUG_INFO("screen changed [S]\n");
  gtk3_curve_create_layouts (widget);
  DEBUG_INFO("screen changed [E]\n");
}

static void
gtk3_curve_dispose (GObject *object)
{
  Gtk3CurvePrivate *priv = GTK3_CURVE (object)->priv;
  G_OBJECT_CLASS (gtk3_curve_parent_class)->dispose (object);
}

static void
gtk3_curve_finalize (GObject *object)
{
  Gtk3Curve *curve;
  Gtk3CurvePrivate *priv;

  g_return_if_fail (GTK3_IS_CURVE (object));

  curve = GTK3_CURVE (object);
  priv = curve->priv;

  if (priv->point)
    g_free (priv->point);
  if (priv->ctlpoint)
    g_free (priv->ctlpoint);

  G_OBJECT_CLASS (gtk3_curve_parent_class)->finalize (object);
}

static void
gtk3_curve_set_property (GObject              *object,
                         guint                 prop_id,
                         const GValue         *value,
                         GParamSpec           *pspec)
{
  Gtk3Curve *curve = GTK3_CURVE (object);
  GtkWidget *widget = GTK_WIDGET(object);
  Gtk3CurvePrivate *priv = curve->priv;

  switch (prop_id)
    {
    case PROP_CURVE_TYPE:
      gtk3_curve_set_curve_type (widget, g_value_get_enum (value));
      break;

    case PROP_MIN_X:
      gtk3_curve_set_range (widget, g_value_get_float (value), priv->max_x,
                            priv->min_y, priv->max_y);
      break;

    case PROP_MAX_X:
      gtk3_curve_set_range (widget, priv->min_x, g_value_get_float (value),
                            priv->min_y, priv->max_y);
      break;

    case PROP_MIN_Y:
      gtk3_curve_set_range (widget, priv->min_x, priv->max_x,
                            g_value_get_float (value), priv->max_y);
      break;

    case PROP_MAX_Y:
      gtk3_curve_set_range (widget, priv->min_x, priv->max_x,
                            priv->min_y, g_value_get_float (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk3_curve_get_property (GObject              *object,
                         guint                 prop_id,
                         GValue               *value,
                         GParamSpec           *pspec)
{
  Gtk3Curve *curve = GTK3_CURVE (object);
  Gtk3CurvePrivate *priv = curve->priv;

  switch (prop_id)
    {
    case PROP_CURVE_TYPE:
      g_value_set_enum (value, priv->curve_type);
      break;

    case PROP_MIN_X:
      g_value_set_float (value, priv->min_x);
      break;

    case PROP_MAX_X:
      g_value_set_float (value, priv->max_x);
      break;

    case PROP_MIN_Y:
      g_value_set_float (value, priv->min_y);
      break;

    case PROP_MAX_Y:
      g_value_set_float (value, priv->max_y);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk3_curve_size_graph (Gtk3Curve *curve)
{
  Gtk3CurvePrivate *priv = curve->priv;
  gint width, height;
  //gint size_x, size_y;
  gfloat aspect;
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (curve));

  width  = (priv->max_x - priv->min_x);
  height = (priv->max_y - priv->min_y);
  aspect = width / (gfloat) height;
  if (width > gdk_screen_get_width (screen) / 4)
    {
      width  = gdk_screen_get_width (screen) / 4;
    }
  if (height > gdk_screen_get_height (screen) / 4)
    {
      height = gdk_screen_get_height (screen) / 4;
    }

  if (aspect < 1.0)
    {
      width  = height * aspect;
    }
  else
    {
      height = width / aspect;
    }

  //size_x = width + (RADIUS * 2);
  //size_y = height + (RADIUS * 2);

  DEBUG_INFO("Set requested size to [%dx%d]\n", width, height);

  gtk_widget_set_size_request (GTK_WIDGET (curve), width, height);
}

static void
gtk3_curve_create_layouts (GtkWidget *widget)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  DEBUG_INFO("create pango [S]\n");
  DEBUG_INFO("create pango [E]\n");
}

static void
gtk3_curve_reset_vector (GtkWidget *widget)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  gint width, height;
  gint w,h;

  g_free (priv->ctlpoint);

  priv->num_ctlpoints = 2;
  priv->ctlpoint = g_malloc (2 * sizeof (priv->ctlpoint[0]));
  priv->ctlpoint[0][0] = priv->min_x;
  priv->ctlpoint[0][1] = priv->min_y;
  priv->ctlpoint[1][0] = priv->max_x;
  priv->ctlpoint[1][1] = priv->max_y;

  w = gtk_widget_get_allocated_width(widget);
  h = gtk_widget_get_allocated_height(widget);
  width = w - RADIUS * 2;
  height = h - RADIUS * 2;
  if (width < RADIUS * 2) width = RADIUS * 2;
  if (height < RADIUS * 2) height = RADIUS * 2;

  if (priv->curve_type == GTK3_CURVE_TYPE_FREE)
    {
      priv->curve_type = GTK3_CURVE_TYPE_LINEAR;
      gtk3_curve_interpolate (widget, width, height);
      priv->curve_type = GTK3_CURVE_TYPE_FREE;
    }
  else
    gtk3_curve_interpolate (widget, width, height);
  DEBUG_INFO("reset vector\n");
  if (gtk_widget_is_visible (GTK_WIDGET (curve)))
    {
      DEBUG_INFO("queue draw\n");
      gtk_widget_queue_draw (GTK_WIDGET (curve));

    }
}

static void
gtk3_curve_interpolate (GtkWidget *widget, gint width, gint height)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  gfloat *vector;
  int i;

  if (width < RADIUS * 2) width = RADIUS * 2;
  vector = g_malloc (width * sizeof (vector[0]));

  gtk3_curve_get_vector (widget, width, vector);

  priv->height = height;
  if (priv->num_points != width)
    {
      priv->num_points = width;
      g_free (priv->point);
      priv->point = g_malloc (priv->num_points * sizeof (priv->point[0]));
    }

  for (i = 0; i < width; ++i)
    {
      priv->point[i].x = RADIUS + i;
      priv->point[i].y = RADIUS + height
                         - project (vector[i], priv->min_y, priv->max_y, height);
    }

  g_free (vector);
}

/* Solve the tridiagonal equation system that determines the second
   derivatives for the interpolation points.  (Based on Numerical
   Recipies 2nd Edition.) */
static void
spline_solve (int n, gfloat x[], gfloat y[], gfloat y2[])
{
  gfloat p, sig, *u;
  gint i, k;

  u = g_malloc ((n - 1) * sizeof (u[0]));

  y2[0] = u[0] = 0.0; /* set lower boundary condition to "natural" */

  for (i = 1; i < n - 1; ++i)
    {
      sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
      p = sig * y2[i - 1] + 2.0;
      y2[i] = (sig - 1.0) / p;
      u[i] = ((y[i + 1] - y[i])
              / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]));
      u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }

  y2[n - 1] = 0.0;
  for (k = n - 2; k >= 0; --k)
    y2[k] = y2[k] * y2[k + 1] + u[k];

  g_free (u);
}

static gfloat
spline_eval (int n, gfloat x[], gfloat y[], gfloat y2[], gfloat val)
{
  gint k_lo, k_hi, k;
  gfloat h, b, a;

  /* do a binary search for the right interval: */
  k_lo = 0;
  k_hi = n - 1;
  while (k_hi - k_lo > 1)
    {
      k = (k_hi + k_lo) / 2;
      if (x[k] > val)
        k_hi = k;
      else
        k_lo = k;
    }

  h = x[k_hi] - x[k_lo];
  g_assert (h > 0.0);

  a = (x[k_hi] - val) / h;
  b = (val - x[k_lo]) / h;
  return a*y[k_lo] + b*y[k_hi] +
         ((a*a*a - a)*y2[k_lo] + (b*b*b - b)*y2[k_hi]) * (h*h)/6.0;
}

static int
project (gfloat value, gfloat min, gfloat max, int norm)
{
  int result = (norm - 1) * ((value - min) / (max - min)) + 0.5;
  return result;
}

static gfloat
unproject (gint value, gfloat min, gfloat max, int norm)
{
  gfloat result = value / (gfloat) (norm - 1) * (max - min) + min;
  return result;
}

/*                          =====================                           */
/* =========================== PUBLIC FUNCTIONS =========================== */
/*                          =====================                           */

void
gtk3_curve_set_gamma (GtkWidget *widget, gfloat gamma)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  gfloat x, one_over_gamma, height;
  Gtk3CurveType old_type;
  gint i;

  if (priv->num_points < 2)
    return;

  old_type = priv->curve_type;
  priv->curve_type = GTK3_CURVE_TYPE_FREE;

  if (gamma <= 0)
    one_over_gamma = 1.0;
  else
    one_over_gamma = 1.0 / gamma;
  height = priv->height;
  for (i = 0; i < priv->num_points; ++i)
    {
      x = (gfloat) i / (priv->num_points - 1);
      priv->point[i].x = RADIUS + i;
      priv->point[i].y =
        RADIUS + (height * (1.0 - pow (x, one_over_gamma)) + 0.5);
    }

  if (old_type != GTK3_CURVE_TYPE_FREE)
    g_signal_emit (curve, curve_type_changed_signal, 0);

  priv->width = priv->num_points;

  DEBUG_INFO("set gamma \n");
  if (gtk_widget_is_visible (GTK_WIDGET (curve)))
    {
      DEBUG_INFO("queue draw \n");
      gtk_widget_queue_draw (GTK_WIDGET (curve));
    }
}

void
gtk3_curve_set_range (GtkWidget *widget,
                      gfloat    min_x,
                      gfloat    max_x,
                      gfloat    min_y,
                      gfloat    max_y)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;

  DEBUG_INFO("set range [S]\n");

  DEBUG_INFO("min_x[%0.1f] max_x[%0.1f]\n", min_x, max_x);
  DEBUG_INFO("min_y[%0.1f] max_y[%0.1f]\n", min_y, max_y);

  g_object_freeze_notify (G_OBJECT (curve));

  if (priv->min_x != min_x)
    {
      priv->min_x = min_x;
      g_object_notify (G_OBJECT (curve), "min-x");
    }

  if (priv->max_x != max_x)
    {
      priv->max_x = max_x;
      g_object_notify (G_OBJECT (curve), "max-x");
    }

  if (priv->min_y != min_y)
    {
      priv->min_y = min_y;
      g_object_notify (G_OBJECT (curve), "min-y");
    }

  if (priv->max_y != max_y)
    {
      priv->max_y = max_y;
      g_object_notify (G_OBJECT (curve), "max-y");
    }

  g_object_thaw_notify (G_OBJECT (curve));

  gtk3_curve_size_graph (curve);
  gtk3_curve_reset_vector (widget);

  DEBUG_INFO("set range [E]\n");
}

void
gtk3_curve_get_vector (GtkWidget *widget, int veclen, gfloat vector[])
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  gfloat rx, ry, dx, dy, min_x, delta_x, *mem, *xv, *yv, *y2v, prev;
  gint dst, i, x, next, num_active_ctlpoints = 0, first_active = -1;

  min_x = priv->min_x;

  if (priv->curve_type != GTK3_CURVE_TYPE_FREE)
    {
      /* count active points: */
      prev = min_x - 1.0;
      for (i = num_active_ctlpoints = 0; i < priv->num_ctlpoints; ++i)
        if (priv->ctlpoint[i][0] > prev)
          {
            if (first_active < 0)
              first_active = i;
            prev = priv->ctlpoint[i][0];
            ++num_active_ctlpoints;
          }

      /* handle degenerate case: */
      if (num_active_ctlpoints < 2)
        {
          if (num_active_ctlpoints > 0)
            ry = priv->ctlpoint[first_active][1];
          else
            ry = priv->min_y;
          if (ry < priv->min_y) ry = priv->min_y;
          if (ry > priv->max_y) ry = priv->max_y;
          for (x = 0; x < veclen; ++x)
            vector[x] = ry;
          return;
        }
    }

  switch (priv->curve_type)
    {
    case GTK3_CURVE_TYPE_SPLINE:
      mem = g_malloc (3 * num_active_ctlpoints * sizeof (gfloat));
      xv  = mem;
      yv  = mem + num_active_ctlpoints;
      y2v = mem + 2*num_active_ctlpoints;

      prev = min_x - 1.0;
      for (i = dst = 0; i < priv->num_ctlpoints; ++i)
        if (priv->ctlpoint[i][0] > prev)
          {
            prev    = priv->ctlpoint[i][0];
            xv[dst] = priv->ctlpoint[i][0];
            yv[dst] = priv->ctlpoint[i][1];
            ++dst;
          }

      spline_solve (num_active_ctlpoints, xv, yv, y2v);

      rx = min_x;
      dx = (priv->max_x - min_x) / (veclen - 1);
      for (x = 0; x < veclen; ++x, rx += dx)
        {
          ry = spline_eval (num_active_ctlpoints, xv, yv, y2v, rx);
          if (ry < priv->min_y) ry = priv->min_y;
          if (ry > priv->max_y) ry = priv->max_y;
          vector[x] = ry;
        }

      g_free (mem);
      break;

    case GTK3_CURVE_TYPE_LINEAR:
      dx = (priv->max_x - min_x) / (veclen - 1);
      rx = min_x;
      ry = priv->min_y;
      dy = 0.0;
      i  = first_active;
      for (x = 0; x < veclen; ++x, rx += dx)
        {
          if (rx >= priv->ctlpoint[i][0])
            {
              if (rx > priv->ctlpoint[i][0])
                ry = priv->min_y;
              dy = 0.0;
              next = i + 1;
              while (next < priv->num_ctlpoints
                     && priv->ctlpoint[next][0] <= priv->ctlpoint[i][0])
                ++next;
              if (next < priv->num_ctlpoints)
                {
                  delta_x = priv->ctlpoint[next][0] - priv->ctlpoint[i][0];
                  dy = ((priv->ctlpoint[next][1] - priv->ctlpoint[i][1])
                        / delta_x);
                  dy *= dx;
                  ry = priv->ctlpoint[i][1];
                  i = next;
                }
            }
          vector[x] = ry;
          ry += dy;
        }
      break;

    case GTK3_CURVE_TYPE_FREE:
      if (priv->point)
        {
          rx = 0.0;
          dx = priv->num_points / (double) veclen;
          for (x = 0; x < veclen; ++x, rx += dx)
            vector[x] = unproject (RADIUS + priv->height - priv->point[(int) rx].y,
                                   priv->min_y, priv->max_y,
                                   priv->height);
        }
      else
        memset (vector, 0, veclen * sizeof (vector[0]));
      break;
    }
}

void
gtk3_curve_set_vector (GtkWidget *widget, int veclen, gfloat vector[])
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  Gtk3CurveType old_type;
  gfloat rx, dx, ry;
  gint i, height;
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (curve));

  old_type = priv->curve_type;
  priv->curve_type = GTK3_CURVE_TYPE_FREE;

  if (priv->point)
    height = gtk_widget_get_allocated_height(GTK_WIDGET (curve)) - RADIUS * 2;
  else
    {
      height = (priv->max_y - priv->min_y);
      if (height > gdk_screen_get_height (screen) / 4)
        height = gdk_screen_get_height (screen) / 4;

      priv->height = height;
      priv->num_points = veclen;
      priv->point = g_malloc (priv->num_points * sizeof (priv->point[0]));
    }
  rx = 0;
  dx = (veclen - 1.0) / (priv->num_points - 1.0);

  for (i = 0; i < priv->num_points; ++i, rx += dx)
    {
      ry = vector[(int) (rx + 0.5)];
      if (ry > priv->max_y) ry = priv->max_y;
      if (ry < priv->min_y) ry = priv->min_y;
      priv->point[i].x = RADIUS + i;
      priv->point[i].y =
        RADIUS + height - project (ry, priv->min_y, priv->max_y, height);
    }
  if (old_type != GTK3_CURVE_TYPE_FREE)
    {
      g_signal_emit (curve, curve_type_changed_signal, 0);
      g_object_notify (G_OBJECT (curve), "curve-type");
    }

  DEBUG_INFO("set vector \n");

  priv->width = priv->num_points;

  if (gtk_widget_is_visible (GTK_WIDGET (curve)))
    {
      DEBUG_INFO("queue draw\n");
      gtk_widget_queue_draw (GTK_WIDGET (curve));
    }
}

void
gtk3_curve_set_curve_type (GtkWidget *widget, Gtk3CurveType new_type)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  gfloat rx, dx;
  gint x, i;

  if (new_type != priv->curve_type)
    {
      gint width, height;

      width = gtk_widget_get_allocated_width(widget) - RADIUS * 2;
      height = gtk_widget_get_allocated_height(widget) - RADIUS * 2;

      if (new_type == GTK3_CURVE_TYPE_FREE)
        {
          gtk3_curve_interpolate (widget, width, height);
          priv->curve_type = new_type;
        }
      else if (priv->curve_type == GTK3_CURVE_TYPE_FREE)
        {
          g_free (priv->ctlpoint);
          priv->num_ctlpoints = 9;
          priv->ctlpoint = g_malloc (priv->num_ctlpoints * sizeof (*priv->ctlpoint));

          rx = 0.0;
          dx = (width - 1) / (gfloat) (priv->num_ctlpoints - 1);

          for (i = 0; i < priv->num_ctlpoints; ++i, rx += dx)
            {
              x = (int) (rx + 0.5);
              priv->ctlpoint[i][0] =
                unproject (x, priv->min_x, priv->max_x, width);
              priv->ctlpoint[i][1] =
                unproject (RADIUS + height - priv->point[x].y,
                           priv->min_y, priv->max_y, height);
            }
          priv->curve_type = new_type;
          gtk3_curve_interpolate (widget, width, height);
        }
      else
        {
          priv->curve_type = new_type;
          gtk3_curve_interpolate (widget, width, height);
        }
      g_signal_emit (curve, curve_type_changed_signal, 0);
      g_object_notify (G_OBJECT (curve), "curve-type");
      DEBUG_INFO("set curve type\n");
      if (gtk_widget_is_visible (GTK_WIDGET (curve)))
        {
          DEBUG_INFO("queue draw\n");
          gtk_widget_queue_draw (GTK_WIDGET (curve));
        }
    }
}

void gtk3_curve_set_color_background (GtkWidget *widget, gfloat r,
                         gfloat g, gfloat b, gfloat a)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  priv->bg_r = r;
  priv->bg_g = g;
  priv->bg_b = b;
  priv->bg_a = a;
  if (gtk_widget_is_visible (widget))
    {
      DEBUG_INFO("queue draw\n");
      gtk_widget_queue_draw (widget);

    }
}

CurveColor gtk3_curve_get_color_background (GtkWidget *widget)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  CurveColor currentColor;
  currentColor.red = priv->bg_r;
  currentColor.green = priv->bg_g;
  currentColor.blue = priv->bg_b;
  currentColor.alpha = priv->bg_a;
  return currentColor;
}

void gtk3_curve_set_color_line (GtkWidget *widget, gfloat r,
                          gfloat g, gfloat b, gfloat a)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  priv->ln_r = r;
  priv->ln_g = g;
  priv->ln_b = b;
  priv->ln_a = a;
  if (gtk_widget_is_visible (widget))
    {
      DEBUG_INFO("queue draw\n");
      gtk_widget_queue_draw (widget);

    }
}

CurveColor gtk3_curve_get_color_line (GtkWidget *widget)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  CurveColor currentColor;
  currentColor.red = priv->ln_r;
  currentColor.green = priv->ln_g;
  currentColor.blue = priv->ln_b;
  currentColor.alpha = priv->ln_a;
  return currentColor;
}

void gtk3_curve_set_color_dot (GtkWidget *widget, gfloat r,
                         gfloat g, gfloat b, gfloat a)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  priv->dt_r = r;
  priv->dt_g = g;
  priv->dt_b = b;
  priv->dt_a = a;
  if (gtk_widget_is_visible (widget))
    {
      DEBUG_INFO("queue draw\n");
      gtk_widget_queue_draw (widget);

    }
}

CurveColor gtk3_curve_get_color_dot (GtkWidget *widget)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  CurveColor currentColor;
  currentColor.red = priv->dt_r;
  currentColor.green = priv->dt_g;
  currentColor.blue = priv->dt_b;
  currentColor.alpha = priv->dt_a;
  return currentColor;
}

void gtk3_curve_set_use_theme_background(GtkWidget *widget, gboolean use)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  priv->use_bg_theme = use;
}

gboolean gtk3_curve_get_use_theme_background(GtkWidget *widget)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  return priv->use_bg_theme;
}

void gtk3_curve_set_grid_size(GtkWidget *widget, Gtk3CurveGridSize size)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  priv->grid_size = size;
}

Gtk3CurveGridSize gtk3_curve_get_grid_size(GtkWidget *widget)
{
  Gtk3Curve *curve = GTK3_CURVE (widget);
  Gtk3CurvePrivate *priv = curve->priv;
  return priv->grid_size;
}