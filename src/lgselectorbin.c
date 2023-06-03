#include <gst/gst.h>
#include <lgebin/lgselectorbin.h>
GST_DEBUG_CATEGORY_STATIC(lg_selector_bin_debug);
#define GST_CAT_DEFAULT lg_selector_bin_debug

static void
_do_init (GType type)
{
  GType g_define_type_id = type;

  //  G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, lg_dishsrc_bin_uri_handler_init);
  GST_DEBUG_CATEGORY_INIT(
      lg_selector_bin_debug,
      "lgselectorbin",
      0,
      "lg selector bin");
}

GST_BOILERPLATE_FULL(
    LGSelectorBin,
    lg_selector_bin,
    GstBin,
    GST_TYPE_BIN,
    _do_init);

static void
lg_selector_bin_finalize (GObject *self);
static void
lg_selector_bin_get_property (GObject *object, guint property_id, GValue *value,
    GParamSpec *pspec);
static void
lg_selector_bin_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec);
static GstStateChangeReturn
lg_selector_bin_change_state (GstElement *element, GstStateChange transition);
static void
property_notified (GObject *object, GParamSpec *pspec, gpointer data);
static GstIterator *
lg_selector_bin_iterate_internal_links (GstPad * pad);
static GstFlowReturn
lg_selector_bin_pad_chain_bypass (GstPad * pad, GstBuffer * buf);
static GstPad *
lg_selector_bin_request_new_pad (GstElement *element, GstPadTemplate *temp,
    const gchar *name);
static void
lg_selector_bin_release_pad (GstElement * element, GstPad *pad);

static gboolean
lg_selector_bin_event (GstPad *pad, GstEvent *event);
static gboolean
lg_selector_bin_sink_event (GstPad *pad, GstEvent *event);

#define DEFAULT_SYNC_STREAMS FALSE
/* properties */
enum
{
  PROP_0,
  PROP_BYPASS,
  PROP_NUMBER_OF_SRCPAD,
  PROP_SYNC_STREAMS,
  PROP_ACTIVE_PAD,
  PROP_IS_STREAM,
  PROP_LAST
};

/* signals */
enum
{
  SIGNAL_LAST
};

static GParamSpec *props[PROP_LAST];
static guint signals[SIGNAL_LAST];

static void
lg_selector_bin_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(g_class);

  gst_element_class_add_static_pad_template (
      element_class,
      &lg_selector_bin_sink_factory);
  gst_element_class_add_static_pad_template (
      element_class,
      &lg_selector_bin_src_factory);

  gst_element_class_set_details_simple (
      element_class,
      "LG Selector Bin",
      "Auxiliary/Inputselector",
      "Input Selector Helper",
      "Wonchul Lee <wonchul86.lee@lge.com>, Jeongseok Kim <jeongseok.kim@lge.com>");

}
static void
lg_selector_bin_class_init (LGSelectorBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) (klass);
  gstelement_class = (GstElementClass *) (klass);

  gobject_class->finalize = lg_selector_bin_finalize;
  gobject_class->set_property = lg_selector_bin_set_property;
  gobject_class->get_property = lg_selector_bin_get_property;

  gstelement_class->request_new_pad = lg_selector_bin_request_new_pad;
  gstelement_class->release_pad = lg_selector_bin_release_pad;

  props[PROP_BYPASS] = g_param_spec_boolean (
      "bypass",
      "Bypass",
      "determine whether sbin construct bypass or not",
      FALSE,
      G_PARAM_READWRITE);

  props[PROP_NUMBER_OF_SRCPAD] = g_param_spec_uint (
      "numsrcpad",
      "NumberOfSrcPads",
      "numberofsrcpads",
      0,
      16,
      0,
      G_PARAM_READWRITE);

  props[PROP_SYNC_STREAMS] = g_param_spec_boolean (
      "sync-streams",
      "Sync Streams",
      "Synchronize inactive streams to the running time of the active stream",
      DEFAULT_SYNC_STREAMS,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_ACTIVE_PAD] = g_param_spec_object (
      "active-pad",
      "Active pad",
      "delivery active pad property information to input-selector",
      GST_TYPE_PAD,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  
  props[PROP_IS_STREAM] = g_param_spec_boolean (
      "is-stream",
      "Is stream source",
      "distinguish file or http source stream",
      TRUE,
      G_PARAM_READWRITE);

  g_object_class_install_property (
      gobject_class,
      PROP_BYPASS,
      props[PROP_BYPASS]);

  g_object_class_install_property (
      gobject_class,
      PROP_NUMBER_OF_SRCPAD,
      props[PROP_NUMBER_OF_SRCPAD]);

  g_object_class_install_property (
      gobject_class,
      PROP_SYNC_STREAMS,
      props[PROP_SYNC_STREAMS]);

  g_object_class_install_property (
      gobject_class,
      PROP_ACTIVE_PAD,
      props[PROP_ACTIVE_PAD]);

  g_object_class_install_property (
      gobject_class,
      PROP_IS_STREAM,
      props[PROP_IS_STREAM]);

  gstelement_class->change_state = lg_selector_bin_change_state;
}

