/*
 * subtitle.c
 *
 *  Created on: Nov 9, 2012
 *      Author: wonchul
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>
#include <gst/controller/gstcontroller.h>
#include <dlfcn.h>
#include "lgsubtitle.h"

GST_DEBUG_CATEGORY_STATIC ( gst_subtitle_debug);
#define GST_CAT_DEFAULT gst_subtitle_debug

#define LG_SUBTITLE_GET_COND(ov) (((LGSubtitle *)ov)->cond)
#define LG_SUBTITLE_WAIT(ov)     (g_cond_wait (LG_SUBTITLE_GET_COND (ov), GST_OBJECT_GET_LOCK (ov)))
#define LG_SUBTITLE_SIGNAL(ov)   (g_cond_signal (LG_SUBTITLE_GET_COND (ov)))
#define LG_SUBTITLE_BROADCAST(ov)(g_cond_broadcast (LG_SUBTITLE_GET_COND (ov)))

static GstStateChangeReturn
lg_subtitle_change_state (GstElement * element, GstStateChange transition);

static GstCaps *
lg_subtitle_getcaps (GstPad * pad);
static gboolean
lg_subtitle_setcaps (GstPad * pad, GstCaps * caps);
static gboolean
lg_subtitle_src_event (GstPad * pad, GstEvent * event);
static gboolean
lg_subtitle_src_query (GstPad * pad, GstQuery * query);

static gboolean
lg_subtitle_video_event (GstPad * pad, GstEvent * event);
static GstFlowReturn
lg_subtitle_video_chain (GstPad * pad, GstBuffer * buffer);
static GstFlowReturn
lg_subtitle_video_bufferalloc (GstPad * pad, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buffer);

static gboolean
lg_subtitle_text_event (GstPad * pad, GstEvent * event);
static GstFlowReturn
lg_subtitle_text_chain (GstPad * pad, GstBuffer * buffer);
static GstPadLinkReturn
lg_subtitle_text_pad_link (GstPad * pad, GstPad * peer);
static void
lg_subtitle_text_pad_unlink (GstPad * pad);
static void
lg_subtitle_pop_text (LGSubtitle * subt);

static void
lg_subtitle_class_init (LGSubtitleClass * klass);
//static void
//lg_subtitle_init (LGSubtitle * klass);
static void
lg_subtitle_finalize (GObject *self);
static void
lg_subtitle_get_property (GObject *object, guint property_id, GValue *value,
    GParamSpec *pspec);
static void
lg_subtitle_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec);
static GstFlowReturn
lg_subtitle_text_render (GstPad *pad, GstBuffer *buf);

static gboolean
gst_subtitle_configure_caps (GstPad *pad, LGSubtitle *subt);

static gboolean
read_timestamp (const gchar *buf, int len, GstClockTime *ts, GstClockTime *dur);

static void
(*__LMF_ParseInternalSubtitle) (void *);
static void
(*__LMF_ParseBitmapSubtitle) (void *);
static int
(*__LMF_Get_subtitleExist) (void);
static void
(*__LMF_INTSUBT_IsExist) (void *);

/**
 * Internal Subtitle Parameter
 */

typedef struct
{
  int startTime;
  int endTime;
  uint nLen;
  char *szBuf;
} LMF_INTERNAL_SUBTITLE_PARAM_T; //inyoung.choi �߰�

/**
 * Internal Subtitle Bitmap Parameter
 */

typedef struct
{
  int startTime;
  int endTime;
  int bmp_width;
  int bmp_height;
  int wind_width;
  int wind_height;
  unsigned char *bmp;
} LMF_SUBTITLE_BITMAP_PARAM_T; //inyoung.choi �߰�


static void
_do_init (GType type)
{
}

GST_BOILERPLATE_FULL (LGSubtitle, lg_subtitle, GstElement, GST_TYPE_ELEMENT,
    _do_init);

/* properties */
enum
{
  PROP_0,
  PROP_LAST
};

/* signals */
enum
{
  SIGNAL_LAST
};

//static GParamSpec *props[PROP_LAST];
////static guint signals[SIGNAL_LAST];

static void
(*__LMF_INTSUBT_IsExist) (void *);

static void
lg_subtitle_class_init (LGSubtitleClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) (klass);
  gstelement_class = (GstElementClass *) (klass);

  gobject_class->finalize = lg_subtitle_finalize;
  gobject_class->set_property = lg_subtitle_set_property;
  gobject_class->get_property = lg_subtitle_get_property;

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (lg_subtitle_change_state);
  gst_element_class_add_static_pad_template (
      gstelement_class,
      &lg_subtitle_video_sink_factory);
  gst_element_class_add_static_pad_template (
      gstelement_class,
      &lg_subtitle_text_sink_factory);

  gst_element_class_add_static_pad_template (
      gstelement_class,
      &lg_subtitle_src_factory);
}

static void
lg_subtitle_init (LGSubtitle * subt, LGSubtitleClass * subt_class)
{
  GST_DEBUG_CATEGORY_INIT (
      gst_subtitle_debug,
      "subtitle",
      0,
      "Template subtitle");

  subt->video_sinkpad = gst_pad_new_from_static_template (
      &lg_subtitle_video_sink_factory,
      "video_sink");
  gst_pad_set_getcaps_function (
      subt->video_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_getcaps));
  gst_pad_set_setcaps_function (
      subt->video_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_setcaps));
  gst_pad_set_event_function (
      subt->video_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_video_event));
  gst_pad_set_chain_function (
      subt->video_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_video_chain));
  gst_pad_set_bufferalloc_function (
      subt->video_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_video_bufferalloc));
  gst_element_add_pad (GST_ELEMENT (subt), subt->video_sinkpad);

  subt->text_sinkpad = gst_pad_new_from_static_template (
      &lg_subtitle_text_sink_factory,
      "text_sink");
  gst_pad_set_event_function (
      subt->text_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_text_event));
  gst_pad_set_chain_function (
      subt->text_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_text_chain));
  gst_pad_set_link_function (
      subt->text_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_text_pad_link));
  gst_pad_set_unlink_function (
      subt->text_sinkpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_text_pad_unlink));
  gst_element_add_pad (GST_ELEMENT (subt), subt->text_sinkpad);

  subt->srcpad = gst_pad_new_from_static_template (
      &lg_subtitle_src_factory,
      "src");
  gst_pad_set_getcaps_function (
      subt->srcpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_getcaps));
  gst_pad_set_event_function (
      subt->srcpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_src_event));
  gst_pad_set_query_function (
      subt->srcpad,
      GST_DEBUG_FUNCPTR (lg_subtitle_src_query));
  gst_element_add_pad (GST_ELEMENT (subt), subt->srcpad);

  subt->subtitle_on = FALSE;
  if (!__LMF_INTSUBT_IsExist) __LMF_INTSUBT_IsExist = (void
  (*) (void *)) dlsym (NULL, "LMF_INTSUBT_IsExist");

  subt->lock = g_mutex_new ();

  subt->silent = FALSE;
  subt->wait_text = TRUE;

  subt->text_buffer = NULL;
  subt->text_linked = FALSE;
  subt->cond = g_cond_new ();
  subt->need_render = TRUE;
  subt->wait_text = TRUE;
  gst_segment_init (&subt->segment, GST_FORMAT_TIME);
}

