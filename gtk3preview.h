/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */
#ifndef __GTK3_PREVIEW_H__
#define __GTK3_PREVIEW_H__

#include <gtk/gtk.h>


#define GTK3_TYPE_PREVIEW            (gtk3_preview_get_type ())
#define GTK3_PREVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK3_TYPE_PREVIEW, Gtk3Preview))
#define GTK3_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK3_TYPE_PREVIEW, Gtk3PreviewClass))
#define GTK3_IS_PREVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK3_TYPE_PREVIEW))
#define GTK3_IS_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK3_TYPE_PREVIEW))
#define GTK3_PREVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK3_TYPE_PREVIEW, Gtk3PreviewClass))


typedef struct _Gtk3Preview       Gtk3Preview;
typedef struct _Gtk3PreviewInfo   Gtk3PreviewInfo;
typedef union  _Gtk3DitherInfo    Gtk3DitherInfo;
typedef struct _Gtk3PreviewClass  Gtk3PreviewClass;

struct _Gtk3Preview
{
  GtkWidget widget;

  guchar *buffer;
  guint16 buffer_width;
  guint16 buffer_height;

  guint16 bpp;
  guint16 rowstride;

  GdkRgbDither dither;

  guint type : 1;
  guint expand : 1;
};

struct _Gtk3PreviewInfo
{
  guchar *lookup;

  gdouble gamma;
};

union _Gtk3DitherInfo
{
  gushort s[2];
  guchar c[4];
};

struct _Gtk3PreviewClass
{
  GtkWidgetClass parent_class;

  Gtk3PreviewInfo info;

};


GType           gtk3_preview_get_type           (void) G_GNUC_CONST;
void            gtk3_preview_uninit             (void);
GtkWidget*      gtk3_preview_new                (Gtk3PreviewType   type);
void            gtk3_preview_size               (Gtk3Preview      *preview,
						gint             width,
						gint             height);
void            gtk3_preview_put                (Gtk3Preview      *preview,
						GdkWindow       *window,
						GdkGC           *gc,
						gint             srcx,
						gint             srcy,
						gint             destx,
						gint             desty,
						gint             width,
						gint             height);
void            gtk3_preview_draw_row           (GtkPreview      *preview,
						guchar          *data,
						gint             x,
						gint             y,
						gint             w);
void            gtk3_preview_set_expand         (Gtk3Preview      *preview,
						gboolean         expand);

void            gtk3_preview_set_gamma          (double           gamma_);
void            gtk3_preview_set_color_cube     (guint            nred_shades,
						guint            ngreen_shades,
						guint            nblue_shades,
						guint            ngray_shades);
void            gtk3_preview_set_install_cmap   (gint             install_cmap);
void            gtk3_preview_set_reserved       (gint             nreserved);
void            gtk3_preview_set_dither         (Gtk3Preview      *preview,
						GdkRgbDither     dither);
GdkVisual*      gtk3_preview_get_visual         (void);
GdkColormap*    gtk3_preview_get_cmap           (void);
GtkPreviewInfo* gtk3_preview_get_info           (void);

/* This function reinitializes the preview colormap and visual from
 * the current gamma/color_cube/install_cmap settings. It must only
 * be called if there are no previews or users's of the preview
 * colormap in existence.
 */
void            gtk3_preview_reset              (void);



#endif /* __GTK3_PREVIEW_H__ */

