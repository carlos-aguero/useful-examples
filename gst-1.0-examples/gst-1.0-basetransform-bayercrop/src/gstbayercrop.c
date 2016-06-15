/*
 * GStreamer
 * Copyright (C) 2016 Carlos Aguero carlos.aguero@ridgerun.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-plugin
 *
 * FIXME:Describe plugin here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! bayercrop ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>

#include "gstbayercrop.h"

#define OUT_WIDTH 600
#define OUT_HEIGHT 400
#define OUT_SIZE 240000 //640*480=307200 320*240=76800

GST_DEBUG_CATEGORY_STATIC (gst_bayer_crop_debug);
#define GST_CAT_DEFAULT gst_bayer_crop_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

#define gst_bayer_crop_parent_class parent_class
G_DEFINE_TYPE (GstBayerCrop, gst_bayer_crop, GST_TYPE_BASE_TRANSFORM);

static void gst_bayer_crop_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_bayer_crop_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_bayer_crop_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

static GstFlowReturn gst_bayer_crop_transform (GstBaseTransform *trans, GstBuffer *inbuf,
                                 GstBuffer *outbuf);

static GstCaps* gst_bayer_crop_transform_caps (GstBaseTransform *trans,
                                   GstPadDirection direction,
                                   GstCaps *caps, GstCaps *filter);

static gboolean gst_bayer_crop_transform_size (GstBaseTransform *trans,
                                   GstPadDirection direction,
                                   GstCaps *caps, gsize size,
                                   GstCaps *othercaps, gsize *othersize);

static gboolean gst_bayer_crop_get_unit_size  (GstBaseTransform *trans, GstCaps *caps,
                                   gsize *size);

/* GObject vmethod implementations */

/* initialize the plugin's class */
static void
gst_bayer_crop_class_init (GstBayerCropClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_bayer_crop_set_property;
  gobject_class->get_property = gst_bayer_crop_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
    g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  gst_element_class_set_details_simple (gstelement_class,
    "Bayercrop",
    "Filter",
    "BaseTransform Filter",
    "carlos.aguero@ridgerun.com");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

/*  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_bayer_crop_transform_ip);*/
  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_bayer_crop_transform);

  GST_BASE_TRANSFORM_CLASS (klass)->transform_size =
      GST_DEBUG_FUNCPTR (gst_bayer_crop_transform_size);

  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_bayer_crop_get_unit_size);

  GST_BASE_TRANSFORM_CLASS (klass)->transform_caps =
      GST_DEBUG_FUNCPTR (gst_bayer_crop_transform_caps);

  /* debug category for fltering log messages
   *
   * FIXME:exchange the string 'Template plugin' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_bayer_crop_debug, "bayercrop", 0, "Template plugin");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_bayer_crop_init (GstBayerCrop *filter)
{
  filter->silent = FALSE;
}

static void
gst_bayer_crop_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBayerCrop *filter = GST_BAYER_CROP (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_bayer_crop_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBayerCrop *filter = GST_BAYER_CROP (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

static GstCaps* gst_bayer_crop_transform_caps (GstBaseTransform *trans,
                                   GstPadDirection direction,
                                   GstCaps *caps, GstCaps *filter)
{

  GstCaps *fixed_caps;

  if (direction == GST_PAD_SRC) {
    fixed_caps = gst_caps_new_simple ("video/x-bayer",
    "format", G_TYPE_STRING, "bggr",
    "framerate", GST_TYPE_FRACTION, 20, 1,
    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    "width", G_TYPE_INT, 640,
    "height", G_TYPE_INT, 480,
    NULL);


  } else if (direction == GST_PAD_SINK) {
    fixed_caps = gst_caps_new_simple ("video/x-bayer",
    "format", G_TYPE_STRING, "bggr",
    "framerate", GST_TYPE_FRACTION, 20, 1,
    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    "width", G_TYPE_INT, OUT_WIDTH,
    "height", G_TYPE_INT, OUT_HEIGHT,
    NULL);
  }
  return fixed_caps;

}


static gboolean gst_bayer_crop_get_unit_size  (GstBaseTransform *trans, GstCaps *caps,
                                   gsize *size)

{
  GstBayerCrop *filter = GST_BAYER_CROP (trans);

  GST_ERROR_OBJECT (filter, "\n>>>>>> fixed caps from fixed are: %" GST_PTR_FORMAT, caps);
  size=OUT_SIZE;

  return TRUE;
}

static gboolean gst_bayer_crop_transform_size (GstBaseTransform *trans,
                                   GstPadDirection direction,
                                   GstCaps *caps, gsize size,
                                   GstCaps *othercaps, gsize *othersize)
{

  GstCaps *fixed_caps;

  GST_ERROR ("gsize: %u", size);

  if (direction == GST_PAD_SRC) {
/*    othercaps = gst_caps_new_simple ("video/x-bayer",
    "format", G_TYPE_STRING, "bggr",
    "framerate", GST_TYPE_FRACTION, 20, 1,
    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    "width", G_TYPE_INT, 640,
    "height", G_TYPE_INT, 480,
    NULL);*/
    *othersize = 640*480;
  } else if (direction == GST_PAD_SINK) {
/*    othercaps = gst_caps_new_simple ("video/x-bayer",
    "format", G_TYPE_STRING, "bggr",
    "framerate", GST_TYPE_FRACTION, 20, 1,
    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    "width", G_TYPE_INT, 640,
    "height", G_TYPE_INT, 480,
    NULL);*/
    *othersize = OUT_SIZE;
  }

  return TRUE;
}