static void
lg_subtitle_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  //gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_details_simple (
      element_class,
      "Subtitle element",
      "replaced sink/SUBTITLE",
      "rendering subtitle",
      "Wonchul Lee <wonchul86.lee@lge.com>");

}

static void
lg_subtitle_finalize (GObject *self)
{
  LGSubtitle *subt = LG_SUBTITLE(self);

  g_mutex_free (subt->lock);

  if (subt->text_buffer)
  {
    gst_buffer_unref (subt->text_buffer);
    subt->text_buffer = NULL;
  }
  if (subt->cond)
  {
    g_cond_free (subt->cond);
    subt->cond = NULL;
  }
  /* call parent finalize method (always do this!) */

  //G_OBJECT_CLASS (lg_subtitle_parent_class) ->finalize (self);
  G_OBJECT_CLASS (parent_class) ->finalize (self);
}

static gboolean
lg_subtitle_setcaps (GstPad * pad, GstCaps * caps)
{
  LGSubtitle *subt;
  gboolean ret = FALSE;

  if (!GST_PAD_IS_SINK (pad)) return TRUE;

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  if (G_UNLIKELY (!subt)) return FALSE;

  ret = gst_pad_set_caps (subt->srcpad, caps);

  gst_object_unref (subt);

  return ret;
}

static void
lg_subtitle_get_property (GObject *object, guint property_id, GValue *value,
    GParamSpec *pspec)
{
  //LGSubtitle *subt = LG_SUBTITLE(object);

  switch (property_id)
  {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
lg_subtitle_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
  //LGSubtitle *subt = LG_SUBTITLE(object);

  switch (property_id)
  {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static gboolean
lg_subtitle_src_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = FALSE;
  LGSubtitle *subt = NULL;

  subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  if (G_UNLIKELY (!subt))
  {
    gst_event_unref (event);
    return FALSE;
  }

  switch (GST_EVENT_TYPE (event))
  {
  case GST_EVENT_SEEK:
  {
    GstSeekFlags flags;

    /* We don't handle seek if we have not text pad */
    if (!subt->text_linked)
    {
      GST_DEBUG_OBJECT (subt, "seek received, pushing upstream");
      ret = gst_pad_push_event (subt->video_sinkpad, event);
      goto beach;
    }

    GST_DEBUG_OBJECT (subt, "seek received, driving from here");

    gst_event_parse_seek (event, NULL, NULL, &flags, NULL, NULL, NULL, NULL);

    /* Flush downstream, only for flushing seek */
    if (flags & GST_SEEK_FLAG_FLUSH)
      gst_pad_push_event (subt->srcpad, gst_event_new_flush_start ());

    /* Mark ourself as flushing, unblock chains */
    GST_OBJECT_LOCK (subt);
    subt->video_flushing = TRUE;
    subt->text_flushing = TRUE;
    lg_subtitle_pop_text (subt);
    GST_OBJECT_UNLOCK (subt);

    /* Seek on each sink pad */
    gst_event_ref (event);
    ret = gst_pad_push_event (subt->video_sinkpad, event);
    if (ret)
    {
      ret = gst_pad_push_event (subt->text_sinkpad, event);
    }
    else
    {
      gst_event_unref (event);
    }
    break;
  }
  default:
    if (subt->text_linked)
    {
      gst_event_ref (event);
      ret = gst_pad_push_event (subt->video_sinkpad, event);
      gst_pad_push_event (subt->text_sinkpad, event);
    }
    else
    {
      ret = gst_pad_push_event (subt->video_sinkpad, event);
    }
    break;
  }

  beach: gst_object_unref (subt);

  return ret;
}

static gboolean
lg_subtitle_src_query (GstPad * pad, GstQuery * query)
{
  gboolean ret = FALSE;
  LGSubtitle *subt = NULL;

  subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  if (G_UNLIKELY (!subt)) return FALSE;

  ret = gst_pad_peer_query (subt->video_sinkpad, query);

  gst_object_unref (subt);

  return ret;
}

static GstCaps *
lg_subtitle_getcaps (GstPad * pad)
{
  LGSubtitle *subt;
  GstPad *otherpad;
  GstCaps *caps;

  subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  if (G_UNLIKELY (!subt))
    return gst_caps_copy (gst_pad_get_pad_template_caps (pad));

  if (pad == subt->srcpad)
    otherpad = subt->video_sinkpad;
  else
    otherpad = subt->srcpad;

  /* we can do what the peer can */
  caps = gst_pad_peer_get_caps (otherpad);
  if (caps)
  {
    GstCaps *temp;
    const GstCaps *templ;

    GST_DEBUG_OBJECT (pad, "peer caps  %" GST_PTR_FORMAT, caps);

    /* filtered against our padtemplate */
    templ = gst_pad_get_pad_template_caps (otherpad);
    GST_DEBUG_OBJECT (pad, "our template  %" GST_PTR_FORMAT, templ);
    temp = gst_caps_intersect (caps, templ);
    GST_DEBUG_OBJECT (pad, "intersected %" GST_PTR_FORMAT, temp);
    gst_caps_unref (caps);
    /* this is what we can do */
    caps = temp;
  }
  else
  {
    /* no peer, our padtemplate is enough then */
    caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
  }

  GST_DEBUG_OBJECT (subt, "returning  %" GST_PTR_FORMAT, caps);

  gst_object_unref (subt);

  return caps;
}

static GstPadLinkReturn
lg_subtitle_text_pad_link (GstPad * pad, GstPad * peer)
{
  LGSubtitle *subt;

  subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  if (G_UNLIKELY (!subt)) return GST_PAD_LINK_REFUSED;

  GST_DEBUG_OBJECT (subt, "Text pad linked");

  subt->text_linked = TRUE;

  gst_object_unref (subt);

  return GST_PAD_LINK_OK;
}

static void
lg_subtitle_text_pad_unlink (GstPad * pad)
{
  LGSubtitle *subt;

  /* don't use gst_pad_get_parent() here, will deadlock */
  subt = LG_SUBTITLE (GST_PAD_PARENT (pad));

  GST_DEBUG_OBJECT (subt, "Text pad unlinked");

  subt->text_linked = FALSE;

  gst_segment_init (&subt->text_segment, GST_FORMAT_UNDEFINED);
}

static gboolean
lg_subtitle_text_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = FALSE;
  LGSubtitle *subt = NULL;

  subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  if (G_UNLIKELY (!subt))
  {
    gst_event_unref (event);
    return FALSE;
  }

  GST_LOG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event))
  {
  case GST_EVENT_NEWSEGMENT:
  {
    GstFormat fmt;
    gboolean update;
    gdouble rate, applied_rate;
    gint64 cur, stop, time;

    subt->text_eos = FALSE;

    gst_event_parse_new_segment_full (
        event,
        &update,
        &rate,
        &applied_rate,
        &fmt,
        &cur,
        &stop,
        &time);

    if (fmt == GST_FORMAT_TIME)
    {
      GST_OBJECT_LOCK (subt);
      gst_segment_set_newsegment_full (
          &subt->text_segment,
          update,
          rate,
          applied_rate,
          GST_FORMAT_TIME,
          cur,
          stop,
          time);
      GST_DEBUG_OBJECT (subt, "TEXT SEGMENT now: %" GST_SEGMENT_FORMAT,
          &subt->text_segment);
      GST_OBJECT_UNLOCK (subt);
    }
    else
    {
      GST_ELEMENT_WARNING (
          subt,
          STREAM,
          MUX,
          (NULL),
          ("received non-TIME newsegment event on text input"));
    }

    gst_event_unref (event);
    ret = TRUE;

    /* wake up the video chain, it might be waiting for a text buffer or
     * a text segment update */
    GST_OBJECT_LOCK (subt);
    LG_SUBTITLE_BROADCAST (subt);
    GST_OBJECT_UNLOCK (subt);
    break;
  }
  case GST_EVENT_FLUSH_STOP:
    GST_OBJECT_LOCK (subt);
    GST_INFO_OBJECT (subt, "text flush stop");
    subt->text_flushing = FALSE;
    subt->text_eos = FALSE;
    lg_subtitle_pop_text (subt);
    gst_segment_init (&subt->text_segment, GST_FORMAT_TIME);
    GST_OBJECT_UNLOCK (subt);
    gst_event_unref (event);
    ret = TRUE;
    break;
  case GST_EVENT_FLUSH_START:
    GST_OBJECT_LOCK (subt);
    GST_INFO_OBJECT (subt, "text flush start");
    subt->text_flushing = TRUE;
    LG_SUBTITLE_BROADCAST (subt);
    GST_OBJECT_UNLOCK (subt);
    gst_event_unref (event);
    ret = TRUE;
    break;
  case GST_EVENT_EOS:
    GST_OBJECT_LOCK (subt);
    subt->text_eos = TRUE;
    GST_INFO_OBJECT (subt, "text EOS");
    /* wake up the video chain, it might be waiting for a text buffer or
     * a text segment update */
    LG_SUBTITLE_BROADCAST (subt);
    GST_OBJECT_UNLOCK (subt);
    gst_event_unref (event);
    ret = TRUE;
    break;
  default:
    ret = gst_pad_event_default (pad, event);
    break;
  }

  gst_object_unref (subt);

  return ret;
}

