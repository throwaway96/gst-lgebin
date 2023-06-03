/*
 * lgebin.c
 *
 *  Created on: Oct 8, 2012
 *      Author: js
 */

#include <gst/gst.h>
#include <lgebin/lgselectorbin.h>
//#include <lgebin/dishsrc.h>
#include <lgebin/dishsrcbin.h>

struct _elements_entry
{
  const gchar *name;
  guint rank;
  GType
  (*type) (void);
};

static struct _elements_entry _elements[] = {
    { "lgselectorbin", GST_RANK_NONE, lg_selector_bin_get_type },
    { "dishsrcbin", GST_RANK_NONE, lg_dishsrc_bin_get_type },
    //{ "dishsrc", GST_RANK_PRIMARY, lg_dishsrc_get_type },
    { NULL, 0 }
};

static gboolean
plugin_init (GstPlugin * plugin)
{
  struct _elements_entry *lg_elements = _elements;

  while ( (*lg_elements).name)
  {
    GST_INFO_OBJECT(plugin, "initializing %s plug-in.", (*lg_elements).name);
    if (!gst_element_register (
        plugin,
        (*lg_elements).name,
        (*lg_elements).rank,
        (*lg_elements).type ())) return FALSE;

    lg_elements++;
  }

  return TRUE;
}

#ifndef PACKAGE
#define PACKAGE "lgebin"
#endif
GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "lgebin",
    "LG Elements plugin",
    plugin_init,
    "0.0.1",
    "LGPL",
    PACKAGE,
    "http://lge.com/")
