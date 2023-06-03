/*
 * dishsrcbin.h
 *
 *  Created on: Oct 19, 2012
 *      Author: js
 */

#ifndef DISHSRC_BIN_H_
#define DISHSRC_BIN_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define LG_TYPE_DISHSRC_BIN \
  (lg_dishsrc_bin_get_type())
#define LG_DISHSRC_BIN(obj) \
 (G_TYPE_CHECK_INSTANCE_CAST ((obj), LG_TYPE_DISHSRC_BIN, LGDishSrcBin))
#define LG_DISHSRC_BIN_CLASS(obj) \
 (G_TYPE_CHECK_CLASS_CAST ((obj), LG_TYPE_DISHSRC_BIN, LGDishSrcBinClass))
#define LG_IS_DISHSRC_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LG_TYPE_DISHSRC_BIN))
#define LG_IS_DISHSRC_BIN_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((obj), LG_TYPE_DISHSRC_BIN))
#define LG_GET_DISHSRC_BIN_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), LG_TYPE_DISHSRC_BIN, LGDishSrcBinClass))

#define LG_DISHSRC_BIN_GET_LOCK(lgbin) (((LGDishSrcBin*)(lgbin))->lock)
#define LG_DISHSRC_BIN_LOCK(lgbin) (g_mutex_lock (LG_DISHSRC_BIN_GET_LOCK(lgbin)))
#define LG_DISHSRC_BIN_UNLOCK(lgbin) (g_mutex_unlock (LG_DISHSRC_BIN_GET_LOCK(lgbin)))

typedef struct _LGDishSrcBinClass LGDishSrcBinClass;
typedef struct _LGDishSrcBin LGDishSrcBin;

struct _LGDishSrcBin
{
  GstBin parent;

  GstPad* srcpad;

  GstElement* src_el;
  GstElement* dec_el;

  gboolean enabled_dtcpip;
  gchar* dtcpip_host;
  guint dtcpip_port;

  gchar* uri;
};

struct _LGDishSrcBinClass
{
  GstBinClass parent_class;
};

GType
lg_dishsrc_bin_get_type (void);

G_END_DECLS

#endif /* DISHSRC_BIN_H_ */