static gboolean
lg_subtitle_video_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = FALSE;
  LGSubtitle *subt = NULL;

  subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  if (G_UNLIKELY (!subt))
  {
    gst_event_unref (event);
    return FALSE;
  }

  GST_DEBUG_OBJECT (pad, "received event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event))
  {
  case GST_EVENT_NEWSEGMENT:
  {
    GstFormat format;
    gdouble rate;
    gint64 start, stop, time;
    gboolean update;

    GST_DEBUG_OBJECT (subt, "received new segment");

    gst_event_parse_new_segment (
        event,
        &update,
        &rate,
        &format,
        &start,
        &stop,
        &time);

    if (format == GST_FORMAT_TIME)
    {
      GST_DEBUG_OBJECT (subt, "VIDEO SEGMENT now: %" GST_SEGMENT_FORMAT,
          &subt->segment);

      gst_segment_set_newsegment (
          &subt->segment,
          update,
          rate,
          format,
          start,
          stop,
          time);
    }
    else
    {
      GST_ELEMENT_WARNING (
          subt,
          STREAM,
          MUX,
          (NULL),
          ("received non-TIME newsegment event on video input"));
    }

    ret = gst_pad_event_default (pad, event);
    break;
  }
  case GST_EVENT_EOS:
    GST_OBJECT_LOCK (subt);
    GST_INFO_OBJECT (subt, "video EOS");
    subt->video_eos = TRUE;
    GST_OBJECT_UNLOCK (subt);
    ret = gst_pad_event_default (pad, event);
    break;
  case GST_EVENT_FLUSH_START:
    GST_OBJECT_LOCK (subt);
    GST_INFO_OBJECT (subt, "video flush start");
    subt->video_flushing = TRUE;
    LG_SUBTITLE_BROADCAST (subt);
    GST_OBJECT_UNLOCK (subt);
    ret = gst_pad_event_default (pad, event);
    break;
  case GST_EVENT_FLUSH_STOP:
    GST_OBJECT_LOCK (subt);
    GST_INFO_OBJECT (subt, "video flush stop");
    subt->video_flushing = FALSE;
    subt->video_eos = FALSE;
    gst_segment_init (&subt->segment, GST_FORMAT_TIME);
    GST_OBJECT_UNLOCK (subt);
    ret = gst_pad_event_default (pad, event);
    break;
  default:
    ret = gst_pad_event_default (pad, event);
    break;
  }

  gst_object_unref (subt);

  return ret;
}

static GstFlowReturn
lg_subtitle_video_bufferalloc (GstPad * pad, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buffer)
{
  LGSubtitle *subt = LG_SUBTITLE (gst_pad_get_parent (pad));
  GstFlowReturn ret = GST_FLOW_WRONG_STATE;
  GstPad *allocpad;

  if (G_UNLIKELY (!subt)) return GST_FLOW_WRONG_STATE;

  GST_OBJECT_LOCK (subt);
  allocpad = subt->srcpad ? gst_object_ref (subt->srcpad) : NULL;
  GST_OBJECT_UNLOCK (subt);

  if (allocpad)
  {
    ret = gst_pad_alloc_buffer (allocpad, offset, size, caps, buffer);
    gst_object_unref (allocpad);
  }

  gst_object_unref (subt);
  return ret;
}

/* Called with lock held */
static void
lg_subtitle_pop_text (LGSubtitle * subt)
{
  g_return_if_fail (LG_IS_SUBTITLE(subt));

  if (subt->text_buffer)
  {
    GST_DEBUG_OBJECT (subt, "releasing text buffer %p", subt->text_buffer);
    gst_buffer_unref (subt->text_buffer);
    subt->text_buffer = NULL;
  }

  /* Let the text task know we used that buffer */LG_SUBTITLE_BROADCAST (subt);
}

/* We receive text buffers here. If they are out of segment we just ignore them.
 If the buffer is in our segment we keep it internally except if another one
 is already waiting here, in that case we wait that it gets kicked out */