static GstFlowReturn
gst_bayer_crop_transform (GstBaseTransform * base, GstBuffer * inbuf, 
                               GstBuffer *outbuf)
{
  GstBayerCrop *filter = GST_BAYER_CROP (base);

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

  if (filter->silent == FALSE)
    g_print ("transform, therefore I'm in.\n");
  
  /* FIXME: do something interesting here.  This simply copies the source
   * to the destination. */
/*  outbuf = gst_buffer_copy_region (inbuf,
                                   GST_BUFFER_COPY_METADATA | GST_BUFFER_COPY_MEMORY,
                                   0,
                                   320*240);*/
//  GST_ERROR("in size: %i", gst_buffer_get_size(inbuf));
//  GST_ERROR("out size: %i", gst_buffer_get_size(outbuf));
  
//  gst_buffer_copy_into (outbuf, inbuf, GST_BUFFER_COPY_ALL, 0 , gst_buffer_get_size(outbuf));
  GstMapInfo inmap, outmap;
  GstClockTime timestamp;

  timestamp = GST_BUFFER_TIMESTAMP (inbuf);
  gst_buffer_map (inbuf, &inmap, GST_MAP_READ);
  gst_buffer_map (outbuf, &outmap, GST_MAP_WRITE);

//  outbuf = gst_buffer_copy (inbuf);
/*
OUT_WIDTH 320
OUT_HEIGHT 240
OUT_SIZE 76800 //640*480=307200 320*240=76800
*/
  int i;
  for (i=0; i < 400; i++) { //400
//  GST_ERROR ("data size: %p", outmap.data);
//  outmap.data = guint8

    memcpy (outmap.data, inmap.data, 600); //copies n bytes //320
    outmap.data += 600;
    inmap.data += 640;
  }

/*  int i;
  for (i=0; i<OUT_HEIGHT; i++) {
    memcpy (outmap.data, inmap.data, OUT_WIDTH);
    inmap.data += 320;
  }*/

  return GST_FLOW_OK;
}

/* this function does the actual processing
 */
static GstFlowReturn
gst_bayer_crop_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  GstBayerCrop *filter = GST_BAYER_CROP (base);

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

  if (filter->silent == FALSE)
    g_print ("I'm plugged, therefore I'm in.\n");
  
  /* FIXME: do something interesting here.  This simply copies the source
   * to the destination. */

  return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "bayercrop", GST_RANK_NONE,
      GST_TYPE_BAYER_CROP);
}

/* gstreamer looks for this structure to register plugins
 *
 * FIXME:exchange the string 'Template plugin' with you plugin description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bayercrop,
    "BayerCrop plugin",
    plugin_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
