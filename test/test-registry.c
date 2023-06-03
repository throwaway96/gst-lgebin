/*
 * test-instance.c
 *
 *  Created on: Sep 6, 2012
 *      Author: js
 */

#include <gst/gst.h>
#include <gst/gstelement.h>
#include <gst/gsturi.h>

#include <lgebin/dishsrcbin.h>

int
main (int argc, char** argv)
{

  gboolean bypass;
  gboolean async_handling;

  GstRegistry *registry;

  GstPluginFeature *feature;
  GstElementFactory *factory;

  GstElement *element;

  GList *flist;

  int i = 0;

//  g_type_init ();
  gst_init (&argc, &argv);

  registry = gst_registry_get_default ();

  element = gst_element_make_from_uri (
      GST_URI_SRC,
      "dtcps://localhost",
      "source");


  if (element)
  {
    printf ("found it!\n");
  }
  else
  {
    printf ("no source\n");
  }

  element = gst_element_factory_make ("dishsrcbin", NULL );

  printf ("is-a uri handler: %d\n", g_type_is_a(LG_TYPE_DISHSRC_BIN, GST_TYPE_URI_HANDLER));
  printf ("is-a element: %d\n", g_type_is_a(LG_TYPE_DISHSRC_BIN, GST_TYPE_ELEMENT));


  return 0;
}