static GstFlowReturn
lg_subtitle_text_chain (GstPad * pad, GstBuffer * buffer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  LGSubtitle *subt = NULL;
  gboolean in_seg = FALSE;
  gint64 clip_start = 0, clip_stop = 0;

  subt = LG_SUBTITLE (GST_PAD_PARENT (pad));

  GST_OBJECT_LOCK (subt);

  if (subt->text_flushing)
  {
    GST_OBJECT_UNLOCK (subt);
    ret = GST_FLOW_WRONG_STATE;
    GST_LOG_OBJECT (subt, "text flushing");
    goto beach;
  }

  if (subt->text_eos)
  {
    GST_OBJECT_UNLOCK (subt);
    ret = GST_FLOW_UNEXPECTED;
    GST_LOG_OBJECT (subt, "text EOS");
    goto beach;
  }

  GST_LOG_OBJECT (subt, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &subt->segment,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer) +
          GST_BUFFER_DURATION (buffer)));

  if (G_LIKELY (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)))
  {
    GstClockTime stop;

    if (G_LIKELY (GST_BUFFER_DURATION_IS_VALID (buffer)))
      stop = GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
    else
      stop = GST_CLOCK_TIME_NONE;

    in_seg = gst_segment_clip (
        &subt->text_segment,
        GST_FORMAT_TIME,
        GST_BUFFER_TIMESTAMP (buffer),
        stop,
        &clip_start,
        &clip_stop);
  }
  else
  {
    in_seg = TRUE;
  }

  if (in_seg)
  {
    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      GST_BUFFER_TIMESTAMP ( buffer) = clip_start;
    else if (GST_BUFFER_DURATION_IS_VALID (buffer))
      GST_BUFFER_DURATION ( buffer) = clip_stop - clip_start;

    if (subt->text_buffer
        && (!GST_BUFFER_TIMESTAMP_IS_VALID (subt->text_buffer)
            || !GST_BUFFER_DURATION_IS_VALID (subt->text_buffer)))
    {
      lg_subtitle_pop_text (subt);
    }
    else
    {

      /* Wait for the previous buffer to go away */
      while (subt->text_buffer != NULL)
      {
        GST_DEBUG (
            "Pad %s:%s has a buffer queued, waiting",
            GST_DEBUG_PAD_NAME (pad));
        LG_SUBTITLE_WAIT (subt);
        GST_DEBUG ("Pad %s:%s resuming", GST_DEBUG_PAD_NAME (pad));
        if (subt->text_flushing)
        {
          GST_OBJECT_UNLOCK (subt);
          ret = GST_FLOW_WRONG_STATE;
          goto beach;
        }
      }
    }

    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))
      gst_segment_set_last_stop (
          &subt->text_segment,
          GST_FORMAT_TIME,
          clip_start);

    subt->text_buffer = gst_buffer_ref (buffer);
    /* That's a new text buffer we need to render */
    subt->need_render = TRUE;

    /* in case the video chain is waiting for a text buffer, wake it up */LG_SUBTITLE_BROADCAST (subt);
  }

  GST_OBJECT_UNLOCK (subt);

  beach:

  gst_buffer_unref (buffer);
  return ret;
}