static void
lg_selector_bin_init (LGSelectorBin * sbin, LGSelectorBinClass *klass)
{
  GstPadTemplate *templ;

  sbin->bypass = FALSE;
  sbin->pending_sync_stream = FALSE;
  sbin->padpairs = g_hash_table_new_full (
      g_direct_hash,
      g_direct_equal,
      NULL,
      NULL );
  sbin->srcpad = NULL;
  sbin->selector = NULL;
  sbin->activepad = NULL;
  sbin->is_stream = FALSE;

  sbin->played = FALSE;

  sbin->lock = g_mutex_new ();

  //add property notify call back.
  g_signal_connect(
      sbin,
      "notify::bypass",
      G_CALLBACK (property_notified),
      NULL);
}

static void
lg_selector_bin_finalize (GObject *self)
{
  LGSelectorBin *sbin = LG_SELECTOR_BIN(self);
  GList *keys = NULL;

  if (sbin->padpairs)
  {
    //    g_hash_table_foreach(sbin->padpairs, gst_proxy_pad_unlink_default, NULL);
    g_hash_table_remove_all (sbin->padpairs);
    g_hash_table_destroy (sbin->padpairs);
  }

  //  if (sbin->selector) g_object_unref (sbin->selector);
  //  if (sbin->activepad) g_object_unref (sbin->activepad);
  g_mutex_free (sbin->lock);

  /* call our parent method (always do this!) */

  G_OBJECT_CLASS (parent_class) ->finalize (self);
}

