/*
 * lgselectorbin.h
 *
 *  Created on: Sep 4, 2012
 *      Author: js
 */

//#ifdef HAVE_CONFIG_H
//#include <config.h>
//#endif

#include <gst/gst.h>

G_BEGIN_DECLS

#define LG_TYPE_SELECTOR_BIN \
  (lg_selector_bin_get_type())
#define LG_SELECTOR_BIN(obj) \
 (G_TYPE_CHECK_INSTANCE_CAST ((obj), LG_TYPE_SELECTOR_BIN, LGSelectorBin))
#define LG_SELECTOR_BIN_CLASS(obj) \
 (G_TYPE_CHECK_CLASS_CAST ((obj), LG_TYPE_SELECTOR_BIN, LGSelectorBinClass))
#define LG_IS_SELECTOR_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LG_TYPE_SELECTOR_BIN))
#define LG_IS_SELECTOR_BIN_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((obj), LG_TYPE_SELECTOR_BIN))
#define LG_GET_SELECTOR_BIN_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), LG_TYPE_SELECTOR_BIN, LGSelectorBinClass))

#define LG_SELECTOR_BIN_GET_LOCK(sbin) (((LGSelectorBin*)(sbin))->lock)
#define LG_SELECTOR_BIN_LOCK(sbin) (g_mutex_lock (LG_SELECTOR_BIN_GET_LOCK(sbin)))
#define LG_SELECTOR_BIN_UNLOCK(sbin) (g_mutex_unlock (LG_SELECTOR_BIN_GET_LOCK(sbin)))

static GstStaticPadTemplate lg_selector_bin_sink_factory =
    GST_STATIC_PAD_TEMPLATE (
        "sink%d",
        GST_PAD_SINK,
        GST_PAD_REQUEST,
        GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate lg_selector_bin_src_factory =
    GST_STATIC_PAD_TEMPLATE (
        "src%d",
        GST_PAD_SRC,
        GST_PAD_SOMETIMES,
        GST_STATIC_CAPS_ANY);

typedef struct _LGSelectorBinClass LGSelectorBinClass;
typedef struct _LGSelectorBin LGSelectorBin;

struct _LGSelectorBin
{
  GstBin parent;

  gboolean bypass;
  guint numsrcpad;

  GHashTable * padpairs;

  GstElement * selector;
  GstPad * srcpad;
  GstPad * activepad;

  gboolean pending_sync_stream;

  gboolean played; // for block eos before going to playing state on MKV

  GMutex * lock;
};
struct _LGSelectorBinClass
{
  GstBinClass parent_class;
};

GType
lg_selector_bin_get_type (void);
LGSelectorBin *
lg_selector_bin_new (void);

G_END_DECLS