static GstFlowReturn
lg_subtitle_video_chain (GstPad * pad, GstBuffer * buffer)
{
  LGSubtitleClass *klass;
  LGSubtitle *subt;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean in_seg = FALSE;
  gint64 start, stop, clip_start = 0, clip_stop = 0;
  gchar *text = NULL;

  subt = LG_SUBTITLE (GST_PAD_PARENT (pad));
  klass = LG_GET_SUBTITLE_CLASS (subt);

  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) goto missing_timestamp;

  /* ignore buffers that are outside of the current segment */
  start = GST_BUFFER_TIMESTAMP (buffer);

  if (!GST_BUFFER_DURATION_IS_VALID (buffer))
  {
    stop = GST_CLOCK_TIME_NONE;
  }
  else
  {
    stop = start + GST_BUFFER_DURATION (buffer);
  }

  GST_LOG_OBJECT (subt, "%" GST_SEGMENT_FORMAT "  BUFFER: ts=%"
      GST_TIME_FORMAT ", end=%" GST_TIME_FORMAT, &subt->segment,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

  /* segment_clip() will adjust start unconditionally to segment_start if
   * no stop time is provided, so handle this ourselves */
  if (stop == GST_CLOCK_TIME_NONE && start < subt->segment.start)
    goto out_of_segment;

  in_seg = gst_segment_clip (
      &subt->segment,
      GST_FORMAT_TIME,
      start,
      stop,
      &clip_start,
      &clip_stop);

  if (!in_seg) goto out_of_segment;

  /* if the buffer is only partially in the segment, fix up stamps */
  if (clip_start != start || (stop != -1 && clip_stop != stop))
  {
    GST_DEBUG_OBJECT (subt, "clipping buffer timestamp/duration to segment");
    buffer = gst_buffer_make_metadata_writable (buffer);
    GST_BUFFER_TIMESTAMP ( buffer) = clip_start;
    if (stop != -1) GST_BUFFER_DURATION ( buffer) = clip_stop - clip_start;
  }

  /* now, after we've done the clipping, fix up end time if there's no
   * duration (we only use those estimated values internally though, we
   * don't want to set bogus values on the buffer itself) */
  if (stop == -1)
  {
    GstStructure *s;
    gint fps_num, fps_denom;

    s = gst_caps_get_structure (GST_PAD_CAPS (pad), 0);
    if (gst_structure_get_fraction (s, "framerate", &fps_num, &fps_denom)
        && fps_num && fps_denom)
    {
      GST_DEBUG_OBJECT (subt, "estimating duration based on framerate");
      stop = start + gst_util_uint64_scale_int (GST_SECOND, fps_denom, fps_num);
    }
    else
    {
      GST_WARNING_OBJECT (subt, "no duration, assuming minimal duration");
      stop = start + 1; /* we need to assume some interval */
    }
  }

  //gst_object_sync_values (G_OBJECT (subt), GST_BUFFER_TIMESTAMP (buffer));

  wait_for_text_buf:

  GST_OBJECT_LOCK (subt);

  if (subt->video_flushing) goto flushing;

  if (subt->video_eos) goto have_eos;

  if (subt->silent && !subt->text_linked)
  {
    GST_OBJECT_UNLOCK (subt);
    ret = gst_pad_push (subt->srcpad, buffer);

    /* Update last_stop */
    gst_segment_set_last_stop (&subt->segment, GST_FORMAT_TIME, clip_start);

    return ret;
  }

  /* Text pad not linked, just push a buffer. */
  if (!subt->text_linked)
  {
    ret = gst_pad_push (subt->srcpad, buffer);
  }
  else
  {
    /* Text pad linked, check if we have a text buffer queued */
    if (subt->text_buffer)
    {
      gboolean pop_text = FALSE, valid_text_time = TRUE;
      GstClockTime text_start = GST_CLOCK_TIME_NONE;
      GstClockTime text_end = GST_CLOCK_TIME_NONE;
      GstClockTime text_running_time = GST_CLOCK_TIME_NONE;
      GstClockTime text_running_time_end = GST_CLOCK_TIME_NONE;
      GstClockTime vid_running_time, vid_running_time_end;

      /* if the text buffer isn't stamped right, pop it off the
       * queue and display it for the current video frame only */
      if (!GST_BUFFER_TIMESTAMP_IS_VALID (subt->text_buffer)
          || !GST_BUFFER_DURATION_IS_VALID (subt->text_buffer))
      {
        GST_WARNING_OBJECT (
            subt,
            "Got text buffer with invalid timestamp or duration");
        valid_text_time = FALSE;
      }
      else
      {
        text_start = GST_BUFFER_TIMESTAMP (subt->text_buffer);
        text_end = text_start + GST_BUFFER_DURATION (subt->text_buffer);
      }

      vid_running_time = gst_segment_to_running_time (
          &subt->segment,
          GST_FORMAT_TIME,
          start);
      vid_running_time_end = gst_segment_to_running_time (
          &subt->segment,
          GST_FORMAT_TIME,
          stop);

      /* If timestamp and duration are valid */
      if (valid_text_time)
      {
        text_running_time = gst_segment_to_running_time (
            &subt->segment,
            GST_FORMAT_TIME,
            text_start);
        text_running_time_end = gst_segment_to_running_time (
            &subt->segment,
            GST_FORMAT_TIME,
            text_end);
      }

      GST_LOG_OBJECT (subt, "T: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (text_running_time),
          GST_TIME_ARGS (text_running_time_end));
      GST_LOG_OBJECT (subt, "V: %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
          GST_TIME_ARGS (vid_running_time),
          GST_TIME_ARGS (vid_running_time_end));

      /* Text too old or in the future */
      if (valid_text_time && text_running_time_end <= vid_running_time)
      {
        /* text buffer too old, get rid of it and do nothing  */
        GST_LOG_OBJECT (subt, "text buffer too old, popping");
        pop_text = FALSE;
        lg_subtitle_pop_text (subt);
        GST_OBJECT_UNLOCK (subt);
        goto wait_for_text_buf;
      }
      else if (valid_text_time && vid_running_time_end <= text_running_time)
      {
        GST_LOG_OBJECT (subt, "text in future, pushing video buf");
        GST_OBJECT_UNLOCK (subt);
        /* Push the video frame */
        ret = gst_pad_push (subt->srcpad, buffer);
      }
      else if (subt->silent)
      {
        GST_LOG_OBJECT (subt, "silent enabled, pushing video buf");
        GST_OBJECT_UNLOCK (subt);
        /* Push the video frame */
        ret = gst_pad_push (subt->srcpad, buffer);
      }
      else
      {

        GST_OBJECT_UNLOCK (subt);
        lg_subtitle_text_render (subt->text_sinkpad, subt->text_buffer);
        ret = gst_pad_push (subt->srcpad, buffer);

        //if (valid_text_time && text_running_time_end <= vid_running_time_end) {
        //  GST_LOG_OBJECT (subt, "text buffer not needed any longer");
        pop_text = TRUE;
        //}
      }
      if (pop_text)
      {
        GST_OBJECT_LOCK (subt);
        lg_subtitle_pop_text (subt);
        GST_OBJECT_UNLOCK (subt);
      }
    }
    else
    {
      gboolean wait_for_text_buf = TRUE;

      if (subt->text_eos) wait_for_text_buf = FALSE;

      if (!subt->wait_text) wait_for_text_buf = FALSE;

      /* Text pad linked, but no text buffer available - what now? */
      if (subt->text_segment.format == GST_FORMAT_TIME)
      {
        GstClockTime text_start_running_time, text_last_stop_running_time;
        GstClockTime vid_running_time;

        vid_running_time = gst_segment_to_running_time (
            &subt->segment,
            GST_FORMAT_TIME,
            GST_BUFFER_TIMESTAMP (buffer));
        text_start_running_time = gst_segment_to_running_time (
            &subt->text_segment,
            GST_FORMAT_TIME,
            subt->text_segment.start);
        text_last_stop_running_time = gst_segment_to_running_time (
            &subt->text_segment,
            GST_FORMAT_TIME,
            subt->text_segment.last_stop);

        if ( (GST_CLOCK_TIME_IS_VALID (text_start_running_time)
            && vid_running_time < text_start_running_time)
            || (GST_CLOCK_TIME_IS_VALID (text_last_stop_running_time)
                && vid_running_time < text_last_stop_running_time))
        {
          wait_for_text_buf = FALSE;
        }
      }

      if (wait_for_text_buf)
      {
        GST_DEBUG_OBJECT (subt, "no text buffer, need to wait for one");
        LG_SUBTITLE_WAIT (subt);
        GST_DEBUG_OBJECT (subt, "resuming");
        GST_OBJECT_UNLOCK (subt);
        goto wait_for_text_buf;
      }
      else
      {
        GST_OBJECT_UNLOCK (subt);
        GST_LOG_OBJECT (subt, "no need to wait for a text buffer");
        ret = gst_pad_push (subt->srcpad, buffer);
      }
    }
  }

  g_free (text);

  /* Update last_stop */
  gst_segment_set_last_stop (&subt->segment, GST_FORMAT_TIME, clip_start);

  return ret;

  missing_timestamp:
  {
    GST_WARNING_OBJECT (subt, "buffer without timestamp, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

  flushing:
  {
    GST_OBJECT_UNLOCK (subt);
    GST_DEBUG_OBJECT (subt, "flushing, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_WRONG_STATE;
  }
  have_eos:
  {
    GST_OBJECT_UNLOCK (subt);
    GST_DEBUG_OBJECT (subt, "eos, discarding buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_UNEXPECTED;
  }
  out_of_segment:
  {
    GST_DEBUG_OBJECT (subt, "buffer out of segment, discarding");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}

static GstStateChangeReturn
lg_subtitle_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  LGSubtitle *subt = LG_SUBTITLE (element);

  switch (transition)
  {
  case GST_STATE_CHANGE_PAUSED_TO_READY:
    GST_OBJECT_LOCK (subt);
    subt->text_flushing = TRUE;
    subt->video_flushing = TRUE;
    /* pop_text will broadcast on the GCond and thus also make the video
     * chain exit if it's waiting for a text buffer */
    lg_subtitle_pop_text (subt);
    GST_OBJECT_UNLOCK (subt);
    break;
  default:
    break;
  }

  ret = parent_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE) return ret;

  switch (transition)
  {
  case GST_STATE_CHANGE_READY_TO_PAUSED:
    GST_OBJECT_LOCK (subt);
    subt->text_flushing = FALSE;
    subt->video_flushing = FALSE;
    subt->video_eos = FALSE;
    subt->text_eos = FALSE;
    gst_segment_init (&subt->segment, GST_FORMAT_TIME);
    gst_segment_init (&subt->text_segment, GST_FORMAT_TIME);
    GST_OBJECT_UNLOCK (subt);
    break;
  default:
    break;
  }

  return ret;
}

static void
dump (const char *name, const unsigned char *data, int len)
{
  GST_DEBUG ("This is dump\n");

  int a, b;

  if (name != NULL && name[0] != 0) puts (name);

  for (a = 0, b = 0; a < len;)
  {
    if (a % 16 == 0) printf ("%04x:", a);
    printf (" %02x", data[a]);
    a++;
    if (a % 16 == 0)
    {
      fputs ("  ", stdout);
      for (; b < a; b++)
        putchar ( (' ' <= data[b] && data[b] <= '~') ? data[b] : '.');
      fputs ("\n", stdout);
    }
  }

  if (a % 16 != 0)
  {
    for (; a % 16 != 0; a++)
      fputs ("   ", stdout);

    fputs ("  ", stdout);
    for (; b < len; b++)
      putchar ( (' ' <= data[b] && data[b] <= '~') ? data[b] : '.');
    fputs ("\n", stdout);
  }
}

static gboolean
gst_subtitle_configure_caps (GstPad *pad, LGSubtitle *subt)
{
  GST_DEBUG ("This is gst_subtitle_set_caps\n");

  GstStructure *structure;
  const gchar *name;
  const GValue *codec_data;
  gint width = 0, height = 0;
  GstBuffer *buffer;

  GST_DEBUG ("got caps %" GST_PTR_FORMAT, gst_pad_peer_get_caps(pad));
  //sink = LG_SUBTITLE (basesink);

  structure = gst_caps_get_structure (gst_pad_peer_get_caps (pad), 0);
  name = gst_structure_get_name (structure);

  GST_DEBUG ("caps name %s", name);
  if (!strcmp (name, "application/x-ass")
      || !strcmp (name, "application/x-ssa"))
  {
    subt->type = gst_subtitle_type_ass;

    codec_data = gst_structure_get_value (structure, "codec_data");
    if (codec_data != NULL)
    {
      buffer = gst_value_get_buffer (codec_data);

      if (0)
      {
        gchar buf[GST_BUFFER_SIZE (buffer) + 1];

        dump ("codec_data", GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

        memcpy (buf, GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
        buf[GST_BUFFER_SIZE (buffer)] = 0;

        GST_DEBUG ("codec data:\n%s", buf);
      }
    }
  }
  else if (!strcmp (name, "text/plain"))
    subt->type = gst_subtitle_type_text_plain;
  else if (!strcmp (name, "video/x-avi-unknown") || !strcmp (
      name,
      "text/x-avi-internal"))
  {
    guint32 fourcc;

    GST_DEBUG ("fourcc - %d\n", subt->type);

    if (gst_structure_get_fourcc (structure, "fourcc", &fourcc) == TRUE)
    {
      if (fourcc == GST_MAKE_FOURCC ('D', 'X', 'S', 'A'))
        subt->type = gst_subtitle_type_DXSA;
      if (fourcc == GST_MAKE_FOURCC ('D', 'X', 'S', 'B'))
        subt->type = gst_subtitle_type_DXSB;
    }

    if (gst_structure_get_int (structure, "width", &width))
    {
      subt->wind_width = width;
    }
    else
    {
      subt->wind_width = 0;
    }
    if (gst_structure_get_int (structure, "height", &height))
    {
      subt->wind_height = height;
    }
    else
    {
      subt->wind_height = 0;
    }

    GST_DEBUG ("bitmap info from Demux - width:%d, height:%d\n", width, height);

    if (subt->type == gst_subtitle_type_unknown)
    {
      GST_ERROR ("cannot determin subtitle type with the caps");
      return FALSE;
    }
  }
  else
  {
    GST_ERROR ("unknown subtitle codec. %s", name);
    return FALSE;
  }
  return TRUE;
}

static inline int
read_1digit (const gchar *buf)
{
  if ('0' <= buf[0] && buf[0] <= '9')
    return buf[0] - '0';
  else
    return 0;
}

static int
read_2digit (const gchar *buf)
{
  return read_1digit (buf) * 10 + read_1digit (buf + 1);
}

static int
read_3digit (const gchar *buf)
{
  return read_1digit (buf) * 100 + read_1digit (buf + 1) * 10 + read_1digit (
      buf + 2);
}

static gboolean
read_timestamp (const gchar *buf, int len, GstClockTime *ts, GstClockTime *dur)
{
  GstClockTime timestamp_start, timestamp_end;

  // DXSB format seems to be...
  //
  // [00:00:36.800-00:00:39.067]blablabla...
  //
  // 0123456789012345678901234567890123456789

  if (len < 27)
  {
    GST_WARNING ("too small buffer");
    dump (NULL, (const unsigned char*) buf, len);
  }
  else
  {
    if (buf[0] != '[' || buf[3] != ':' || buf[6] != ':' || buf[9] != '.'
        || buf[13] != '-' || buf[16] != ':' || buf[19] != ':' || buf[22] != '.'
        || buf[26] != ']')
    {
      GST_WARNING ("unknown format");
      dump (NULL, (const unsigned char*) buf, len);

      *ts = *dur = 0;
      return FALSE;
    }
    else
    {
      timestamp_start = 0;
      timestamp_start += read_2digit (buf + 1) * 60 * 60 * 1000000000LL;
      timestamp_start += read_2digit (buf + 4) * 60 * 1000000000LL;
      timestamp_start += read_2digit (buf + 7) * 1000000000LL;
      timestamp_start += read_3digit (buf + 10) * 1000000LL;

      timestamp_end = 0;
      timestamp_end += read_2digit (buf + 14) * 60 * 60 * 1000000000LL;
      timestamp_end += read_2digit (buf + 17) * 60 * 1000000000LL;
      timestamp_end += read_2digit (buf + 20) * 1000000000LL;
      timestamp_end += read_3digit (buf + 23) * 1000000LL;

      *ts = timestamp_start;
      *dur = timestamp_end - timestamp_start;
    }
  }
  return TRUE;
}

static unsigned char
get_nibble (const unsigned char *buf, int cnt)
{
  if (cnt & 0x1)
  {
    return (buf[cnt / 2] & 0x0F);
  }
  else
  {
    return ( (buf[cnt / 2] & 0xF0) >> 4);
  }
}

struct palette
{
  unsigned char a;
  unsigned char r;
  unsigned char g;
  unsigned char b;
};

static void
decode_rle (const unsigned char **_data, int len, unsigned char *bmp,
    int stride, int w, int h, const struct palette *p)
{
  const unsigned char *data = *_data;
  int nr = 0;
  int x, y;

  for (y = 0; y < h && nr / 2 < len; y++)
  {
    int nw;

    nw = 0;
    for (x = 0; x < w && nr / 2 < len;)
    {
      int a;
      int run;
      unsigned int d;

      d = get_nibble (data, nr++);
      if (d < 4)
      {
        d = (d << 4) | get_nibble (data, nr++);
        if (d < 16)
        {
          d = (d << 4) | get_nibble (data, nr++);
          if (d < 64)
          {
            d = (d << 4) | get_nibble (data, nr++);
            if (d < 256)
            {
              d &= 3;
            }
          }
        }
      }

      run = d >> 2;
      if (run == 0 || run > w - x) run = w - x;

      for (a = 0; a < run; a++)
      {
        bmp[nw++] = p[d & 3].a;
        bmp[nw++] = p[d & 3].r;
        bmp[nw++] = p[d & 3].g;
        bmp[nw++] = p[d & 3].b;
      }

      x += run;
    }

    bmp += stride;

    if ( (nr & 1) == 1) nr++;
  }

  *_data = data + nr / 2;
}

static GstFlowReturn
lg_subtitle_text_render (GstPad *pad, GstBuffer *buffer)
{
  ////GstFlowReturn res;
  LGSubtitle *subt = LG_SUBTITLE(GST_PAD_PARENT(pad));

  GST_DEBUG ("This is gst_subtitle_element_chain\n");

  struct timeval startTime1, startTime2, endTime1, endTime2;
  long secElapsed, microsecElapsed;
  gettimeofday (&startTime1, 0);
  //LG_SUBTITLE_LOCK(subt);

  if (!subt->subtitle_on)
  {
    __LMF_INTSUBT_IsExist ((void *) 1);
    subt->subtitle_on = TRUE;
  }

  //FIXME
  if (!subt->need_render)
  {
    return;
  }

  GST_INFO_OBJECT (subt,
      "Pushing buffer with duration %"
      GST_TIME_FORMAT ", offset %" G_GUINT64_FORMAT
      " and size %d over pad %s",
      GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)),
      GST_BUFFER_OFFSET (buffer), GST_BUFFER_SIZE(buffer), GST_PAD_NAME (pad));

  GST_DEBUG ("GST_PAD_NAME : %s\n", GST_PAD_NAME (pad));

  //	structure = gst_caps_get_structure(gst_pad_get_caps(pad), 0);
  //	name = gst_structure_get_name (structure);
  //	GST_DEBUG ("caps name %s", name);

  gst_subtitle_configure_caps (pad, subt);

  GST_DEBUG ("This is gst_subtitle_chain\n");
  //char *err;
  if (!__LMF_ParseInternalSubtitle) __LMF_ParseInternalSubtitle = (void
  (*) (void *)) dlsym (NULL, "LMF_ParseInternalSubtitle");

  if (!__LMF_ParseBitmapSubtitle) __LMF_ParseBitmapSubtitle = (void
  (*) (void *)) dlsym (NULL, "LMF_ParseBitmapSubtitle");

  if (!__LMF_INTSUBT_IsExist) __LMF_INTSUBT_IsExist = (void
  (*) (void *)) dlsym (NULL, "LMF_INTSUBT_IsExist");

  if (!__LMF_Get_subtitleExist) __LMF_Get_subtitleExist = (int
  (*) (void)) dlsym (NULL, "LMF_Get_subtitleExist");

  int isInternal = 0;
  LMF_INTERNAL_SUBTITLE_PARAM_T *pSubtitleParam = NULL;
  LMF_SUBTITLE_BITMAP_PARAM_T *pBitmapParam = NULL;
  pSubtitleParam = g_malloc (sizeof(LMF_INTERNAL_SUBTITLE_PARAM_T));
  pBitmapParam = g_malloc (sizeof(LMF_SUBTITLE_BITMAP_PARAM_T));

  const unsigned char *buf = GST_BUFFER_DATA (buffer);
  int len = GST_BUFFER_SIZE (buf);

  GstClockTime timestamp;
  GstClockTime duration;

  gchar *text = NULL;
  int text_len = 0;
  int bmpsize = 0;

  unsigned char *bitmap = NULL;
  int bitmap_width = 0;
  int bitmap_height = 0;
  int bitmap_left, bitmap_top, bitmap_right, bitmap_bottom;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);

  if (0)
  {
    gchar buf[GST_BUFFER_SIZE (buffer) + 1];

    dump ("subtitle data", GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

    memcpy (buf, GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
    buf[GST_BUFFER_SIZE (buffer)] = 0;

    printf ("%s\n", buf);
  }

  if (subt->type == gst_subtitle_type_ass)
  {
    int a;
    int comma;
    isInternal = 1;
    //if(!__LMF_INTSUBT_IsExist)
    __LMF_INTSUBT_IsExist ((void *) isInternal);

    for (a = 0, comma = 0; a < len && comma < 8; a++)
    {
      if (buf[a] == ',') comma++;
    }

    if (a == len)
    {
      LG_SUBTITLE_UNLOCK(subt);
      return GST_FLOW_OK;
    }

    text = (gchar *) buf + a;
    text_len = len - a - 1;
  }
  else if (subt->type == gst_subtitle_type_text_plain)
  {
    isInternal = 1;
    //if(!__LMF_INTSUBT_IsExist)
    __LMF_INTSUBT_IsExist ((void *) isInternal);
    text = (gchar *) buf;
    text_len = len;
  }
  else if (subt->type == gst_subtitle_type_DXSB || subt->type
      == gst_subtitle_type_DXSA)
  {
    isInternal = 2;
    //if(!__LMF_INTSUBT_IsExist)
    __LMF_INTSUBT_IsExist ((void *) isInternal);
    const unsigned char *tmp = buf + 27;
    int header_size;
    struct palette palette[4];
    int topsize;

    //dump ("DXSB", buf, len);

    if (subt->type == gst_subtitle_type_DXSA)
      header_size = 57;
    else
      header_size = 53;

    /* get the header */
    if (len < header_size)
    {
      GST_WARNING ("too small buffer for DXS[AB] header");
    }
    else
    {
      read_timestamp ((const gchar*) buf, len, &timestamp, &duration);
      if (duration <= 0)
      {
        GST_WARNING ("no duration");
      }
      else
      {
        int a;

        int *datas[] = {
            &bitmap_width,
            &bitmap_height,
            &bitmap_left,
            &bitmap_top,
            &bitmap_right,
            &bitmap_bottom,
            &topsize, };

        /* get deader */
        for (a = 0; a < sizeof (datas) / sizeof (datas[0]); a++)
        {
          *datas[a] = tmp[0] + (tmp[1] << 8);
          tmp += 2;
        }

        for (a = 0; a < 4; a++)
        {
          palette[a].r = *tmp++;
          palette[a].g = *tmp++;
          palette[a].b = *tmp++;

          if (a == 0)
          {
            palette[a].a = 0;
            if (subt->type != gst_subtitle_type_DXSA)
              palette[a].r = palette[a].g = palette[a].b = 0;
          }
          else
            palette[a].a = 0xff;
        }

        if (subt->type == gst_subtitle_type_DXSA) for (a = 0; a < 4; a++)
          palette[a].a = *tmp++;

        GST_DEBUG (
            "width %d, height %d, ltrb %d,%d,%d,%d, topsize %d",
            bitmap_width,
            bitmap_height,
            bitmap_left,
            bitmap_top,
            bitmap_right,
            bitmap_bottom,
            topsize);
#if 0
        for (a=0; a<4; a++)
        {
          GST_DEBUG ("palette %d, %02x%02x%02x%02x", a,
              palette[a].a,
              palette[a].r,
              palette[a].g,
              palette[a].b);
        }
#endif

        if (len < topsize)
          GST_WARNING ("too small data. Oops?");
        else
        {
          /* allocate framebuffer memory */
          if (bitmap_width > 1920 || bitmap_height > 1080)
          {
            GST_WARNING ("too big framebuffer");
          }
          else
            bitmap = g_malloc (bitmap_width * bitmap_height * 4);
        }
      }
    }

    /* decode */
    if (bitmap != NULL)
    {
      decode_rle (
          &tmp,
          topsize,
          bitmap,
          bitmap_width * 4 * 2,
          bitmap_width,
          bitmap_height / 2,
          palette);

      decode_rle (
          &tmp,
          len - (tmp - buf),
          bitmap + bitmap_width * 4,
          bitmap_width * 4 * 2,
          bitmap_width,
          bitmap_height / 2,
          palette);
    }
  }
  else
  {

    GST_ERROR ("unknown subtitle type(%d)", subt->type);
    isInternal = 0;
    __LMF_INTSUBT_IsExist ((void *) isInternal);
  }

  GST_DEBUG ("timestamp %" GST_TIME_FORMAT ", duration %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp),
      GST_TIME_ARGS (duration)
  );

  /* dump the result */
  if (text != NULL)
  {
    gchar tmp[text_len + 1];

    memcpy (tmp, text, text_len);
    tmp[text_len] = 0;

    pSubtitleParam->szBuf = g_malloc (text_len + 1 * sizeof(char));
    memset (pSubtitleParam->szBuf, 0x00, text_len + 1 * sizeof(char));
    memcpy (pSubtitleParam->szBuf, tmp, text_len);
    pSubtitleParam->nLen = text_len;
    pSubtitleParam->startTime = (GstClockTime) (timestamp) / 1000000;
    pSubtitleParam->endTime = (GstClockTime) (timestamp + duration) / 1000000;

    GST_DEBUG ("subtitle : %s", pSubtitleParam->szBuf);
    GST_DEBUG (
        "timestamp : %d, %d",
        pSubtitleParam->startTime,
        pSubtitleParam->endTime);

    if (__LMF_ParseInternalSubtitle == NULL)
    {
      GST_ERROR ("dlsym Error: %s", dlerror ());
    }
    else
    {
      __LMF_ParseInternalSubtitle ((void *) pSubtitleParam);
      GST_DEBUG ("dlsym Success...");
    }

  }

  if (bitmap != NULL)
  {
#if 0

    gchar *name;
    int filenum;
    int out = -1;

    name = gst_element_get_name (GST_ELEMENT (subt));

    for (filenum=0; filenum<1000; filenum++)
    {
      gchar *outname;

      outname = g_strdup_printf ("/tmp/subtitle/subtitle_%s_%03d.ppm", name, filenum);
      if (outname)
      {
        out = open (outname, O_CREAT|O_EXCL|O_WRONLY, 0644);
        if (out >= 0)
        {
          GST_DEBUG ("print rgb   to %s", outname);
          break;
        }

        g_free (outname);
      }
    }

    if (out >= 0)
    {
      gchar *outname;
      FILE *o;
      int a, b;

      o = fdopen (out, "w");
      fprintf (o, "P6\n%d %d\n255\n",
          bitmap_width, bitmap_height);
      fflush (o);

      for (b=0; b<bitmap_height; b++)
      {
        for (a=0; a<bitmap_width; a++)
        {
          unsigned char buf[3];

          buf[0] = bitmap[(b*bitmap_width + a)*4 + 1];
          buf[1] = bitmap[(b*bitmap_width + a)*4 + 2];
          buf[2] = bitmap[(b*bitmap_width + a)*4 + 3];

          write (out, buf, 3);
        }
      }

      close (out);

      outname = g_strdup_printf ("/tmp/subtitle/subtitle_%s_%03d_alpha.pbm", name, filenum);
      out = open (outname, O_CREAT|O_WRONLY, 0644);
      if (out >= 0)
      {
        GST_DEBUG ("print alpha to %s", outname);
        o = fdopen (out, "w");
        fprintf (o, "P5\n%d %d\n255\n",
            bitmap_width, bitmap_height);
        fflush (o);

        for (b=0; b<bitmap_height; b++)
        for (a=0; a<bitmap_width; a++)
        write (out, &bitmap[(b*bitmap_width + a)*4 + 0], 1);

        close (out);
      }
    }
#endif

    bmpsize = sizeof (bitmap);
    //GST_DEBUG ("bmpsize = %d \n", bmpsize);

    pBitmapParam->bmp = g_malloc (bitmap_width * bitmap_height * 4 + 1);
    memset (pBitmapParam->bmp, 0x00, bitmap_width * bitmap_height * 4 + 1);
    memcpy (pBitmapParam->bmp, bitmap, bitmap_width * bitmap_height * 4 + 1);

    pBitmapParam->startTime = (GstClockTime) (timestamp) / 1000000;
    pBitmapParam->endTime = (GstClockTime) (timestamp + duration) / 1000000;
    pBitmapParam->bmp_width = bitmap_width;
    pBitmapParam->bmp_height = bitmap_height;
    pBitmapParam->wind_width = subt->wind_width;
    pBitmapParam->wind_height = subt->wind_height;

    GST_DEBUG (
        "timestamp : %d, %d",
        pBitmapParam->startTime,
        pBitmapParam->endTime);

    if (__LMF_ParseBitmapSubtitle == NULL)
    {
      GST_ERROR ("dlsym Error: %s", dlerror ());
    }
    else
    {
      gettimeofday (&startTime2, 0);
      __LMF_ParseBitmapSubtitle ((void *) pBitmapParam);
      gettimeofday (&endTime2, 0);
      GST_DEBUG ("dlsym Success...");
    }

    //if (name != NULL){
    //	g_free (name);
    //	name = NULL;
    //}


    if (bitmap != NULL)
    {
      g_free (bitmap);
      bitmap = NULL;
    }
  }

  //LG_SUBTITLE_UNLOCK(subt);
  gettimeofday (&endTime1, 0);
  secElapsed = endTime1.tv_sec - startTime1.tv_sec;
  microsecElapsed = abs (endTime1.tv_usec - startTime1.tv_usec);
  GST_ERROR_OBJECT (
      subt,
      "total Lock to unlock sec : %ld, microsec : %ld",
      secElapsed,
      microsecElapsed);
  secElapsed = endTime2.tv_sec - startTime2.tv_sec;
  microsecElapsed = abs (endTime2.tv_usec - startTime2.tv_usec);
  GST_ERROR_OBJECT (
      subt,
      "parseBitmapsubtitle sec : %ld, microsec : %ld",
      secElapsed,
      microsecElapsed);

  //g_object_unref(buffer);

  subt->need_render = FALSE;
  return GST_FLOW_OK;

  //return gst_pad_push(subt->srcpad, buffer);
}

/*
 static GstStateChangeReturn
 lg_subtitle_change_state(GstElement *element, GstStateChange transition)
 {
 LGSubtitle *subt = LG_SUBTITLE(element);
 GstStateChangeReturn result;

 switch (transition) {
 case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
 //	if(!__LMF_ParseInternalSubtitle)
 //		__LMF_ParseInternalSubtitle = (void(*)(void *))dlsym(NULL,"LMF_ParseInternalSubtitle");
 //
 //	if(!__LMF_ParseBitmapSubtitle)
 //		__LMF_ParseBitmapSubtitle = (void(*)(void *))dlsym(NULL,"LMF_ParseBitmapSubtitle");
 //
 if(!__LMF_INTSUBT_IsExist)
 __LMF_INTSUBT_IsExist = (void(*)(void *))dlsym(NULL,"LMF_INTSUBT_IsExist");
 //
 //	if(!__LMF_Get_subtitleExist)
 //  		__LMF_Get_subtitleExist = (int(*)(void))dlsym(NULL,"LMF_Get_subtitleExist");
 __LMF_INTSUBT_IsExist((void *)1);
 break;
 default:
 break;
 }

 return GST_STATE_CHANGE_SUCCESS;
 }*/