static void
lg_selector_bin_get_property (GObject *object, guint property_id, GValue *value,
    GParamSpec *pspec)
{
  LGSelectorBin *sbin = LG_SELECTOR_BIN(object);

  LG_SELECTOR_BIN_LOCK(sbin);
  switch (property_id)
  {
  case PROP_BYPASS:
    g_value_set_boolean (value, sbin->bypass);
    break;
  case PROP_NUMBER_OF_SRCPAD:
    g_value_set_uint (value, sbin->numsrcpad);
    break;
  case PROP_ACTIVE_PAD:
    g_value_set_object (value, sbin->activepad);
    break;
  case PROP_SYNC_STREAMS:
  {
    gboolean sync;
    if (sbin->selector)
      g_object_get (sbin->selector, "sync-streams", &sync, NULL );
    else
      sync = TRUE;
    g_value_set_boolean (value, sync);
    break;
  }
  case PROP_IS_STREAM:
    g_value_set_boolean (value, sbin->is_stream);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
  LG_SELECTOR_BIN_UNLOCK(sbin);
}

static void
lg_selector_bin_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
  LGSelectorBin *sbin = LG_SELECTOR_BIN(object);

  LG_SELECTOR_BIN_LOCK(sbin);
  switch (property_id)
  {
  case PROP_BYPASS:
    sbin->bypass = g_value_get_boolean (value);
    break;
  case PROP_NUMBER_OF_SRCPAD:
    sbin->numsrcpad = g_value_get_uint (value);
    break;
  case PROP_ACTIVE_PAD:
  {
    GstPad * inputsel_active_sinkpad;
    sbin->activepad = g_value_get_object (value);
    inputsel_active_sinkpad = gst_ghost_pad_get_target (
        GST_GHOST_PAD_CAST(sbin->activepad) );

    g_object_set (sbin->selector, "active-pad", inputsel_active_sinkpad, NULL );

    g_object_unref (inputsel_active_sinkpad);
	break;
  }
  case PROP_SYNC_STREAMS:
    if (sbin->selector)
    {
      g_object_set (
          sbin->selector,
          "sync-streams",
          g_value_get_boolean (value),
          NULL );
    }
    else
    {
      sbin->pending_sync_stream = TRUE;
    }
    break;
  case PROP_IS_STREAM:
    sbin->is_stream = g_value_get_boolean (value);
	break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
  LG_SELECTOR_BIN_UNLOCK(sbin);
}

static void
property_notified (GObject *object, GParamSpec *pspec, gpointer data)
{
  GValue value = { 0 };
  gboolean bl;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (object, pspec->name, &value);
  bl = g_value_get_boolean (&value);

  g_print (
      "property '%s' is set to '%s'\n",
      pspec->name,
      (bl ? "TRUE" : "FALSE"));

  g_value_unset (&value);
}

static GstIterator *
lg_selector_bin_iterate_internal_links (GstPad * pad)
{
  GstIterator *it = NULL;
  LGSelectorBin *sbin = LG_SELECTOR_BIN(gst_pad_get_parent(pad));
  GstPad * opad;

  LG_SELECTOR_BIN_LOCK(sbin);

  opad = g_hash_table_lookup (sbin->padpairs, pad);

  if (opad != NULL )
  {
    gst_object_ref (opad);

    it = gst_iterator_new_single (
        GST_TYPE_PAD,
        opad,
        (GstCopyFunction) gst_object_ref,
        (GFreeFunc) gst_object_unref);

    gst_object_unref (opad);
  }

  LG_SELECTOR_BIN_UNLOCK(sbin);

  gst_object_unref (sbin);

  return it;
}

static GstFlowReturn
lg_selector_bin_pad_chain_bypass (GstPad * pad, GstBuffer * buf)
{
  GstFlowReturn ret;
  LGSelectorBin *sbin = LG_SELECTOR_BIN(gst_pad_get_parent(pad));
  GstPad * matched_srcpad = NULL;

  LG_SELECTOR_BIN_LOCK(sbin);

  matched_srcpad = g_hash_table_lookup (sbin->padpairs, pad);
  ret = gst_pad_push (matched_srcpad, buf);

  LG_SELECTOR_BIN_UNLOCK(sbin);

  gst_object_unref (sbin);
  return ret;
}

static void
pad_removed_cb (GstElement *element, GstPad *pad, LGSelectorBin *sbin)
{
  GstPad *ghost;

  if (!GST_PAD_IS_SRC (pad)) return;

  if (! (ghost = g_object_get_data (G_OBJECT (pad), "lgselectorbin.ghostpad")))
    return;

  if (!GST_IS_GHOST_PAD(ghost)) return;

  gst_ghost_pad_set_target (GST_GHOST_PAD_CAST(ghost), NULL );

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT_CAST(sbin), ghost);

  return;
}

