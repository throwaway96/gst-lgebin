#include <gst/gst.h>
#include <lgebin/dishsrcbin.h>

#include <uriparser/Uri.h>

static void
lg_dishsrc_bin_uri_handler_init (gpointer g_iface, gpointer iface_data);

static GstStateChangeReturn
lg_dishsrc_bin_change_state (GstElement *element, GstStateChange transition);

GST_DEBUG_CATEGORY_STATIC(lg_dishsrc_bin_debug);
#define GST_CAT_DEFAULT lg_dishsrc_bin_debug

static void
_do_init (GType type)
{
  GType g_define_type_id = type;

  G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, lg_dishsrc_bin_uri_handler_init);

  GST_DEBUG_CATEGORY_INIT(
      lg_dishsrc_bin_debug,
      "dishsrcbin",
      0,
      "dish source bin");
}

GST_BOILERPLATE_FULL(
    LGDishSrcBin,
    lg_dishsrc_bin,
    GstBin,
    GST_TYPE_BIN,
    _do_init);

enum
{
  PROP_0,
  PROP_URI,
  PROP_DTCPIP,
  PROP_DTCPIP_HOST,
  PROP_DTCPIP_PORT
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE (
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void
lg_dishsrc_bin_finalize (GObject *self)
{
  LGDishSrcBin *bin = LG_DISHSRC_BIN(self);

  /* call our parent method (always do this!) */

  if (bin->uri) g_free (bin->uri);

  G_OBJECT_CLASS (parent_class) ->finalize (self);

//  if (bin->srcpad) g_object_unref (bin->srcpad);
//  if (bin->src_el) g_object_unref (bin->src_el);
//  if (bin->dec_el) g_object_unref (bin->dec_el);
}

static void
lg_dishsrc_bin_set_property (GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
  LGDishSrcBin *bin = LG_DISHSRC_BIN(object);
  GstURIHandler *uri_handler = GST_URI_HANDLER(bin);
  GstURIHandlerInterface *uri_iface = GST_URI_HANDLER_GET_INTERFACE(uri_handler);

  gchar *uri;

  switch (property_id)
  {
  case PROP_URI:
    GST_OBJECT_LOCK(bin);
    uri = g_value_dup_string (value);
    uri_iface->set_uri (uri_handler, uri);
    g_free (uri);

    GST_INFO_OBJECT (bin, "setting up uri property [%s]", bin->uri);
    GST_OBJECT_UNLOCK(bin);

    break;
  default:
    break;
  }
}

static void
lg_dishsrc_bin_get_property (GObject *object, guint property_id, GValue *value,
    GParamSpec *pspec)
{
  LGDishSrcBin *bin = LG_DISHSRC_BIN(object);

  switch (property_id)
  {
  case PROP_URI:
    g_value_set_string (value, bin->uri);
    break;
  case PROP_DTCPIP:
    g_value_set_boolean (value, bin->enabled_dtcpip);
    break;
  case PROP_DTCPIP_HOST:
    g_value_set_string (value, bin->dtcpip_host);
    break;
  case PROP_DTCPIP_PORT:
    g_value_set_uint (value, bin->dtcpip_port);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }

}

static void
lg_dishsrc_bin_class_init (LGDishSrcBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) (klass);
  gstelement_class = (GstElementClass *) (klass);

  gobject_class->set_property = lg_dishsrc_bin_set_property;
  gobject_class->get_property = lg_dishsrc_bin_get_property;
  gobject_class->finalize = lg_dishsrc_bin_finalize;

  g_object_class_install_property (
      gobject_class,
      PROP_URI,
      g_param_spec_string (
          "uri",
          "URI",
          "URI to decode",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (
      gobject_class,
      PROP_DTCPIP,
      g_param_spec_boolean (
          "dtcpip",
          "DTCP/IP",
          "loading status dtcp/ip decryption plugin",
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (
      gobject_class,
      PROP_DTCPIP_HOST,
      g_param_spec_string (
          "dtcpip-host",
          "DTCP/IP Host",
          "DTCP/IP Host information from uri",
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (
      gobject_class,
      PROP_DTCPIP_PORT,
      g_param_spec_uint (
          "dtcpip-port",
          "DTCP/IP port",
          "DTCP/IP port information from uri",
          1,
          65535,
          1,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = lg_dishsrc_bin_change_state;
}
static void
lg_dishsrc_bin_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(g_class);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_details_simple (
      element_class,
      "Dish Source",
      "Source/DTCP-IP",
      "Download stream via http/https",
      "Jeongseok Kim <jeongseok.kim@lge.com>");

}

static void
lg_dishsrc_bin_init (LGDishSrcBin * bin, LGDishSrcBinClass *g_class)
{
  GstPad *srcpad;
  GstPadTemplate *pad_tmpl;

  GST_INFO_OBJECT (bin, "initializing dishsrc bin instance.");

  bin->src_el = gst_element_factory_make ("souphttpsrc", NULL );
  gst_bin_add (GST_BIN_CAST(bin), GST_ELEMENT_CAST(bin->src_el) );

  pad_tmpl = gst_static_pad_template_get (&src_template);
  bin->srcpad = gst_ghost_pad_new_no_target_from_template (
      "src", pad_tmpl);
  gst_object_unref(pad_tmpl);

  gst_pad_set_active (bin->srcpad, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST(bin), bin->srcpad);

  bin->enabled_dtcpip = FALSE;
}

static gchar**
lg_dishsrc_bin_uri_get_protocols (void)
{
  static gchar *protocols[] = { "http", "https", "dtcp", "dtcps", NULL };

  return protocols;
}

static gchar**
lg_dishsrc_bin_uri_get_protocols_full (GType type)
{
  return lg_dishsrc_bin_uri_get_protocols ();
}

static const gchar*
lg_dishsrc_bin_uri_get_uri (GstURIHandler *handler)
{
  LGDishSrcBin *bin = LG_DISHSRC_BIN(handler);
  return bin->uri;
}

static gboolean
lg_dishsrc_bin_uri_set_uri (GstURIHandler *handler, const gchar *uri)
{
  gboolean ret = FALSE;
  gchar *replaced_uri;
  gchar *scheme;

  UriUriA uri_uri, uri_dest;
  UriParserStateA uri_state;
  UriQueryListA *uri_qlist, *uri_walk;

  int uri_qcnt = 0;
  int uri_required_chars = 0;

  GstURIHandlerInterface *iface;
  LGDishSrcBin *bin = LG_DISHSRC_BIN(handler);

  GstURIHandler *internal_handler = GST_URI_HANDLER(bin->src_el);

  if (!internal_handler)
  {
    GST_ERROR_OBJECT (bin, "cannot find internal uri handler");
    return FALSE;
  }

  iface = GST_URI_HANDLER_GET_INTERFACE(internal_handler);
  if (!iface)
  {
    GST_ERROR_OBJECT (bin, "cannot find internal uri interface");
    return FALSE;
  }

  uri_state.uri = &uri_uri;
  if (uriParseUriA (&uri_state, uri) != URI_SUCCESS)
  {
    GST_ERROR_OBJECT (bin, "invalid uri type[%s]", uri);
    uriFreeUriMembersA (&uri_uri);

    return FALSE;
  }

  if (uriDissectQueryMallocA (
      &uri_qlist,
      &uri_qcnt,
      uri_uri.query.first,
      uri_uri.query.afterLast) != URI_SUCCESS)
  {
    GST_ERROR_OBJECT (bin, "failed to parse uri query");
    return FALSE;
  }

  for (uri_walk = uri_qlist; uri_walk != NULL ; uri_walk = uri_walk->next)
  {
    if (!g_ascii_strcasecmp ("DTCP1HOST", uri_walk->key))
    {
      if (bin->dtcpip_host) g_free (bin->dtcpip_host);
      bin->dtcpip_host = g_strdup (uri_walk->value);
    }

    if (!g_ascii_strcasecmp ("DTCP1PORT", uri_walk->key))
    {
      bin->dtcpip_port = g_ascii_strtoll (uri_walk->value, 0, 10);
    }
  }

  uriFreeQueryListA (uri_qlist);

  scheme = g_uri_parse_scheme (uri);

  g_free (bin->uri);
  bin->uri = g_strdup (uri);

  memcpy(&uri_dest, &uri_uri, sizeof(UriUriA));
  uri_dest.query.first = 0;
  uri_dest.query.afterLast = 0;

  uriToStringCharsRequiredA(&uri_dest, &uri_required_chars);
  replaced_uri = g_malloc(sizeof(char) * (uri_required_chars + 1));
  uriToStringA(replaced_uri, &uri_dest, uri_required_chars + 1, NULL);

  uriFreeUriMembersA (&uri_uri);

  if (!g_ascii_strncasecmp ("dtcp", scheme, 4))
  {
    memcpy (replaced_uri, "http", 4);

    bin->enabled_dtcpip = TRUE;

    if (!bin->dec_el) 
    {
      bin->dec_el = gst_element_factory_make ("dtcpip", NULL );
      gst_bin_add (GST_BIN_CAST(bin), GST_ELEMENT_CAST(bin->dec_el) );
    }

    if (bin->dec_el)
    {
      GST_DEBUG_OBJECT (
          bin,
          "set to dtcp host as '%s:%d'",
          bin->dtcpip_host,
          bin->dtcpip_port);

      g_object_set (bin->dec_el, "dtcp1host", bin->dtcpip_host, NULL );
      g_object_set (bin->dec_el, "dtcp1port", bin->dtcpip_port, NULL );
    }
    else
    {
      GST_INFO_OBJECT (bin, "cannot create dtcpip element");
    }

    GST_DEBUG_OBJECT (
        bin,
        "attempting to set replaced-uri[%s] for internal source element",
        replaced_uri);
  }
  else
  {
    bin->enabled_dtcpip = FALSE;
  }

  g_free (scheme);
  ret = iface->set_uri (internal_handler, replaced_uri);
  g_free (replaced_uri);

  return ret;
}

static GstURIType
lg_dishsrc_bin_uri_get_type (void)
{
  return GST_URI_SRC;
}

static GstURIType
lg_dishsrc_bin_uri_get_type_full (GType type)
{
  return lg_dishsrc_bin_uri_get_type ();
}

static void
lg_dishsrc_bin_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  GST_DEBUG_OBJECT (iface, "initializing uri handler");
  iface->get_type = lg_dishsrc_bin_uri_get_type;
  iface->get_type_full = lg_dishsrc_bin_uri_get_type_full;

  iface->get_protocols = lg_dishsrc_bin_uri_get_protocols;
  iface->get_protocols_full = lg_dishsrc_bin_uri_get_protocols_full;
  iface->get_uri = lg_dishsrc_bin_uri_get_uri;
  iface->set_uri = lg_dishsrc_bin_uri_set_uri;

}

static GstStateChangeReturn
lg_dishsrc_bin_change_state (GstElement *element, GstStateChange transition)
{
  GstPad *srcpad, *dec_sinkpad, *dec_srcpad;
  LGDishSrcBin *bin = LG_DISHSRC_BIN(element);

  GstStateChangeReturn result;

  GST_DEBUG_OBJECT (bin, "state change (%d)", transition);
  switch (transition)
  {
  case GST_STATE_CHANGE_NULL_TO_READY:
    GST_DEBUG_OBJECT (bin, "state change: NULL to READY");

    srcpad = gst_element_get_static_pad (bin->src_el, "src");

    if (bin->dec_el && bin->enabled_dtcpip)
    {
      GST_INFO_OBJECT (bin, "linking http source element to dtcpip element");

      dec_sinkpad = gst_element_get_static_pad (bin->dec_el, "sink");
      gst_pad_link (srcpad, dec_sinkpad);
      g_object_unref (dec_sinkpad);

      g_object_unref (srcpad);

      srcpad = gst_element_get_static_pad (bin->dec_el, "src");
    }

    gst_ghost_pad_set_target (GST_GHOST_PAD(bin->srcpad), srcpad);

    g_object_unref (srcpad);

    break;

  case GST_STATE_CHANGE_READY_TO_PAUSED:
    GST_DEBUG_OBJECT (bin, "state change: READY to PAUSED");
    break;

  case GST_STATE_CHANGE_READY_TO_NULL:

    break;
  }

  result = GST_ELEMENT_CLASS(parent_class) ->change_state (element, transition);

  return result;
}
