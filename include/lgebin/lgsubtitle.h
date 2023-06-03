/*
 * subtitle.h
 *
 *  Created on: Nov 9, 2012
 *      Author: wonchul
 */

#ifndef SUBTITLE_H_
#define SUBTITLE_H_

#endif /* SUBTITLE_H_ */

#include <gst/gst.h>

G_BEGIN_DECLS

#define LG_TYPE_SUBTITLE \
  (lg_subtitle_get_type())
#define LG_SUBTITLE(obj) \
 (G_TYPE_CHECK_INSTANCE_CAST ((obj), LG_TYPE_SUBTITLE, LGSubtitle))
#define LG_SUBTITLE_CLASS(obj) \
 (G_TYPE_CHECK_CLASS_CAST ((obj), LG_TYPE_SUBTITLE, LGSubtitleClass))
#define LG_IS_SUBTITLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LG_TYPE_SUBTITLE))
#define LG_IS_SUBTITLE_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((obj), LG_TYPE_SUBTITLE))
#define LG_GET_SUBTITLE_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), LG_TYPE_SUBTITLE, LGSubtitleClass))

#define LG_SUBTITLE_GET_LOCK(obj) (((LGSubtitle*)(obj))->lock)
#define LG_SUBTITLE_LOCK(obj) (g_mutex_lock(LG_SUBTITLE_GET_LOCK(obj)))
#define LG_SUBTITLE_UNLOCK(obj) (g_mutex_unlock(LG_SUBTITLE_GET_LOCK(obj)))

static GstStaticPadTemplate lg_subtitle_text_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
        "text_sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS (
        /* text subtitle */
        "application/x-ass; "
          "application/x-ssa; "
          "text/plain; "

          /* bitmap subtitle */
          "video/x-avi-unknown, fourcc=(fourcc)DXSA; "
          "text/x-avi-internal, fourcc=(fourcc)DXSA; "
          "video/x-avi-unknown, fourcc=(fourcc)DXSB; "
          "text/x-avi-internal, fourcc=(fourcc)DXSB; "));

static GstStaticPadTemplate lg_subtitle_video_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
        "video_sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate lg_subtitle_src_factory = GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

typedef struct _LGSubtitleClass LGSubtitleClass;
typedef struct _LGSubtitle LGSubtitle;

typedef enum
{
  /* text type */
  gst_subtitle_type_unknown,
  gst_subtitle_type_text_plain,
  gst_subtitle_type_ass,

  /* bitmap type */
  gst_subtitle_type_DXSB,
  gst_subtitle_type_DXSA,
} gst_subtitle_type_t;

struct _LGSubtitle
{
  GstElement parent;

  GstPad *video_sinkpad;
  GstPad *text_sinkpad;
  GstPad *srcpad;

  GstSegment segment;
  GstSegment text_segment;
  GstBuffer *text_buffer;
  gboolean text_linked;
  gboolean video_flushing;
  gboolean video_eos;
  gboolean text_flushing;
  gboolean text_eos;

  GCond *cond; // ?
  gboolean silent;

  gboolean wait_text;
  gboolean need_render;

  gboolean subtitle_on;

  gst_subtitle_type_t type;

  gint wind_width;
  gint wind_height;

  GMutex *lock;
};

struct _LGSubtitleClass
{
  GstElementClass parent_class;
};

GType
lg_subtitle_get_type (void);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_INFO_OBJECT (plugin, "initializing subtitle element plug-in.");
  if (!gst_element_register (
      plugin,
      "subtitle",
      GST_RANK_PRIMARY,
      lg_subtitle_get_type ())) return FALSE;

  return TRUE;
}

#ifndef PACKAGE
#define PACKAGE "subtitle"
#endif
GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "subtitle",
    "LG subtitle element",
    plugin_init,
    "0.0.0",
    "LGPL",
    PACKAGE,
    "http://lge.com/")

G_END_DECLS