static GstPad *
lg_selector_bin_request_new_pad (GstElement *element, GstPadTemplate *temp,
    const gchar *name)
{
  LGSelectorBin *sbin;
  GstPad * selector_sinkpad = NULL;
  GstPad * selector_srcpad = NULL;
  GstPad * sinkpad = NULL;
  GstPad * srcpad = NULL;
  gchar * pad_name = NULL;
  GstPadTemplate *pad_tmpl;
  GstPadLinkReturn res;

  sbin = LG_SELECTOR_BIN(element);

  LG_SELECTOR_BIN_LOCK(sbin);

  // 1. bypass : N to N
  if (sbin->bypass)
  {
    GST_INFO_OBJECT (element, "selectorbin built bypass type");

    // 1-1. make src/sink pad
    pad_name = g_strdup_printf ("src%d", g_hash_table_size (sbin->padpairs));
    pad_tmpl = gst_static_pad_template_get (&lg_selector_bin_src_factory);
    srcpad = gst_pad_new_from_template (pad_tmpl, pad_name);
    gst_object_unref (pad_tmpl);
    g_free (pad_name);

    pad_name = g_strdup_printf ("sink%d", g_hash_table_size (sbin->padpairs));
    pad_tmpl = gst_static_pad_template_get (&lg_selector_bin_sink_factory);
    sinkpad = gst_pad_new_from_template (pad_tmpl, pad_name);
    gst_object_unref (pad_tmpl);
    g_free (pad_name);

    //add sinkpad func
    gst_pad_set_iterate_internal_links_function (
        sinkpad,
        GST_DEBUG_FUNCPTR(lg_selector_bin_iterate_internal_links));

    gst_pad_set_chain_function (
        sinkpad,
        GST_DEBUG_FUNCPTR (lg_selector_bin_pad_chain_bypass));

    // 1-2. add pad to bin
    gst_pad_set_active (srcpad, TRUE);
    gst_pad_set_active (sinkpad, TRUE);
    gst_element_add_pad (GST_ELEMENT_CAST(sbin), srcpad);
    gst_element_add_pad (GST_ELEMENT_CAST(sbin), sinkpad);

    // 1-3. link them.
    gst_pad_link (srcpad, sinkpad);

    g_hash_table_insert (
        sbin->padpairs,
        g_object_ref (sinkpad),
        g_object_ref (srcpad));

    if (!sbin->activepad) sbin->activepad = sinkpad;
    goto done;
  }

  // 2. input-selector : N to 1
  // 2-1. make input-selector and add src pad.
  if (sbin->selector == NULL )
  {

    sbin->selector = gst_element_factory_make ("input-selector", NULL );

    if (sbin->pending_sync_stream)
    {
      g_object_set (sbin->selector, "sync-streams", TRUE, NULL );
    }

    gst_bin_add (GST_BIN_CAST(sbin), GST_ELEMENT_CAST(sbin->selector));
    gst_element_set_state (sbin->selector, GST_STATE_PAUSED);

    selector_srcpad = gst_element_get_static_pad (sbin->selector, "src");

    pad_tmpl = gst_static_pad_template_get (&lg_selector_bin_src_factory);
    sbin->srcpad = gst_ghost_pad_new_from_template (
        "src0",
        selector_srcpad,
        pad_tmpl);
    /*
     g_signal_connect(
     sbin->selector,
     "pad-removed",
     G_CALLBACK (pad_removed_cb),
     sbin);
     */
    g_object_set_data (
        G_OBJECT(selector_srcpad),
        "lgselectorbin.ghostpad",
        sbin->srcpad);
    g_object_unref (selector_srcpad);
    g_object_unref (pad_tmpl);

    gst_pad_set_event_function (
      sbin->srcpad,
      GST_DEBUG_FUNCPTR(lg_selector_bin_event));  

    gst_pad_set_active (sbin->srcpad, TRUE);
    gst_element_add_pad (GST_ELEMENT_CAST(sbin), sbin->srcpad);
  }

  //2-2. add sink pad.
  selector_sinkpad = gst_element_get_request_pad (sbin->selector, "sink%d");

  pad_name = g_strdup_printf ("sink%d", g_hash_table_size (sbin->padpairs));
  pad_tmpl = gst_static_pad_template_get (&lg_selector_bin_sink_factory);
  sinkpad = gst_ghost_pad_new_from_template (
      pad_name,
      selector_sinkpad,
      pad_tmpl);
  g_free (pad_name);
  g_object_set_data (
      G_OBJECT(selector_sinkpad),
      "lgselectorbin.ghostpad",
      sinkpad);

  g_object_unref (selector_sinkpad);
  g_object_unref (pad_tmpl);

  gst_pad_set_iterate_internal_links_function (
      sinkpad,
      GST_DEBUG_FUNCPTR(lg_selector_bin_iterate_internal_links));

  gst_pad_set_event_function (
      sinkpad,
      GST_DEBUG_FUNCPTR(lg_selector_bin_sink_event));

  gst_pad_set_active (sinkpad, TRUE);
  gst_element_add_pad (element, sinkpad);

  g_hash_table_insert (sbin->padpairs, sinkpad, sbin->srcpad);
  //      g_object_ref (sinkpad),
  //      g_object_ref (sbin->srcpad));


  //FIXME set the first requested pad to active pad
  if (!sbin->activepad)
  {
    GstPad *inputsel_active_sinkpad;
    sbin->activepad = sinkpad;
    inputsel_active_sinkpad = gst_ghost_pad_get_target (
        GST_GHOST_PAD_CAST(sbin->activepad) );

    g_object_set (sbin->selector, "active-pad", inputsel_active_sinkpad, NULL );
    g_object_unref (inputsel_active_sinkpad);
  }

  done: LG_SELECTOR_BIN_UNLOCK(sbin);
  return sinkpad;
}

static void
lg_selector_bin_release_pad (GstElement * element, GstPad *pad)
{
  LGSelectorBin * sbin;
  GstPad * srcpad;

  sbin = LG_SELECTOR_BIN(element);
//  LG_SELECTOR_BIN_LOCK(sbin);

  // case1. bypass
  if (sbin->bypass)
  {
    //remove srcpad of selectorbin
    srcpad = g_hash_table_lookup (sbin->padpairs, pad);
    gst_pad_set_active (srcpad, FALSE);
    gst_element_remove_pad (GST_ELEMENT(sbin), srcpad);
  }
  else
  {
    // case2. input-selector
    GstPad * inputsel_active_sinkpad; // sinkpad of input-selector

    if (!GST_IS_GHOST_PAD(pad)) return; //goto done;

    // remove sinkpad of input-selector
    inputsel_active_sinkpad = gst_ghost_pad_get_target (
        GST_GHOST_PAD_CAST(pad) );
    gst_pad_set_active (inputsel_active_sinkpad, FALSE);

    gst_element_release_request_pad (sbin->selector, inputsel_active_sinkpad);

    // if there is any more pads, remove input-selector
    if (g_hash_table_size (sbin->padpairs) == 1)
    {
      srcpad = g_hash_table_lookup (sbin->padpairs, pad);
      gst_element_remove_pad (GST_ELEMENT(sbin), srcpad);

      gst_element_set_state (sbin->selector, GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST(sbin), sbin->selector);
      sbin->selector = NULL;
    }

    g_object_unref (inputsel_active_sinkpad);
  }

  g_hash_table_remove (sbin->padpairs, pad);
  //remove sinkpad of selectorbin
  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT(sbin), pad);

//done:
//  LG_SELECTOR_BIN_UNLOCK(sbin);
}

static gboolean
lg_selector_bin_event (GstPad *pad, GstEvent *event)
{
  gboolean res = FALSE;
  LGSelectorBin * sbin;
  GstPad * opad;

  sbin = LG_SELECTOR_BIN(gst_pad_get_parent(pad));

  LG_SELECTOR_BIN_LOCK(sbin);

  opad = g_hash_table_lookup (sbin->padpairs, pad);

  GST_DEBUG_OBJECT (pad, "received event %x", GST_EVENT_TYPE(event) );
  if (!opad) opad = sbin->activepad;

  if (opad)
  {
    res = gst_pad_push_event (opad, event);
  }
  else
  {
    gst_event_unref (event);
  }
  LG_SELECTOR_BIN_UNLOCK(sbin);
  gst_object_unref (sbin);
  return res;
}

static gboolean
lg_selector_bin_sink_event (GstPad *pad, GstEvent *event)
{
  gboolean res = FALSE;
  LGSelectorBin * sbin;

  sbin = LG_SELECTOR_BIN (gst_pad_get_parent(pad));


#ifndef MTK5398 
  if (!sbin->is_stream && !sbin->played && GST_EVENT_TYPE(event) == GST_EVENT_EOS)
  {
    GError *err = g_error_new_literal (GST_STREAM_ERROR,
	  GST_STREAM_ERROR_FAILED, "eos never handle before playing states, thus selectorbin post error msg");
    GstMessage *errMsg = gst_message_new_error (GST_OBJECT_CAST(sbin), err, "never handled eos, post error msg");
	gst_element_post_message (GST_ELEMENT_CAST(sbin), errMsg);

	g_error_free (err);
  }
#endif
  GstPad *target_sinkpad = gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(pad));
  res = gst_pad_send_event(target_sinkpad, event);
  if(target_sinkpad)
    g_object_unref (target_sinkpad);

  gst_object_unref (sbin);
  return res;
}

static GstStateChangeReturn
lg_selector_bin_change_state (GstElement *element, GstStateChange transition)
{
  LGSelectorBin *sbin = LG_SELECTOR_BIN(element);

  GstStateChangeReturn result;
  switch (transition){
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (!sbin->played)
	    sbin->played = TRUE;
	  break;
    default:
      break;
  }
  result = GST_ELEMENT_CLASS(parent_class) ->change_state (element, transition);

  return result;
}
