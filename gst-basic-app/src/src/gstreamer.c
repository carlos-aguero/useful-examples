

#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>

static int debug = 0;

#define vvdbg(format, arg...)  if (debug>2) fprintf(stderr, "[%s|%d]: " format "\n", __FILE__, __LINE__, ##arg)
#define vdbg(format, arg...)  if (debug>1) fprintf(stderr, "[%s|%d]: " format "\n", __FILE__, __LINE__, ##arg)
#define dbg(format, arg...)   if (debug>0) fprintf(stderr, "[%s|%d]: " format "\n",  __FILE__, __LINE__, ##arg)

//command line menu
static const GOptionEntry entries[] = {
  {"debug", 'k', FALSE, G_OPTION_ARG_INT, &debug,
        "Set debug message level: 0 = No debug; 1 = debug; 2 = verbose debug",
      "DEBUG"},
  {NULL}
};

/*
 * Dump out the data to ".dot" files.
 */
static void
debug_dump (const int line, GstElement * const pipeline)
{
  // Generate lots of debugging information
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, "media_type");
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS,"cap_details");
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS,"params");
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_STATES,"states");
}

typedef struct
{
	GstElement *master_pipeline, *interpipesrc, *interpipesink,
	    *filesrc, *demux, *decoder;
	GMainLoop *loop;

} ServerElements;

static void
cb_new_pad (GstElement *element,
            GstPad     *pad,
            gpointer    data)
{
	
	ServerElements* server_elements = (ServerElements*) data;
	
	vdbg("At demux callback!!!");
	
    gchar *name;

    name = gst_pad_get_name (pad);
    vdbg("At demux callback. Linking: %s", name);
    
    if(!gst_element_link_pads(server_elements->demux, name, server_elements->decoder, "sink"))
    {
		vdbg("Cant link demux with decoder!");
		abort();
	}
	debug_dump (__LINE__, server_elements->master_pipeline);
    vdbg("At demux callback!!!");
    
    g_free (name);
}


/*
 * handle_socket -- Create and handle incoming socket requests
 */
static void *
handle_pipe_state (void *data)
{
	ServerElements *server_elements = (ServerElements *) data;
	static int val = 0;
	static int first_try = 0;
  
	while (TRUE)
	{
		sleep(1);
		
		if(val == 0)
		{
			vdbg("Pipe to PLAY");
			gst_element_set_state (server_elements->master_pipeline, GST_STATE_PLAYING);

			// wait until it's up and running or failed
			if (gst_element_get_state (server_elements->master_pipeline, NULL, NULL,
				-1) == GST_STATE_CHANGE_FAILURE)
			{
				g_error ("Failed to set master_pipeline into PLAYING state");
			}
			
			val = 1;
		}
		else
		{
			vdbg("Pipe to NULL");
			gst_element_set_state (server_elements->master_pipeline, GST_STATE_NULL);

			// wait until it's up and running or failed
			if (gst_element_get_state (server_elements->master_pipeline, NULL, NULL,
				  -1) == GST_STATE_CHANGE_FAILURE)
			{
				g_error ("Failed to set master_pipeline into NULL state");
			}
			
			val = 0;
		}
	}

  return(NULL);
}


int main (int argc, char ** argv) {

	ServerElements server_elements;

	//Variables
	vdbg("DEBUG");
	
	//Main variables
	GOptionContext *context;
	GOptionGroup *gstreamer_group;
	GError *error = NULL;
	pthread_t run_thread;
	
	//GST init and main loop
	gst_init (&argc, &argv);
	server_elements.loop = g_main_loop_new (NULL, FALSE);
	
	vdbg("DEBUG");
	//Context and command line
	context = g_option_context_new ("linear server");

	gstreamer_group = gst_init_get_option_group ();
	g_option_context_add_group (context, gstreamer_group);

	g_option_context_add_main_entries (context, entries, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr
			("Usage: %s [-w <v4l2src width> -h <v4l2src height> -d <run as daemon when set> -t <use videotestsrc when set> -f <frames per second>]\n",
			argv[0]);
		g_printerr
			("VIDEO_PATTERN defines which test pattern to use\n");
			g_printerr ("ERROR: option parsing failed: %s\n", error->message);
		exit (1);
	}

	vdbg("DEBUG");
	
	//Create new pipeline as master_pipeline
	server_elements.master_pipeline = gst_pipeline_new ("master_pipeline");
	
	//with demux
	//http://stackoverflow.com/questions/17960652/gstreamerdynamically-link-realize
	
	//Pipeline
	/*server_elements.interpipesink =
      gst_parse_bin_from_description ("filesrc location=/recording.mp4 ! qtdemux name=demux ! dmaidec_h264 numOutputBufs=30 ! interpipesink node-name=node ", TRUE, NULL);*/
    
	server_elements.filesrc = gst_element_factory_make ("filesrc", "filesrc");
	server_elements.demux = gst_element_factory_make ("qtdemux", "qtdemux");
	server_elements.decoder = gst_element_factory_make ("dmaidec_h264", "dmaidec_h264");
	//server_elements.interpipesink = gst_element_factory_make ("fakesink", "fakesink");
	server_elements.interpipesink = gst_element_factory_make ("interpipesink", "interpipesink");
	
	g_object_set (G_OBJECT (server_elements.filesrc), "location", "/recording.mp4", NULL);
	g_object_set (G_OBJECT (server_elements.decoder), "numOutputBufs", 30, NULL);
	g_object_set (G_OBJECT (server_elements.interpipesink), "node-name", "node", NULL);
	g_object_set (G_OBJECT (server_elements.interpipesink), "sync", FALSE, NULL);
	g_object_set (G_OBJECT (server_elements.interpipesink), "async", FALSE, NULL);
            
    server_elements.interpipesrc =
      gst_parse_bin_from_description ("interpipesrc listen-to=node ! fakesink enable-last-buffer=false \
        async=false sync=true", TRUE, NULL);
        
	gst_bin_add_many (GST_BIN (server_elements.master_pipeline),
      server_elements.filesrc, server_elements.demux, server_elements.decoder, server_elements.interpipesink, server_elements.interpipesrc, NULL);
      
	if (!gst_element_link(server_elements.filesrc, server_elements.demux)) {
		printf("link filesrd--demux fail\n");
		return;
	}
	if (!gst_element_link(server_elements.decoder, server_elements.interpipesink)) {
		printf("link decoder--sink fail\n");
		return;
	}

	g_signal_connect (server_elements.demux, "pad-added", G_CALLBACK (cb_new_pad), &server_elements);

	//Run pipeline at thread
	vdbg("DEBUG");

	debug_dump (__LINE__, server_elements.master_pipeline);
	vdbg("DEBUG");
	//Thread that changes pipeline state
	vdbg ("Creating thread for handle_socket(...)");
	if (pthread_create (&run_thread, NULL, handle_pipe_state,
		  (void *) &server_elements) != 0) {
		g_error ("Failed creating handle_pipe_state thread");
		exit (-1);
	}
	vdbg("DEBUG");
	printf("\nRunning\n");
	g_main_loop_run (server_elements.loop);

	return 0;
}


#if 0


#define MOTION_ENABLE 1
//#undef MOTION_ENABLE
/* 
 * Modify license
 * // LINEAR: I believe that this is a work for hire.   We own the copyright and it's not GPL
 * Copyright (C) RidgeRun 2013
 *     Carlos Aguero <carlos.aguero@ridgerun.com>
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
 * 
 *
 * BUGS (Linear internal bookkeeping):
 *  2. Probably have lots of memory leaks.
 *  6. Need to get configuration parameters from the configuration file.
 *  7. Multiple RTSP connections not tested
 *  8. Teardown of RTSP streams is not tested
 */

#define vvdbg(format, arg...)  if (debug>2) fprintf(stderr, "[%s|%d]: " format "\n", __FILE__, __LINE__, ##arg)
#define vdbg(format, arg...)  if (debug>1) fprintf(stderr, "[%s|%d]: " format "\n", __FILE__, __LINE__, ##arg)
#define dbg(format, arg...)   if (debug>0) fprintf(stderr, "[%s|%d]: " format "\n",  __FILE__, __LINE__, ##arg)

#define MAX_AMOUNT_OF_CLIENTS 7
#define RTSP_DEFAULT_WIDTH 640
#define RTSP_DEFAULT_HEIGHT 480
#define RTSP_DEFAULT_FPS 15
#define MJPEG_DEFAULT_WIDTH 320
#define MJPEG_DEFAULT_HEIGHT 240
#define MJPEG_DEFAULT_FPS 10

#ifdef MOTION_ENABLE

//MD at 320x240
static const unsigned int MOTION_WIDTH = 320;	  	// Width of the motion detection window
static const unsigned int MOTION_HEIGHT = 240;  	// Height of the motion detection window

static const unsigned int N_MOTION = 4;	// Number of motion detection windows

#define MD_NAME_0 motion_0
#define MD_NAME_1 motion_1
#define MD_NAME_2 motion_2
#define MD_NAME_3 motion_3

#define MD_WIDTH 319
#define MD_HEIGHT 239
#define MD_PIX_TO_MOTION 50
#define MD_FGTHRESH 5 //depends on light

//Coordinate MD instances
#define MD_INTERVAL 12
#define MD_FRAMES_OFF0 0
#define MD_FRAMES_OFF1 3
#define MD_FRAMES_OFF2 6
#define MD_FRAMES_OFF3 9

#define MOTION(ELEMENT_NAME, FGTHRESH, PIX_TO_MOTION, HEIGHT, WIDTH, INTERVAL, FRAMES_OFF) "motion name=" #ELEMENT_NAME " fg_thresh=" #FGTHRESH " pix-to-motion=" #PIX_TO_MOTION " top=0 bottom=" #HEIGHT " left=0 right=" #WIDTH " no-motion-interval=" #INTERVAL " motion-interval=" #INTERVAL " frames-offset=" #FRAMES_OFF

//gst-launch v4l2src always-copy=false ! dmaiaccel ! capsfilter caps='video/x-raw-yuv,width=640,height=480,framerate=(fraction)30/1' ! $MOTION0 ! $MOTION1 ! $MOTION2 ! $MOTION3 ! dmaiperf name=perf print-arm-load=true ! TIDmaiVideoSink videoStd=D1_NTSC videoOutput=composite sync=false enable-last-buffer=false --gst-debug=motion:3

#endif

/*
 * mjpeg video streamer.
 *
 * This code is designed to serve as a development base for a multi-stream
 * application server for a mjpeg stream.
 *
 * It takes video from v4l2source and audio from audiotestsrc and produces
 * multiple mjpeg streams wth AlarmForce padded stream.
 *
 * It also servers up RTSP video and audio as well.
 * 
 */
#include <map>
#include <set>
#include <cassert>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>

#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>

#include <gio/gio.h>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp-server/rtsp-media-factory.h>

#include "mjpeg.fd.socket.h"
#include "../../../gst-linearmjpegaudio/src/src/gstlinearmjpegaudio.h"

GstElement *debug_element = NULL;

#define VALUE_INIT {0, {{0}, {0}}}

static int debug = 0;
/*----------------Elements names-----------------*/
//Video
static const char *const VIDEO_FAKE = "video_fake";
static const char *const VIDEO_SRC_TEE = "video_src_tee";

//Audio
static const char *const AUDIO_SRC_TEE_ELEMENT = "audio_src_tee";
static const char *const AUDIO_FAKE_ELEMENT = "audio_fake";

//Linear sink
static const char *const LINEAR_MJPEG_SINK = "linearmjpegsink";

//Default values for v4l2src resolution
static gint v4l2src_width = 640;
static gint v4l2src_height = 480;
static gint v4l2src_fps = 30;
static gboolean cmd_videotest = false;
static gboolean cmd_audiotest = false;
static gboolean first_try = true;

//command line menu
static const GOptionEntry entries[] = {
  {"width", 'w', 0, G_OPTION_ARG_INT, &v4l2src_width,
        "Width used to set v4l2src resolution",
      "WIDTH"},
  {"height", 'h', 0, G_OPTION_ARG_INT, &v4l2src_height,
        "Height used to set v4l2src resolution",
      "HEIGHT"},
  {"fps", 'f', 0, G_OPTION_ARG_INT, &v4l2src_fps,
        "fps used to set v4l2src frames per seconds",
      "FPS"},
  {"audiotest", 'a', false, G_OPTION_ARG_NONE, &cmd_audiotest,
        "If set use audiotestsrc as audio source instead of alsasrc",
      "AUDIOTEST"},
  {"videotest", 't', false, G_OPTION_ARG_NONE, &cmd_videotest,
        "If set use videotestsrc as video source instead of v4l2src",
      "VIDEOTEST"},
  {"debug", 'k', false, G_OPTION_ARG_INT, &debug,
        "Set debug message level: 0 = No debug; 1 = debug; 2 = verbose debug",
      "DEBUG"},
  {NULL}
};

/*
 * Information about a single linear sink
 */
typedef struct
{
  int fd;			// FD we are writing to
  unsigned int fps;		// FPS for the video
  unsigned int width;		// Size of the video
  unsigned int height;
  gboolean padding;		// Are we padding

} LinearSinkInfo;

struct mjpeg_sink_info
{
  unsigned int id;              // ID number
  int fd;                       // FD we are writing to
  unsigned int unlink_count;    // Number of unlink's we attempted
  GstElement *linearmjpegaudio; // Element we are working on
  unsigned int audio_probe_id;  // ID of the audio probe cb
  unsigned int video_probe_id;  // ID of the video probe cb
};

typedef struct
{
  //File descriptor
  int file_descriptor;
  //src pad info: what kind of info? name?
  char *jpeg_pad_name;
  char *audio_pad_name;
  char *element_name;

  gboolean in_use;

} SinkInfo;

// Pointer to a gst element
typedef GstElement* GstElementPtr;

typedef struct
{

  GstElement *master_pipeline, *video_src_pipeline, *audio_src_pipeline,
   *jpeg_pipeline, *video_src_caps, *audio_src_caps, *video_src,
   *video_tee, *audio_src, *mulaw_enc, *audio_tee, *videorate,
   *videoscale, *video_caps, *jpegenc, *jpeg_tee, *audio_fake,
   *video_fake, *accel, *video_src_tee, *audio_src_tee,
   *jpeg_pipeline_tee, *jpeg_pipeline_fakesink, *rate_caps, *h264_pipeline,
   *rtspsink, *jpeg_dmairesizer, *jpeg_pipeline_fps_caps;
  GstElementPtr motion_detect[N_MOTION];

  GstPad *audio_tee_src_to_fakesink_pad, *audio_tee_src_to_master_pipeline_pad,
   *audio_fakesink_sink_pad, *video_tee_src_to_fakesink_pad,
   *video_audio_fakesink_sink_pad, *video_tee_src_to_master_pipeline_pad,
   *video_fakesink_sink_pad, *jpeg_tee_src_to_master_pipeline_pad,
   *video_rate_sink_pad, *video_src_bin_src_pad, *video_src_tee_src_pad,
   *video_src_tee_sink_pad, *video_fake_sink_pad,
   *video_src_tee_src_to_master_pipeline_pad, *video_src_tee_to_jpeg_src_pad,
   *video_src_tee_src_to_jpeg_src_pad, *audio_src_bin_src_pad, *audio_fake_sink_pad,
   *audio_src_tee_sink_pad, *jpeg_pipeline_sink_pad, *jpeg_pipeline_src_pad,
   *jpeg_pipeline_tee_sink_pad, *jpeg_pipeline_tee_src_pad,
   *jpeg_pipeline_fakesink_sink_pad, *h264_pipeline_sink_pad, *h264_pipeline_src_pad,
   *video_src_tee_src_pad_for_h264_pipeline, *rtspsink_sink_pad;

  //Array of pointers to gstreamer pads
  GstPad *linearmjpegaudio_audio_sink_pad[MAX_AMOUNT_OF_CLIENTS];
  GstPad *linearmjpegaudio_video_sink_pad[MAX_AMOUNT_OF_CLIENTS];
  GstPad *audio_src_tee_src_to_linear_pad[MAX_AMOUNT_OF_CLIENTS];
  GstPad *audio_src_tee_src_pad;

  //Array of poitners to GstElements
  GstElement *linearmjpegaudio[MAX_AMOUNT_OF_CLIENTS];
  GstPad *jpeg_pipeline_tee_src_pad_for_linear_sink[MAX_AMOUNT_OF_CLIENTS];
  
  //Array of Sink info to store sink element information
  SinkInfo sink_info[MAX_AMOUNT_OF_CLIENTS];

  GMainLoop *loop;

  //pad probes buffer drop control  
  gboolean video_src_bin_src_pad_buffer_pass;
  gboolean video_src_tee_to_jpeg_src_pad_buffer_pass;
  gboolean audio_src_bin_src_pad_buffer_pass;
  gboolean audio_src_tee_to_linear_pad_buffer_pass;
  gboolean jpeg_pipeline_src_pad_buffer_pass;
  gboolean jpeg_pipeline_created;
  gboolean video_src_tee_src_pad_for_h264_pipeline_buffer_pass;
  gboolean h264_pipeline_src_pad_buffer_pass;
  
  gboolean pending_request;

  guint configured_jpeg_width, configured_jpeg_height, configured_jpeg_fps;

  int remove_fd;

  //to keep in control the amount of linear sinks
  int sink_amount;
  int jpeg_fps;
  
  //Results for numerator/denominator of buffer drop for jpeg encoder pipeline
  gint result_num;
  gint result_den;
  gint buffer_pass_counter;
  gint total_buffer_counter;
  
  //h264 rtsp config
  gint rtsp_fps;
  gint rtsp_height;
  gint rtsp_width;

  struct mjpeg_sink_info info;
  struct mjpeg_sink_info *cur_info;

  int tmp_index;
  guint state;

} ServerElements;

static ServerElements *
    p_server_elements;

// Code fragement that's used a lot to create elements
#define MAKE_ELEMENT(var, factory, name) \
    var = gst_element_factory_make(factory, name); \
    if (var == NULL) { \
      std::cerr << "Could not create factory " << factory << " name " << name << " element" << std::endl; \
      abort(); \
    }

static
    pthread_mutex_t pthread_recursive_mutex_initializer_np = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
// Class that provides a wrapper around a mutex lock
class thread_lock
{

private:
  const char *const name;        // Name of the lock
  pthread_mutex_t lock_mutex;      // The lock

  const char *last_file;          // Last locker
  int last_line;
  
public:
  thread_lock (const char *const _name):
  name (_name),last_file ("--"),last_line (-1)
  {
    lock_mutex = pthread_recursive_mutex_initializer_np;
  }
  void
  lock (const char *const file, const int line)
  {
    if (debug > 0) {
      std::cout << "\nLOCK " << name << ": " << file << ':' << line << ' ' <<
          std::hex << pthread_self () << std::dec << std::endl;
    }
    pthread_mutex_lock (&lock_mutex);
    last_file = file;
    last_line = line;
  }
  void
  unlock (const char *const file, const int line)
  {
    if (debug > 0) {
      std::cout << "UNLOCK " << name << ": " << file << ':' << line << ' ' <<
          std::hex << pthread_self () << std::dec << std::endl;
    }
    pthread_mutex_unlock (&lock_mutex);
  }
};

// Lock for any major stuff being done to the system
static thread_lock master_lock ("master");

/*
 * debug_indent -- Print an indent
 *
 * Parameters
 *  level -- The level of the indent
 */
static void
debug_indent (const unsigned int level)
{
  for (unsigned int i = 0; i < level; ++i) {
    std::cout << "    ";
  }
}

/*
 * given state -- return string
 */
static
    std::string
state_2_string (const GstState state)
{
  switch (state) {
    case GST_STATE_VOID_PENDING:
      return (std::string ("GST_STATE_VOID_PENDING"));
    case GST_STATE_NULL:
      return (std::string ("GST_STATE_NULL"));
    case GST_STATE_READY:
      return (std::string ("GST_STATE_READY"));
    case GST_STATE_PAUSED:
      return (std::string ("GST_STATE_PAUSED"));
    case GST_STATE_PLAYING:
      return (std::string ("GST_STATE_PLAYING"));
    default:
      return (std::string ("Unknown"));
  }
}

/*
 * debug_tree_helper -- Dump a tree recursively
 *
 * Parameters
 *  indent -- amount to indent
 *  element -- Root of the tree to dump
 */
static void
dump_tree_helper (const unsigned int indent, GstElement * element)
{
  debug_indent (indent);
  std::cout << gst_element_get_name (element) << " " <<
      state_2_string (GST_STATE (element)) << std::endl;
  // The iterator to list all the pads
  GstIterator *cur_pad = gst_element_iterate_pads (element);
  while (1) {
    GstPad *pad = NULL;         // The pad we are looking at

    if (gst_iterator_next (cur_pad,
            reinterpret_cast < void **>(&pad)) != GST_ITERATOR_OK)
        break;

    debug_indent (indent + 1);
    std::cout << "PAD: " << gst_pad_get_name (pad);
    // Who are we connected to
    GstPad *peer = gst_pad_get_peer (pad);
    if (peer != NULL) {
      // Get who we are connected to if any
      GstElement *parent = gst_pad_get_parent_element (peer);
      if (parent == NULL) {
        std::cout << "--> ???:" << gst_pad_get_name (peer);
        std::cout << std::endl;
      } else {
        std::cout << "--> " << gst_element_get_name (parent) << ":" <<
            gst_pad_get_name (peer);
        std::cout << std::endl;
        if (gst_pad_get_direction (pad) == GST_PAD_SRC) {
          dump_tree_helper (indent + 2, parent);
        }
        gst_object_unref (parent);
      }
      gst_object_unref (peer);
    } else {
      std::cout << std::endl;
    }
    gst_object_unref (pad);
  }
}

#if 0
/*
 * debug_dump_tree -- Dump a tree view of the pipeline
 *
 * Parameters
 *  line -- The line number for the dump
 */
static void
debug_dump_tree (const int line, ServerElements * server_elements)
{
  std::cout << "DUMP " << line << std::endl;
  dump_tree_helper (1, server_elements->video_src);
  dump_tree_helper (1, server_elements->audio_src);
  std::cout << std::endl;
}
#endif // 0

/*
 * Dump out the data to ".dot" files.
 */
static void
debug_dump (const int line, GstElement * const pipeline)
{
  static int sequence = 0;      // Sequence number we are using
  std::ostringstream prefix;    // Output file prefix

  prefix << std::setw (3) << std::setfill ('0') << sequence << '.' << std::
      setw (0) << line << '_';
  ++sequence;

  // Generate lots of debugging information
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_MEDIA_TYPE, (prefix.str () + "media_type").c_str ());
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS,
      (prefix.str () + "cap_details").c_str ());
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS,
      (prefix.str () + "params").c_str ());
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline), GST_DEBUG_GRAPH_SHOW_STATES,
      (prefix.str () + "states").c_str ());
  GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN (pipeline),
      static_cast < GstDebugGraphDetails > (0),
      (prefix.str () + "none").c_str ());
}

static gboolean
audio_src_bin_src_pad_buffer_probe (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  ServerElements *server_elements = (ServerElements *) u_data;
  return server_elements->audio_src_bin_src_pad_buffer_pass;
}

/*
 * build_audio_pipeline -- Create the audio supply pipeline
 */
static void
build_basic_audio_bin (ServerElements * server_elements)
{
    if (cmd_audiotest) {
      static const char* const AUDIO = 
	  "audiotestsrc name=audio_src !"
	  " audio/x-raw-int,rate=8000,channels=2,width=16,depth=16 !"
	  " audioconvert !"
	  " audio/x-raw-int,rate=8000,channels=1,width=16,depth=16 !"
	  " mulawenc name=mulawenc !"
	  " queue";
      std::cout << "AUDIO " << AUDIO << std::endl;
      server_elements->audio_src_pipeline =
          gst_parse_bin_from_description (AUDIO, true, NULL);
    } else {
      server_elements->audio_src_pipeline =
	  gst_parse_bin_from_description
      ("alsasrc latency-time=30000 buffer-time=800000 name=audio_src ! audio/x-raw-int,rate=8000,channels=2,width=16,depth=16 ! audioconvert ! audio/x-raw-int,rate=8000,channels=1,width=16,depth=16 ! mulawenc name=mulawenc ! queue",
      true, NULL);
    }

  gst_bin_add (GST_BIN (server_elements->master_pipeline),
      server_elements->audio_src_pipeline);

  server_elements->audio_src =
      gst_bin_get_by_name (GST_BIN (server_elements->audio_src_pipeline),
      "audio_src");

  server_elements->audio_src_bin_src_pad =
      gst_element_get_static_pad (server_elements->audio_src_pipeline, "src");

  gst_pad_add_buffer_probe (server_elements->audio_src_bin_src_pad,
      G_CALLBACK (audio_src_bin_src_pad_buffer_probe),
      (gpointer) server_elements);
}

/*
 * Adds audio tee and fakesink into pipeline and links audio elements
 * */
static void
connect_audio_src (ServerElements * server_elements)
{
  MAKE_ELEMENT (server_elements->audio_src_tee, "tee", AUDIO_SRC_TEE_ELEMENT);
  MAKE_ELEMENT (server_elements->audio_fake, "fakesink", AUDIO_FAKE_ELEMENT);

  g_object_set (server_elements->audio_fake, "sync", false, "async", false,
      NULL);

  gst_bin_add_many (GST_BIN (server_elements->master_pipeline),
      server_elements->audio_src_tee, server_elements->audio_fake, NULL);

  server_elements->audio_src_tee_src_pad =
      gst_element_get_request_pad (server_elements->audio_src_tee, "src%d");

  server_elements->audio_fake_sink_pad =
      gst_element_get_static_pad (server_elements->audio_fake, "sink");

  server_elements->audio_src_tee_sink_pad =
      gst_element_get_static_pad (server_elements->audio_src_tee, "sink");

  if (gst_pad_link (server_elements->audio_src_tee_src_pad,
          server_elements->audio_fake_sink_pad) != GST_PAD_LINK_OK) {

    g_error ("can't link audio_src_tee -> audio_fake_sink_pad");
  }
  //Memory allocation pad
  g_object_set (server_elements->audio_src_tee, "alloc-pad",
      server_elements->audio_src_tee_src_pad, NULL);

  //Link audio bin with video src tee
  if (gst_pad_link (server_elements->audio_src_bin_src_pad,
          server_elements->audio_src_tee_sink_pad) != GST_PAD_LINK_OK) {

    g_error ("can't link audio_src_bin -> audio_src_tee");
  }

}

/*
 * Buffer probe callback for video src bin
 * */
static gboolean
video_src_bin_src_pad_buffer_probe (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  ServerElements *server_elements = (ServerElements *) u_data;
  
  vvdbg("Buffer out of videosrc: %i", server_elements->video_src_bin_src_pad_buffer_pass);
  return server_elements->video_src_bin_src_pad_buffer_pass;
}

/*
 * Buffer probe ballback for video buffers going to jpeg pipeline
 * */
static gboolean
video_src_tee_to_jpeg_src_pad_buffer_probe (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
   ServerElements *server_elements = (ServerElements *) u_data;
  vvdbg("Buffer passing for video_src_tee_to_jpeg_src_pad_buffer_probe");
  if (!server_elements->video_src_tee_to_jpeg_src_pad_buffer_pass)
    return false;
    
  if(server_elements->result_num == server_elements->result_den)
    return true;
    
  //vvdbg("value: buffer_pass_counter: %i; total_buffer_counter: %i", server_elements->buffer_pass_counter, server_elements->total_buffer_counter);
  if ((server_elements->buffer_pass_counter < server_elements->result_den) && (server_elements->total_buffer_counter < server_elements->result_num))
  {
    //vvdbg("pass");
    server_elements->buffer_pass_counter++;
    server_elements->total_buffer_counter++;
    return true;
  }
  else
  {
    if ((server_elements->total_buffer_counter+1) == server_elements->result_num)
    {
      //vvdbg("drop");
      server_elements->total_buffer_counter = 0;
      server_elements->buffer_pass_counter = 0;
      return false;
    }
    //vvdbg("drop");
    server_elements->total_buffer_counter++;
    return false;
  }
}

/*
 * Adds video tee and fakesink into pipeline and links video elements
 * */
void
connect_video_src (ServerElements * server_elements)
{
  MAKE_ELEMENT (server_elements->video_src_tee, "tee", VIDEO_SRC_TEE);
  MAKE_ELEMENT (server_elements->video_fake, "fakesink", VIDEO_FAKE);

  g_object_set (server_elements->video_fake, "sync", false, "async", false,
      NULL);

  gst_bin_add_many (GST_BIN (server_elements->master_pipeline),
      server_elements->video_src_tee, server_elements->video_fake, NULL);

  server_elements->video_src_tee_src_pad =
      gst_element_get_request_pad (server_elements->video_src_tee, "src%d");

  server_elements->video_fake_sink_pad =
      gst_element_get_static_pad (server_elements->video_fake, "sink");

  server_elements->video_src_tee_to_jpeg_src_pad =
      gst_element_get_request_pad (server_elements->video_src_tee, "src%d");

  server_elements->video_src_tee_sink_pad =
      gst_element_get_static_pad (server_elements->video_src_tee, "sink");

  if (gst_pad_link (server_elements->video_src_tee_src_pad,
          server_elements->video_fake_sink_pad) != GST_PAD_LINK_OK) {

    g_error ("cant link video_src_tee -> video_fake_sink");
  }
  //Memory allocation pad
  g_object_set (server_elements->video_src_tee, "alloc-pad",
      server_elements->video_src_tee_src_pad, NULL);

  //Link video bin with video src tee  
  if (gst_pad_link (server_elements->video_src_bin_src_pad,
          server_elements->video_src_tee_sink_pad) != GST_PAD_LINK_OK) {

    g_error ("can't link video_src_bin -> video_src_tee");
  }
  
  //Buffer drop calculation
  //Assuming always (v4l2src_fps/1)/(jpeg_fps/1)
  gdouble src_to_transform = (gdouble)v4l2src_fps / MJPEG_DEFAULT_FPS;//(gdouble)fps;
  
  gst_util_double_to_fraction(src_to_transform, &server_elements->result_num, &server_elements->result_den);
  vdbg("v4l2src_fps/fps: %f = %i/%i", src_to_transform, v4l2src_fps, MJPEG_DEFAULT_FPS);
  vdbg("From %i buffers, let pass: %i", server_elements->result_num, server_elements->result_den);

  //Init values before buffer probe
  server_elements->buffer_pass_counter = 0;
  server_elements->total_buffer_counter = 0;
  gst_pad_add_buffer_probe (server_elements->video_src_tee_to_jpeg_src_pad,
      G_CALLBACK (video_src_tee_to_jpeg_src_pad_buffer_probe),
      (gpointer) server_elements);
}

/*
 * Creates video src bin
 * Adds buffer probe
 * 
 * */
void
build_basic_video_bin (ServerElements * server_elements)
{
  GstCaps *filtercaps;

  if (cmd_videotest) {
    std::ostringstream source;
    const char* pattern = getenv("VIDEO_PATTERN") == NULL ? "smpte" : getenv("VIDEO_PATTERN");
    source << "videotestsrc name=video_src pattern=" << pattern << " ! capsfilter name=video_src_caps";

    std::cout << "VIDEO " << source.str() << std::endl;
    server_elements->video_src_pipeline = gst_parse_bin_from_description (source.str().c_str(), true, NULL);
  } else {
    static const char* source = "v4l2src name=video_src ! capsfilter name=video_src_caps";
    if (debug)
	std::cout << "SOURCE " << source << std::endl;
    server_elements->video_src_pipeline =
        gst_parse_bin_from_description (source, true, NULL);
  }

  gst_bin_add (GST_BIN (server_elements->master_pipeline),
      server_elements->video_src_pipeline);

  server_elements->video_src =
      gst_bin_get_by_name (GST_BIN (server_elements->video_src_pipeline),
      "video_src");

  server_elements->video_src_caps =
      gst_bin_get_by_name (GST_BIN (server_elements->video_src_pipeline),
      "video_src_caps");

  server_elements->accel =
      gst_bin_get_by_name (GST_BIN (server_elements->video_src_pipeline),
      "accel");

  //Set caps for video source
  std::ostringstream caps_string;
  caps_string << "video/x-raw-bayer,width=" << v4l2src_width << ",height=" <<
      v4l2src_height << ",framerate=" << v4l2src_fps << "/1";

  if (debug > 0)
    std::cout << "Video source caps: " << caps_string.str () << std::endl;

  filtercaps = gst_caps_from_string (caps_string.str ().c_str ());
  g_object_set (server_elements->video_src_caps, "caps", filtercaps, NULL);
  gst_caps_unref (filtercaps);

  if (!cmd_videotest) {
    g_object_set (server_elements->video_src, "always-copy", false, NULL);
    g_object_set (server_elements->video_src, "chain-ipipe", false, NULL);
  }

  server_elements->video_src_bin_src_pad =
      gst_element_get_static_pad (server_elements->video_src_pipeline, "src");

  gst_pad_add_buffer_probe (server_elements->video_src_bin_src_pad,
      G_CALLBACK (video_src_bin_src_pad_buffer_probe),
      (gpointer) server_elements);

}

/*
 * Buffer probe at jpeg_pipeline src pad
 * */
static gboolean
jpeg_pipeline_src_pad_buffer_probe (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  ServerElements *server_elements = (ServerElements *) u_data;
  
  vvdbg("Buffer at jpeg_pipeline src pad buffer probe: %i", server_elements->jpeg_pipeline_src_pad_buffer_pass);
  return server_elements->jpeg_pipeline_src_pad_buffer_pass;
}

#ifdef MOTION_ENABLE

static struct cmd_motion_detected current_motion_pad = {0};
/*
 * send_motion -- Send motion pad data
 */
static void send_motion(ServerElements* server_elements)
{
    inline_pad_data pad_data;	// The data to send

    pad_data.cmd = CMD_MOTION_DETECTED;
    pad_data.size = sizeof(current_motion_pad);
    pad_data.data.cmd_motion_detected = current_motion_pad;

    master_lock.lock (__FILE__, __LINE__);
    for (int i = 0; i < MAX_AMOUNT_OF_CLIENTS; i++) {
	if (server_elements->sink_info[i].in_use) {
	    struct _Gstlinearmjpegaudio* gst_element = 
		reinterpret_cast<struct _Gstlinearmjpegaudio* const>(server_elements->linearmjpegaudio[i]);
	    gst_element->add_pad(gst_element, &pad_data);
	}
    }
    master_lock.unlock (__FILE__, __LINE__);
}
/*
 * MD callbacks:
 * start_motion_callback
 * stop_motion_callback
 * bg_stable_callback
 * bg_timeout_callback
 * */
 
static void
start_motion_callback (GstElement *element,
              gint       pixel_count,
              gpointer    data)
{
    // Get a pointer to the server elements
    ServerElements* server_elements = static_cast<ServerElements*>(data);

    // Get the name of the element
    char* name = gst_element_get_name(element); 
    // motion_x
    // 01234567
    int number = name[7] - '0';

    syslog(LOG_INFO, "Motion detected for %s pixels %d", name, pixel_count);
    std::cout << "DEBUG: Motion " << (name == NULL ? "NULL" : name) << " pixels " << pixel_count << std::endl;

    current_motion_pad.motion_indicator[number]  = (pixel_count * MOTION_HEIGHT * MOTION_HEIGHT) / 255;
    if (current_motion_pad.motion_indicator[number] < current_motion_pad.motion_threshold[number]) {
	syslog(LOG_INFO, "Motion fixup needed for %s.  Indicator %d below limit %d",
		name, current_motion_pad.motion_indicator[number], current_motion_pad.motion_threshold[number]);
	current_motion_pad.motion_indicator[number] = current_motion_pad.motion_threshold[number];
    }
    send_motion(server_elements);

    g_free(name);
}

static void
stop_motion_callback (GstElement *element,
              gpointer    data)
{
    // Get a pointer to the server elements
    ServerElements* server_elements = static_cast<ServerElements*>(data);

    char* name = gst_element_get_name(element); 
    // motion_x
    // 01234567
    int number = name[7] - '0';
    syslog(LOG_INFO, "Motion stop for %s", name);
    std::cout << "DEBUG: Stop motion " << (name == NULL ? "NULL" : name) << std::endl;

    current_motion_pad.motion_indicator[number]  = 0;
    send_motion(server_elements);

    g_free(name);
}

static void
bg_stable_callback (GstElement *element,
              gpointer    data)
{
  char* name = gst_element_get_name(element); 
  vdbg("BG Stable: %s", name);
  g_free(name);
}

static void
bg_timeout_callback (GstElement *element,
              gpointer    data)
{
  char* name = gst_element_get_name(element); 
  vdbg("BG Timeout: %s", name);
  g_free(name);
}
#endif //MOTION_ENABLE
   

static void set_motion_parameters(ServerElements* server_elements);
/*
 * build_jpeg_pipeline
 *
 * NOTE: This does not add the linearmjpegaudio component.
 * Connects jpeg_pipeline to video src 
 */
static void
build_jpeg_pipeline (const int fps,    // Number of FPS for video
    const int width,            // Width of the output
    const int height,           // Height of the output
    ServerElements * server_elements    // Struct with server elements
    )
{
  int qValue = 50;	// Quality value
  // Get the settings for this group
  GSettings* settings = g_settings_new("camera.JPEG");
  if (settings != NULL) {
      // Because of dconf stupidity, it's '-' not '_'
      qValue = g_settings_get_int(settings, "quality-level");

      // Limit the value to something sane
      if (qValue < 1)
	  qValue = 1;
      if (qValue > 97)
	  qValue = 97;
  }
  g_object_unref(settings);

#ifdef MOTION_ENABLE
  
  //MOTION values are variables, currently  set to worst case scenario for testing
  std::ostringstream jpeg_pipeline_string;
     jpeg_pipeline_string << "queue ! dmaiaccel ! dmairesizer target-width=" << width << " target-height=" << height << " name=jpeg_dmairesizer ! \
     tee name=dmaresizer_tee ! queue ! dmaienc_jpeg qValue=50 name=jpegenc dmaresizer_tee. ! \
     queue ! edmaresizer target-width=320 target-height=240 name=h264_edmaresizer ! " <<
     MOTION(motion_0, 5,50, 239, 319, 3, 0) << " ! "
     MOTION(motion_1, 5,50, 239, 319, 3, 1) << " ! "
     MOTION(motion_2, 5,50, 239, 319, 3, 2) << " ! "
     MOTION(motion_3, 5,50, 239, 312, 3, 3) << 
     " ! fakesink sync=false async=false";      
  std::cout << "DEBUG stream " << jpeg_pipeline_string.str() << std::endl;
        
#else	// MOTION
  std::ostringstream jpeg_pipeline_string;
     jpeg_pipeline_string << "queue ! dmaiaccel ! dmairesizer target-width=" << width << " target-height=" << height << 
        " name=jpeg_dmairesizer ! dmaienc_jpeg name=jpegenc qValue=" << qValue << " ! queue ";

#endif // MOTION
  if (debug > 0) {
      std::cout << "jpeg_pipeline_string: " << jpeg_pipeline_string.str () << std::endl;
  }

  server_elements->jpeg_pipeline =
      gst_parse_bin_from_description
      (jpeg_pipeline_string.str ().c_str (),
      true, NULL);

  gst_bin_add (GST_BIN (server_elements->master_pipeline),
      server_elements->jpeg_pipeline);

#ifdef MOTION_ENABLE
  /*
   * MD element callbacks for:
   * 
   * start-motion
   * stop-motion
   * bg-timeout
   * bg-stable
   * 
   * */
   
  for (unsigned int i = 0; i < N_MOTION; ++i) {
      std::string name("motion_");
      name += static_cast<char>(i + '0');
      //Motion detection
      //Pointer to MD element
      server_elements->motion_detect[i] =
	  gst_bin_get_by_name (GST_BIN (server_elements->jpeg_pipeline),
	  name.c_str());
      
      //Signal callback for start-motion signal
       g_signal_connect (server_elements->motion_detect[i], "start-motion", G_CALLBACK (start_motion_callback), server_elements);
       
       //Signal callback for stop-motion signal
       g_signal_connect (server_elements->motion_detect[i], "stop-motion", G_CALLBACK (stop_motion_callback), server_elements);
       
       //Signal callback for bg-stable signal
       g_signal_connect (server_elements->motion_detect[i], "bg-stable", G_CALLBACK (bg_stable_callback), server_elements);
       
       //Signal callback for bg-timeout signal
       g_signal_connect (server_elements->motion_detect[i], "bg-timeout", G_CALLBACK (bg_timeout_callback), server_elements);
       
   }

   set_motion_parameters(server_elements);
       
#endif //MOTION_ENABLE

  /* 
   * Values will be used at build_pipeline states in order to change caps
   * if new resolution or fps are selected
   * */
  server_elements->configured_jpeg_width = width;
  server_elements->configured_jpeg_height = height;
  server_elements->configured_jpeg_fps = fps;

  server_elements->jpegenc =
      gst_bin_get_by_name (GST_BIN (server_elements->jpeg_pipeline), "jpegenc");
  
  server_elements->jpeg_dmairesizer =
      gst_bin_get_by_name (GST_BIN (server_elements->jpeg_pipeline), "jpeg_dmairesizer");
  
  server_elements->jpeg_pipeline_sink_pad =
      gst_element_get_static_pad (server_elements->jpeg_pipeline, "sink");

  server_elements->jpeg_pipeline_src_pad =
      gst_element_get_static_pad (server_elements->jpeg_pipeline, "src");

  gst_pad_add_buffer_probe (server_elements->jpeg_pipeline_src_pad,
      G_CALLBACK (jpeg_pipeline_src_pad_buffer_probe),
      (gpointer) server_elements);

  if (gst_pad_link (server_elements->video_src_tee_to_jpeg_src_pad,
          server_elements->jpeg_pipeline_sink_pad) != GST_PAD_LINK_OK) {
    g_error
        ("\nERROR Linking: video_src_tee_to_jpeg_src_pad -> jpeg_pipeline_sink_pad\n");
  }
  //Adds a tee to the end of mjpeg_pipeline
  MAKE_ELEMENT (server_elements->jpeg_pipeline_tee, "tee", "jpeg_pipeline_tee");
  gst_bin_add (GST_BIN (server_elements->master_pipeline),
      server_elements->jpeg_pipeline_tee);

  server_elements->jpeg_pipeline_tee_sink_pad =
      gst_element_get_static_pad (server_elements->jpeg_pipeline_tee, "sink");

  //Link jpeg_pipeline_src_pad --> jpeg_pipeline_tee_sink_pad
  //Tee will provide data in case of various linearsink elements
  if (gst_pad_link (server_elements->jpeg_pipeline_src_pad,
          server_elements->jpeg_pipeline_tee_sink_pad) != GST_PAD_LINK_OK) {
    g_error
        ("ERROR Linking: jpeg_pipeline_src_pad -> jpeg_pipeline_tee_sink_pad");
  }
  //Attach fakesink into pipeline tee and set alloc-pad
  MAKE_ELEMENT (server_elements->jpeg_pipeline_fakesink, "fakesink",
      "jpeg_pipeline_fakesink");
  gst_bin_add (GST_BIN (server_elements->master_pipeline),
      server_elements->jpeg_pipeline_fakesink);
  g_object_set (server_elements->jpeg_pipeline_fakesink, "sync", false, "async",
      false, NULL);

  //Request src pad from tee
  //*jpeg_pipeline_tee_src_pad, *jpeg_pipeline_fakesink_sink_pad;
  server_elements->jpeg_pipeline_tee_src_pad =
      gst_element_get_request_pad (server_elements->jpeg_pipeline_tee, "src%d");

  //Request sink pad from fakesink
  server_elements->jpeg_pipeline_fakesink_sink_pad =
      gst_element_get_static_pad (server_elements->jpeg_pipeline_fakesink,
      "sink");

  //Memory allocation pad
  g_object_set (server_elements->jpeg_pipeline_tee, "alloc-pad",
      server_elements->jpeg_pipeline_tee_src_pad, NULL);

  if (gst_pad_link (server_elements->jpeg_pipeline_tee_src_pad,
          server_elements->jpeg_pipeline_fakesink_sink_pad) !=
      GST_PAD_LINK_OK) {
    g_printerr
        ("ERROR Linking:jpeg_pipeline_tee_src_pad -> jpeg_pipeline_fakesink_sink_pad");
  }

}

/*
 * add_linearmjpegaudio -- Add a linearmjpegaudio component 
 *  to the pipeline.
 */
static bool
add_linearmjpegaudio (int fd,   // FD to send the data to
    int width, int height,      // Size of the stream
    gboolean padding,           // Pad the output
    ServerElements * server_elements)
{

  //Find first free index
  int i = 0;
  for (i = 0; i < MAX_AMOUNT_OF_CLIENTS; i++) {
    vdbg ("Searching for first free index at: i=%i", i);
    if (server_elements->sink_info[i].in_use == false)
      break;
  }

  //Check if free slot was found
  if (i == MAX_AMOUNT_OF_CLIENTS)
  {
    g_printerr("No free slot found for new linearsink, client will timeout");
    return false;
  }
  vdbg ("Got first free index at i=%i", i);

  //Creating name for gstreamer element
  std::ostringstream id_str;    // String version of the id
  id_str << '_' << i;

  // Create the stream
  MAKE_ELEMENT (server_elements->linearmjpegaudio[i], "linearmjpegaudio",
      (LINEAR_MJPEG_SINK + id_str.str ()).c_str ());

  g_object_set (server_elements->linearmjpegaudio[i], "padding", padding, NULL);
  g_object_set (server_elements->linearmjpegaudio[i], "width", width, NULL);
  g_object_set (server_elements->linearmjpegaudio[i], "height", height, NULL);
  g_object_set (server_elements->linearmjpegaudio[i], "fd", fd, NULL);

  gst_bin_add_many (GST_BIN (server_elements->master_pipeline),
      server_elements->linearmjpegaudio[i], NULL);

  //Element created, slot in use
  server_elements->sink_info[i].in_use = true;
  server_elements->sink_amount++;

  //Will be used for linking element
  server_elements->tmp_index = i;
  
  return true;

}

/*
 * sync linearsink element with its parent (master_pipeline)
 * */
static void
sync_linearsink_with_parent (ServerElements * server_elements)
{

  if (!gst_element_sync_state_with_parent (server_elements->linearmjpegaudio
          [server_elements->tmp_index]))
    g_error ("Failed to sync linearmjpegaudio[%i] with parent\n",
        server_elements->tmp_index);

  //Buffer pass control
  //video to encoder
  server_elements->video_src_tee_to_jpeg_src_pad_buffer_pass = true;
  server_elements->jpeg_pipeline_src_pad_buffer_pass = true;

  //audio to linear
  server_elements->audio_src_tee_to_linear_pad_buffer_pass = true;

  //sources
  server_elements->audio_src_bin_src_pad_buffer_pass = true;
  server_elements->video_src_bin_src_pad_buffer_pass = true;

}

/*
 * start_all -- Start all the elements
 * Used to start all elements of the pipeline
 */
static void
start_all (ServerElements * server_elements)
{

  //all elements are at NULL when entering here
  //sources
  server_elements->audio_src_bin_src_pad_buffer_pass = true;
  server_elements->video_src_bin_src_pad_buffer_pass = true;

  //audio to linear
  server_elements->audio_src_tee_to_linear_pad_buffer_pass = true;

  //video to encoder
  server_elements->video_src_tee_to_jpeg_src_pad_buffer_pass = true;
  server_elements->jpeg_pipeline_src_pad_buffer_pass = true;
  
  //to h264 pipeline
  server_elements->video_src_tee_src_pad_for_h264_pipeline_buffer_pass = true;
  server_elements->h264_pipeline_src_pad_buffer_pass = true;

  gst_element_set_state (server_elements->master_pipeline, GST_STATE_PLAYING);

  // wait until it's up and running or failed
  if (gst_element_get_state (server_elements->master_pipeline, NULL, NULL,
          -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set master_pipeline into PLAYING state");
  }

}

/*
 * Remove linear sink element by fd
 * Seek into pool for fd, this will give index info for removing 
 * element and pads
 * remove_fd assigned at struct at bus_call
 */
static void
remove_linearmjpegaudio_by_fd (ServerElements * server_elements)
{

  int i = 0;
  for (i = 0; i < MAX_AMOUNT_OF_CLIENTS; i++) {
    vdbg ("Searching for fd at linear sink elements pool, index: %i", i);
    if (server_elements->sink_info[i].file_descriptor ==
        server_elements->remove_fd) {
      break;
    }
  }
  if (i == MAX_AMOUNT_OF_CLIENTS)
  {
    g_printerr ("No file descriptor found at current Linear sinks, can't remove");
    return;
  }
  vdbg ("Got file descriptor match at index: %i", i);

  //Check if element is at master_pipeline
  if (server_elements->sink_info[i].in_use == false)
  {
    vdbg ("linearmjpegaudio sink element already removed at index: %i", i);
    return;
  }

  //Release pads
  gst_element_release_request_pad (server_elements->audio_src_tee,
      server_elements->audio_src_tee_src_to_linear_pad[i]);
  gst_element_release_request_pad (server_elements->jpeg_pipeline_tee,
      server_elements->jpeg_pipeline_tee_src_pad_for_linear_sink[i]);

  // null state
  gst_element_set_state (server_elements->linearmjpegaudio[i], GST_STATE_NULL);

  // wait until it's up and running or failed
  if (gst_element_get_state (server_elements->linearmjpegaudio[i], NULL, NULL,
          -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("linear sink failed to go into NULL state");
  }
  //remove element, this function also removes reference
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
      server_elements->linearmjpegaudio[i]);

  server_elements->sink_info[i].in_use = false;
  server_elements->sink_amount--;

}

/*
 * attach current linear sink (pointed by tmp_index) with audio and video
 * streams
 * */
static void
attach_current_linear_sink (ServerElements * server_elements)
{
  //attach
  //Link audio tee to linear[] element
  if (gst_pad_link (server_elements->audio_src_tee_src_to_linear_pad
          [server_elements->tmp_index],
          server_elements->
          linearmjpegaudio_audio_sink_pad[server_elements->tmp_index]) !=
      GST_PAD_LINK_OK) {
    g_error
        ("ERROR Linking: audio_src_tee_src_to_linear_pad[%i] -> linearmjpegaudio_audio_sink_pad[%i]",
        server_elements->tmp_index, server_elements->tmp_index);
  }
  //Link video tee to linear[] element  
  if (gst_pad_link (server_elements->jpeg_pipeline_tee_src_pad_for_linear_sink
          [server_elements->tmp_index],
          server_elements->
          linearmjpegaudio_video_sink_pad[server_elements->tmp_index]) !=
      GST_PAD_LINK_OK) {
    g_error
        ("ERROR Linking: jpeg_pipeline_tee_src_pad_for_linear_sink[%i] -> linearmjpegaudio_video_sink_pad[%i]",
        server_elements->tmp_index, server_elements->tmp_index);
  }
}

/*
 * Request pads from jpeg tee, audio tee and linearmjpegaudio element
 * store data of these pads at struct. Will be used at link function
 * */
static void
request_pads_and_save (ServerElements * server_elements,
    LinearSinkInfo * linear_sink_info)
{
  //Request src pad from jpeg_pipeline_tee
  server_elements->
      jpeg_pipeline_tee_src_pad_for_linear_sink[server_elements->tmp_index] =
      gst_element_get_request_pad (server_elements->jpeg_pipeline_tee, "src%d");

  //Request src pad from audio_src_tee
  server_elements->audio_src_tee_src_to_linear_pad[server_elements->tmp_index] =
      gst_element_get_request_pad (server_elements->audio_src_tee, "src%d");

  server_elements->linearmjpegaudio_audio_sink_pad[server_elements->tmp_index] =
      gst_element_get_static_pad (server_elements->linearmjpegaudio
      [server_elements->tmp_index], "audio");

  server_elements->linearmjpegaudio_video_sink_pad[server_elements->tmp_index] =
      gst_element_get_static_pad (server_elements->linearmjpegaudio
      [server_elements->tmp_index], "video");

  /*
   * Save data into SinkInfo:
   * file_descriptor
   * jpeg_pad_name
   * audio_pad_name
   * element_name
   * in_use
   * */
  server_elements->sink_info[server_elements->tmp_index].file_descriptor =
      linear_sink_info->fd;
  server_elements->sink_info[server_elements->tmp_index].jpeg_pad_name =
      gst_pad_get_name
      (server_elements->jpeg_pipeline_tee_src_pad_for_linear_sink
      [server_elements->tmp_index]);
  server_elements->sink_info[server_elements->tmp_index].audio_pad_name =
      gst_pad_get_name
      (server_elements->jpeg_pipeline_tee_src_pad_for_linear_sink
      [server_elements->tmp_index]);
  server_elements->sink_info[server_elements->tmp_index].element_name =
      gst_element_get_name (server_elements->
      linearmjpegaudio[server_elements->tmp_index]);

  vdbg (">>>>> Element File Descriptor:%i",
      server_elements->sink_info[server_elements->tmp_index].file_descriptor);
  vdbg (">>>>> Element Pad Name for jpeg: %s",
      server_elements->sink_info[server_elements->tmp_index].jpeg_pad_name);
  vdbg (">>>>> Element Pad Name for audio: %s",
      server_elements->sink_info[server_elements->tmp_index].audio_pad_name);
  vdbg (">>>>> Element Name: %s",
      server_elements->sink_info[server_elements->tmp_index].element_name);
}

/*
 * Stop all linear sink elements and remove from master pipeline
 * */
static void
stop_all_sinks (ServerElements * server_elements)
{

  int i = 0;
  for (i = 0; i < MAX_AMOUNT_OF_CLIENTS; i++) {
    if (server_elements->sink_info[i].in_use) {
      server_elements->remove_fd =
          server_elements->sink_info[i].file_descriptor;
      //Also decrease sink_amount
      remove_linearmjpegaudio_by_fd (server_elements);

      //More sinks to remove?
      if (server_elements->sink_amount <= 0)
        break;
    }
  }

}

#if 0
/*
 * set jpeg elements to null state:
 * -jpeg_pipeline_fakesink
 * -jpeg_pipeline_tee
 * -jpeg_pipeline
 * */
static void
set_jpeg_elements_to_null (ServerElements * server_elements)
{

  //TO NULL
  gst_element_set_state (server_elements->jpeg_pipeline_fakesink,
      GST_STATE_NULL);

  // wait until it's NULL
  if (gst_element_get_state (server_elements->jpeg_pipeline_fakesink, NULL,
          NULL, -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set jpeg_pipeline_fakesink into NULL state");
  }
  //TO NULL
  gst_element_set_state (server_elements->jpeg_pipeline_tee, GST_STATE_NULL);

  // wait until it's NULL
  if (gst_element_get_state (server_elements->jpeg_pipeline_tee, NULL, NULL,
          -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set jpeg_pipeline_tee into NULL state");
  }
  //TO NULL
  gst_element_set_state (server_elements->jpeg_pipeline, GST_STATE_NULL);

  // wait until it's NULL
  if (gst_element_get_state (server_elements->jpeg_pipeline, NULL, NULL,
          -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set jpeg_pipeline into NULL state");
  }
}

/*
 * Sets all buffer_pass to false
 * Not used at this moment
 */
static void
start_general_buffer_drop (ServerElements * server_elements)
{
	
  server_elements->jpeg_pipeline_src_pad_buffer_pass = false;
  server_elements->video_src_tee_to_jpeg_src_pad_buffer_pass = false;
  server_elements->video_src_bin_src_pad_buffer_pass = false;
  server_elements->audio_src_bin_src_pad_buffer_pass = false;
  server_elements->audio_src_tee_to_linear_pad_buffer_pass = false;
  server_elements->jpeg_pipeline_created = false;
  server_elements->video_src_tee_src_pad_for_h264_pipeline_buffer_pass = false;
}

/*
 * remove jpeg related elements from bin:
 * -jpeg_pipeline
 * -jpeg_pipeline_tee
 * -jpeg_pipeline_fakesink
 * */
static void
remove_jpeg_elements_from_bin (ServerElements * server_elements)
{
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
      server_elements->jpeg_pipeline);
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
      server_elements->jpeg_pipeline_tee);
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
      server_elements->jpeg_pipeline_fakesink);

  server_elements->jpeg_pipeline = NULL;
}

/*
 * Remove h264 pipeline related elements
 * */
static void 
remove_h264_elements_from_bin (ServerElements * server_elements)
{
  vdbg("REMOVING h264 elements from bin");

  //To NULL
  // wait until it's NULL
  gst_element_set_state (server_elements->h264_pipeline,
      GST_STATE_NULL);

  if (gst_element_get_state (server_elements->h264_pipeline, NULL,
      NULL, -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set h264_pipeline into NULL state");
  }
  
  //Unlink
  gst_pad_unlink (server_elements->video_src_tee_src_pad_for_h264_pipeline,
      server_elements->h264_pipeline_sink_pad);

  //remove
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
      server_elements->h264_pipeline);
}

/*
 * Unlink jpeg elements
 * Function not being called. Either way, keep it for a while
 * */
static void
unlink_jpeg_elements (ServerElements * server_elements)
{
  //Unlink jpeg_pipeline
  gst_pad_unlink (server_elements->video_src_tee_to_jpeg_src_pad,
      server_elements->jpeg_pipeline_sink_pad);
  gst_pad_unlink (server_elements->jpeg_pipeline_src_pad,
      server_elements->jpeg_pipeline_tee_sink_pad);
}

/*
 * Sync jpeg elements
 * Function not being called. Either way, keep it for a while
 * */
static void
sync_jpeg_elements (ServerElements * server_elements)
{
  if (!gst_element_sync_state_with_parent (server_elements->jpeg_pipeline))
    g_error ("Failed to sync jpeg_pipeline with parent\n");

  if (!gst_element_sync_state_with_parent (server_elements->jpeg_pipeline_tee))
    g_error ("Failed to sync jpeg_pipeline_tee with parent\n");

  if (!gst_element_sync_state_with_parent
      (server_elements->jpeg_pipeline_fakesink))
    g_error ("Failed to sync jpeg_pipeline_fakesink with parent\n");
}

/*
 * Function that sets server elements state to NULL and 
 * remove elements from pipeline
 * 
 * Function not being called. Either way, keep it for a while
 * */
static void server_to_null(ServerElements* server_elements)
{

  g_print("\n>>>>> AUDIO TO NULL STATE\n");
  //Set audio to NULL
  //TO NULL
  gst_element_set_state (server_elements->audio_fake,
      GST_STATE_NULL);

  // wait until it's NULL
  if (gst_element_get_state (server_elements->audio_fake, NULL,
      NULL, -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set audio_fake into NULL state");
  }

  gst_element_set_state (server_elements->audio_src_tee,
      GST_STATE_NULL);

  // wait until it's NULL
  if (gst_element_get_state (server_elements->audio_src_tee, NULL,
      NULL, -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set audio_src_tee into NULL state");
  }

  gst_element_set_state (server_elements->audio_src_pipeline,
      GST_STATE_NULL);

  // wait until it's NULL
  if (gst_element_get_state (server_elements->audio_src_pipeline, NULL,
      NULL, -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set audio_src_pipeline into NULL state");
  }

  //Remove audio
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
  server_elements->audio_fake);

  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
  server_elements->audio_src_tee);

  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
  server_elements->audio_src_pipeline);
}
#endif // 0

#ifdef QQQ
/*
 * Buffer probe for h264 pipeline tee src pad
 * */
static gboolean
video_src_tee_src_pad_for_h264_pipeline_buffer_probe (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  ServerElements *server_elements = (ServerElements *) u_data;
  vvdbg("buffer passing going to h264 rtsp pipeline: %i", server_elements->video_src_tee_src_pad_for_h264_pipeline_buffer_pass);
  return server_elements->video_src_tee_src_pad_for_h264_pipeline_buffer_pass;
}

/*
 * links h264 rtsp pipeline elements and adds buffer probe
 * */
static void link_h264_rtsp_pipeline (ServerElements *server_elements)
{
  //Request src pad from video_src_tee
  server_elements->video_src_tee_src_pad_for_h264_pipeline =
      gst_element_get_request_pad (server_elements->video_src_tee, "src%d");
      
  //Add buffer probe
  gst_pad_add_buffer_probe (server_elements->video_src_tee_src_pad_for_h264_pipeline,
      G_CALLBACK (video_src_tee_src_pad_for_h264_pipeline_buffer_probe),
      (gpointer) server_elements);

  //Attach video_src_tee_src_pad_for_h264_pipeline -> h264_pipeline_sink_pad
  if (gst_pad_link (server_elements->video_src_tee_src_pad_for_h264_pipeline,
      server_elements->h264_pipeline_sink_pad) != GST_PAD_LINK_OK) {
    g_error ("Can't link video_src_tee_src_pad_for_h264_pipeline -> h264_pipeline_sink_pad");
  }
  
}

/*
 * h264 pipeline src pad buffer probe
 * */
static gboolean
h264_pipeline_src_pad_buffer_probe (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  ServerElements *server_elements = (ServerElements *) u_data;
  vvdbg("Buffer out of h264 pipeline: %i", server_elements->h264_pipeline_src_pad_buffer_pass);
  return server_elements->h264_pipeline_src_pad_buffer_pass;
}

/*
 * Set h264 rtsp encode and stream parameters
 * TO DO not wire values to:
 * 15fps
 * 640x480
 * h264 target bitrate=1000000
 * */
static void build_h264_rtsp_pipeline(ServerElements *server_elements)
{

  //Attach rtsp sink at: rtsp://<ip>/img/media.save
  std::ostringstream h264_pipeline_string;
        
  h264_pipeline_string << "dmaiaccel ! bufferdrop ! video/x-raw-yuv,format=(fourcc)NV12,width=" << v4l2src_width << ",height=" << v4l2src_height <<
        ",framerate=" << server_elements->rtsp_fps << "/1 ! edmaresizer target-width=" << server_elements->rtsp_width << " target-height=" << server_elements->rtsp_height << 
        " name=h264_edmaresizer ! queue ! dmaienc_h264 copyOutput=false ratecontrol=1 encodingpreset=2 maxbitrate=600000 targetbitrate=600000 profile=66 level=30 entropy=0 \
        t8x8intra=0 t8x8inter=0 seqscaling=0 idrinterval=5 intraframeinterval=5 name=dmaienc_h264 ! "
        "video/x-h264, framerate=(fraction)15/1, width=(int)640, height=(int)480 ";

  server_elements->h264_pipeline =
      gst_parse_bin_from_description
      (h264_pipeline_string.str ().c_str (),
      true, NULL);

  if (debug > 0) {
      std::cout << "h264_pipeline_string: " << h264_pipeline_string.str () << std::endl;
  }
  
  server_elements->h264_pipeline =
      gst_parse_bin_from_description
      (h264_pipeline_string.str ().c_str (),
      true, NULL);

  gst_bin_add (GST_BIN (server_elements->master_pipeline),
      server_elements->h264_pipeline);
      
  //Get h264 pipeline sink pad
  server_elements->h264_pipeline_sink_pad =
      gst_element_get_static_pad (server_elements->h264_pipeline, "sink");
      
  //Get h264 pipeline sink pad
  server_elements->h264_pipeline_src_pad =
      gst_element_get_static_pad (server_elements->h264_pipeline, "src");

  //rtspsink will be at master pipeline 
  MAKE_ELEMENT (server_elements->rtspsink, "rtspsink", "rtspsink");

  gst_bin_add (GST_BIN (server_elements->master_pipeline),
      server_elements->rtspsink);
      
  g_object_set (server_elements->rtspsink, "mapping", "/img/video.save", NULL);
  g_object_set (server_elements->rtspsink, "service", "554", NULL);

  server_elements->rtspsink_sink_pad =
      gst_element_get_static_pad (server_elements->rtspsink, "sink");
  
  if (gst_pad_link (server_elements->h264_pipeline_src_pad,
      server_elements->rtspsink_sink_pad) != GST_PAD_LINK_OK) {
    g_error ("cant link h264_pipeline_src_pad -> rtspsink_sink_pad");
  }
  
  //add buffer probe
  gst_pad_add_buffer_probe (server_elements->h264_pipeline_src_pad,
      G_CALLBACK (h264_pipeline_src_pad_buffer_probe),
      (gpointer) server_elements);

}
#endif // QQQ

/*
 * Function used for removing jpeg pipeline, in other state jpeg pipeline
 * will be created again with new paramters
 * */
static void remove_jpeg_pipeline(ServerElements *server_elements, LinearSinkInfo *linear_sink_info)
{
  //Start buffer drop
  vdbg("remove jpeg pipe");
  server_elements->jpeg_pipeline_src_pad_buffer_pass = false;
  server_elements->video_src_tee_to_jpeg_src_pad_buffer_pass = false;
  vdbg("remove jpeg pipe");
  //Stop and remove linear sinks
  stop_all_sinks (server_elements);
  vdbg("remove jpeg pipe");
  //Set jpeg related elements to NULL state
  gst_element_set_state (server_elements->jpeg_pipeline_fakesink, GST_STATE_NULL);
  // wait until it's up and running or failed
  if (gst_element_get_state (server_elements->jpeg_pipeline_fakesink, NULL, NULL,
          -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set jpeg_pipeline_fakesink into NULL state");
  }
  vdbg("remove jpeg pipe");
  gst_element_set_state (server_elements->jpeg_pipeline_tee, GST_STATE_NULL);
  // wait until it's up and running or failed
  if (gst_element_get_state (server_elements->jpeg_pipeline_tee, NULL, NULL,
          -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set jpeg_pipeline_tee into NULL state");
  }
  vdbg("remove jpeg pipe");
  gst_element_set_state (server_elements->jpeg_pipeline, GST_STATE_NULL);
  // wait until it's up and running or failed
  if (gst_element_get_state (server_elements->jpeg_pipeline, NULL, NULL,
          -1) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to set jpeg_pipeline into NULL state");
  }
  vdbg("remove jpeg pipe");
  //Unlink jpeg related elements
  gst_pad_unlink (server_elements->jpeg_pipeline_src_pad,
      server_elements->jpeg_pipeline_tee_sink_pad);
  gst_pad_unlink (server_elements->jpeg_pipeline_tee_src_pad,
      server_elements->jpeg_pipeline_tee_sink_pad);
  vdbg("remove jpeg pipe");
  //remove jpeg related elements from master pipeline
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
server_elements->jpeg_pipeline_fakesink);
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
server_elements->jpeg_pipeline_tee);
  gst_bin_remove (GST_BIN (server_elements->master_pipeline),
server_elements->jpeg_pipeline);
vdbg("remove jpeg pipe");
  //release requested pad from tee
  gst_element_release_request_pad (server_elements->video_src_tee, server_elements->video_src_tee_to_jpeg_src_pad);
  vdbg("remove jpeg pipe");
  //To NULL
  server_elements->jpeg_pipeline_fakesink = NULL;
  server_elements->jpeg_pipeline_tee = NULL;
  server_elements->jpeg_pipeline = NULL;
  server_elements->jpeg_pipeline_sink_pad = NULL;
#ifdef QQQ
  server_elements->h264_pipeline = NULL;
#endif
vdbg("remove jpeg pipe");
  //Calculate new fps
  gdouble src_to_transform = (gdouble)v4l2src_fps / (gdouble)linear_sink_info->fps;
  
  gst_util_double_to_fraction(src_to_transform, &server_elements->result_num, &server_elements->result_den);
  vdbg("v4l2src_fps/fps: %f = %i/%i", src_to_transform, v4l2src_fps, linear_sink_info->fps);
  vdbg("From %i buffers, let pass: %i", server_elements->result_num, server_elements->result_den);
  
  //Set new params
  server_elements->configured_jpeg_width = linear_sink_info->width;
  server_elements->configured_jpeg_height = linear_sink_info->height;
  server_elements->configured_jpeg_fps = linear_sink_info->fps;
  
}

/*
 * Creates a new jpeg pipeline and links
 * */
static void
renew_jpeg_pipeline(ServerElements *server_elements, LinearSinkInfo *linear_sink_info)
{
  //attach video_src_tee -> jpeg_pipeline
  //Redundant, but for safety reasons
  server_elements->jpeg_pipeline_src_pad_buffer_pass = false;
  server_elements->video_src_tee_to_jpeg_src_pad_buffer_pass = false;
  
  //Request src pad from video tee
  server_elements->video_src_tee_to_jpeg_src_pad =
  gst_element_get_request_pad (server_elements->video_src_tee, "src%d");

  //Init buffer counter and pass
  server_elements->buffer_pass_counter = 0;
  server_elements->total_buffer_counter = 0;    
  gst_pad_add_buffer_probe (server_elements->video_src_tee_to_jpeg_src_pad,
      G_CALLBACK (video_src_tee_to_jpeg_src_pad_buffer_probe),
    (gpointer) server_elements);

  build_jpeg_pipeline (linear_sink_info->fps, linear_sink_info->width,
      linear_sink_info->height, server_elements);

  //Sync jpeg elements with parent
  if (!gst_element_sync_state_with_parent (server_elements->jpeg_pipeline))
      g_error ("\nFailed to sync jpeg_pipeline with parent\n");

  if (!gst_element_sync_state_with_parent (server_elements->jpeg_pipeline_fakesink))
      g_error ("\nFailed to sync jpeg_pipeline_fakesink with parent\n");

  if (!gst_element_sync_state_with_parent (server_elements->jpeg_pipeline_tee))
      g_error ("\nFailed to sync jpeg_pipeline_tee with parent\n");

  //Let buffers pass
  server_elements->jpeg_pipeline_src_pad_buffer_pass = true;
  server_elements->video_src_tee_to_jpeg_src_pad_buffer_pass = true;
}

/*
 * create jpeg pipeline
 * */
static void
jpeg_pipeline_create(ServerElements *server_elements, LinearSinkInfo *linear_sink_info)
{
#ifdef QQQ
  if(server_elements->h264_pipeline == NULL)
      build_h264_rtsp_pipeline (server_elements);

  link_h264_rtsp_pipeline(server_elements);
#endif // QQQ

  if(!server_elements->pending_request)
  {
    //Build mjpeg for a default resolution
    build_jpeg_pipeline (MJPEG_DEFAULT_FPS, MJPEG_DEFAULT_WIDTH,
        MJPEG_DEFAULT_HEIGHT, server_elements);
  }
  else
  {
    //Build jpeg for configured resolution
    build_jpeg_pipeline (linear_sink_info->fps, linear_sink_info->width,
    linear_sink_info->height, server_elements);
  }
  //Since sinks are set, it is safe to set master_pipeline at PLAY
  start_all (server_elements);
}

/*
 * build_pipeline
 * State machine that add/remove elements, controls buffer drop,
 * start/stop elements, etc.
 */
static void
build_pipeline (ServerElements * server_elements,
    LinearSinkInfo * linear_sink_info)
{
  //enable state machine changes
  gboolean change_state_enable = true;

  while (change_state_enable) {
    vdbg ("Executing state: %d", server_elements->state);
    switch (server_elements->state) {
      case 0:
        //This is the init state, action happened at main, dont do anything here
        server_elements->state = 1;
        break;

      case 1:
        //Build Basic video bin
        build_basic_video_bin (server_elements);
        server_elements->state = 2;
        break;

      case 2:
        //Connect Basic video bin with tee and fakesink
        connect_video_src (server_elements);
        server_elements->state = 3;
        break;

      case 3:
        //Build Basic audio bin
        build_basic_audio_bin (server_elements);
        //Connect basic audio bin with tee and fakesink
        connect_audio_src (server_elements);
        server_elements->state = 4;
        break;

      case 4:
        //Check if video src was running and jump
        if (GST_STATE (server_elements->video_src_pipeline) !=
            GST_STATE_PLAYING) {
		    vdbg("build_jpeg_pipeline, videosrc was not at PLAY");
			server_elements->state = 16;	
        }
        else
        {
	      vdbg("build_jpeg_pipeline, videosrc was at PLAY");
	      server_elements->state = 17;
	    }
        
        break;

      case 5:
        //Init done and idle state
        if(server_elements->pending_request)
        {
          server_elements->pending_request = false;
          server_elements->state = 6;
        }
        else
        {
          change_state_enable = false;
          debug_dump (__LINE__, server_elements->master_pipeline);
        }
        break;

      case 6:

        vdbg ("State 6 deciding jump:");
        if (server_elements->jpeg_pipeline == NULL) {
          vdbg ("server_elements->jpeg_pipeline == NULL");
          server_elements->state = 7;
        } else {
          vdbg ("server_elements->jpeg_pipeline != NULL");
          /*
           * jpeg_pipeline was created previously, check if new CMD_JPEG
           * has different resolution or fps
           * */
          if ((server_elements->configured_jpeg_width !=
                  linear_sink_info->width)
              || (server_elements->configured_jpeg_height !=
                  linear_sink_info->height)
              || (server_elements->configured_jpeg_fps !=
                  linear_sink_info->fps)) {
            /*
             * Stop jpeg pipeline, remove linear sinks and use new
             * resolution of fps
             * */
            server_elements->state = 12;
            break;
          }
          //CMD_JPEG is using same resolution, add linear sink
          server_elements->state = 8;
        }

        break;

      case 7:
        /*
         * Create jpeg pipeline and its elements, also configure jpeg
         * caps according to request
         * */
        build_jpeg_pipeline (linear_sink_info->fps, linear_sink_info->width,
            linear_sink_info->height, server_elements);
         
        server_elements->state = 8;
        break;

      case 8:
        /*
         * Add a new linear sink element into mastar pipeline
         * */
        if(!add_linearmjpegaudio (linear_sink_info->fd, linear_sink_info->width,
            linear_sink_info->height, linear_sink_info->padding,
            server_elements)) {
              vdbg("MAX amount of clients reached, not adding linear sink");
              server_elements->state = 5;
              break;
			}
	//LINEAR Question: What happens if the above call did not find 
	//	a slot for the video?
        /*
         * Do pad request at jpeg_pipeline_tee and audio_src_tee, save
         * sink information at a SinkInfo struct
         * */
        request_pads_and_save (server_elements, linear_sink_info);

        server_elements->state = 10;
        break;

      case 9:
        //state 9 actions now at state 8, 8 and 9 where joined
        //Free for other actions
        //Not using this state, but for safety reasons...
        g_printerr("State 9, should not be used");
        server_elements->state = 5;
        break;

      case 10:
        vdbg ("State 10 deciding jump:");
        vdbg ("server_elements->video_src_pipeline state: %i",
            GST_STATE (server_elements->video_src_pipeline));
        vdbg ("server_elements->linearmjpegaudio[%i] state: %i",
            server_elements->tmp_index,
            GST_STATE (server_elements->
                linearmjpegaudio[server_elements->tmp_index]));
        vdbg ("server_elements->jpeg_pipeline state: %i",
            GST_STATE (server_elements->jpeg_pipeline));

        if (GST_STATE (server_elements->video_src_pipeline) !=
            GST_STATE_PLAYING) {
          //Assume: (if v4l2src is not playing) -> (all needs to be set to PLAY)
          server_elements->state = 11;
          break;
        } else if (GST_STATE (server_elements->jpeg_pipeline) !=
            GST_STATE_PLAYING) {

          vdbg ("v4l2src running and h264 running, but no mjpeg pipeline running");
          server_elements->state = 15;
          break;
        } else
            if (GST_STATE (server_elements->linearmjpegaudio[server_elements->
                    tmp_index]) != GST_STATE_PLAYING) {
          //Sync linear sink with parent
          server_elements->state = 13;
          break;
        }
        g_error ("\nError deciding state\n");

        //Impossible to get here, just for keeping the standard
        break;

      case 11:
        //Attach elements
        attach_current_linear_sink (server_elements);
        //start elements
        start_all (server_elements);

        server_elements->state = 5;
        break;

      case 12:
        /* 
         * Start buffer drop, unlink and remove elements
         * */
         //renew jpeg pipeline parameters: resolution modification, fps mod.
        remove_jpeg_pipeline(server_elements, linear_sink_info);
        server_elements->pending_request = true;
        server_elements->state = 4;
        break;

      case 13:
        //New linear sink done, just stay at main loop
        sync_linearsink_with_parent (server_elements);
        
        //Checking element state:
        vdbg ("State 13 Checking");
        vdbg ("server_elements->video_src_pipeline state: %i",
            GST_STATE (server_elements->video_src_pipeline));
        vdbg ("server_elements->linearmjpegaudio[%i] state: %i",
            server_elements->tmp_index,
            GST_STATE (server_elements->
                linearmjpegaudio[server_elements->tmp_index]));
        vdbg ("server_elements->jpeg_pipeline state: %i",
            GST_STATE (server_elements->jpeg_pipeline));

        //Attach elements
        attach_current_linear_sink (server_elements);

        //Element started, we can now increment the index
        server_elements->state = 5;
        break;

      case 14:
        //remove linear sink by fd, unlink and release requested pads
        //got into this state because of bus_call request
        remove_linearmjpegaudio_by_fd (server_elements);
        server_elements->state = 5;
        break;
        
      case 15:
        sync_linearsink_with_parent (server_elements);
        
        //Checking element state:
        vdbg ("State 15 Checking");
        vdbg ("server_elements->video_src_pipeline state: %i",
            GST_STATE (server_elements->video_src_pipeline));
        vdbg ("server_elements->linearmjpegaudio[%i] state: %i",
            server_elements->tmp_index,
            GST_STATE (server_elements->
                linearmjpegaudio[server_elements->tmp_index]));
        vdbg ("server_elements->jpeg_pipeline state: %i",
            GST_STATE (server_elements->jpeg_pipeline));

        //Attach elements
        attach_current_linear_sink (server_elements);

        //Element started, we can now increment the index
        server_elements->state = 5;
        break;
        
      case 16:
      
        /*
         * Creates jpeg pipeline and h264 for rtsp, consider
         * master pipeline not at PLAY
         */
        jpeg_pipeline_create(server_elements, linear_sink_info);
        server_elements->state = 5;
        break;
        
      case 17:
        /*
         * Creates a new jpeg pipeline, consider master pipeline 
         * into PLAY
         * */
        renew_jpeg_pipeline(server_elements, linear_sink_info);
        server_elements->state = 5;
        break;

      default:
        g_print ("WARNING: linear-server default state reached: %i",
            server_elements->state);
        server_elements->state = 5;
        break;
    }
    vdbg ("Jumped to state : %d\n", server_elements->state);
  }
}

/*
 * Gstreamer bus callback
 * */
static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  ServerElements *server_elements = (ServerElements *) data;

  /* Parse message */
  if (msg != NULL) {
    GError *err;
    gchar *debug_info;
    int tries;

    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ELEMENT:
        // The name really is CamelCase
        if (strcmp (gst_structure_get_name (msg->structure),
                "LinearMjpegAudio") == 0) {

          // Get the value of the fd that caused this message
          const GValue *const fd_value =
              gst_structure_get_value (msg->structure, "fd");

          // Get the fd of the person who just errored out
          int fd = g_value_get_int (fd_value);

          // Shutdown the pipeline state
          /*
           * What to do with request if server still not at state 5
           * At this moment ignoring
           * 
           * If server at state 5, we can continue with petition
           * 
           * TO DO: Add a queue or similar?
           * 
           */
           tries = 0;
           while (server_elements->state != 5)
           {
             vdbg("bus_call: Waiting for server to get into state 5");
             if(tries > 9)
               break;
               
             sleep(1);
           }
           
          if (server_elements->state != 5) {
            g_printerr ("Server state != 5 not able to remove fd; ERROR");
          } else {
            server_elements->state = 14;
            //fd will be used at remove_linearmjpegaudio_by_fd function
            server_elements->remove_fd = fd;
            build_pipeline (server_elements, NULL);
          }

        }
        break;
      case GST_MESSAGE_ERROR:
        gst_message_parse_error (msg, &err, &debug_info);
        std::cerr << "Error received from element " << GST_OBJECT_NAME (msg->
            src)
            << ": " << err->message << std::endl;
        std::cerr << "Debugging information: " << (debug_info ? debug_info :
            "none") << std::endl;;
        g_clear_error (&err);
        g_free (debug_info);
        break;
      case GST_MESSAGE_EOS:
        std::cout << "End-Of-Stream reached." << std::endl;
        break;
      default:
        //Now it is common to get here, a lot of element change state being printed
        if (debug > 3)
          g_printerr("\nUnexpected message received.  Number: %i\n", GST_MESSAGE_TYPE (msg));
        
        break;
    }

  }
  return true;
}

/*
 * init_socket -- Create a socket on which to receive
 *  fd information.
 *
 * Returns
 *  fd -- Socket we created
 *  -1 -- Fatal error
 */
static int
init_socket (void)
{
  if (remove (MJPEG_FD_SOCKET) < 0) {
    if (errno != ENOENT) {
      return (-1);              // Could not remove the old socket
    }
  }
  struct sockaddr_un addr;      // Address we are going to attach to
  path_to_address (MJPEG_FD_SOCKET, &addr);

  int fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return (-1);                // Could not create socket
  }
  if (bind (fd, (struct sockaddr *) &addr, sizeof (struct sockaddr_un)) < 0) {
    close (fd);
    return (-1);
  }
  if (chmod(MJPEG_FD_SOCKET, 0666) != 0) {
      std::cerr << "ERROR: Could not set socket permissions:" << errno << std::endl;
  }
  if (listen (fd, 10) < 0) {
    close (fd);
    return (-1);
  }
  return (fd);
}
/*
 * do_snap -- Get a jpeg snapshot from the camera and write it out
 * 	to the caller.
 *
 * Parameters
 * 	fd -- The fd on which to write the data
 */
//RIDGE RUN -- Make real
static void do_snap(const int fd)
{
    //## FAKE -- should get the data from the jpeg stream
    int read_fd = open("/usr/linear/snap/snap2.jpg", O_RDONLY);
    if (read_fd < 0)
	return;
    while (1) {
	char buffer[10*1024];

	ssize_t read_size = read(read_fd, buffer, sizeof(buffer));
	if (read_size <= 0)
	    break;
	if (write(fd, buffer, read_size) != read_size)
	    break;
    }
    close(read_fd);
}
/*
 * Mute the output base on an external command
 */
//RIDGE RUN -- Make real
static void do_mute(bool mute)
{
    syslog(LOG_INFO, "MUTE %s", mute ? "ON" : "OFF");
    // ## TBD
}
/*
 * get_fd_and_info -- Wait for a socket to send us a message
 *  and return the result of that message.
 *
 * Parameters
 *  socket_fd -- Socket on which to receive data
 *  server_elements -- Informaiton about the current server situation
 *
 * Returns
 *  information about a mjpeg stream.
 *  if the FD is set to -1, there was an error
 */
static struct fd_info
get_fd_and_info (const int socket_fd, ServerElements* const server_elements)
{
    // The default fd information
    struct fd_info fd_info = { 0 };

    // Get a socket on which we accept requests
    int accept_fd = accept (socket_fd, NULL, NULL);
    if (accept_fd < 0) {
	std::cerr << "ERROR: Accept failed" << std::endl;
	return (fd_info);
    }
    // Control message explaing what we want to receive
    union
    {
	struct cmsghdr cmh;
	char control[CMSG_SPACE (sizeof (int))];
    } control_un;

    /* Set 'control_un' to describe ancillary data that we want to receive */
    control_un.cmh.cmsg_len = CMSG_LEN (sizeof (int));
    control_un.cmh.cmsg_level = SOL_SOCKET;
    control_un.cmh.cmsg_type = SCM_RIGHTS;

    struct msghdr msgh;           // Message indicating what control we are using
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof (control_un.control);

    struct iovec iov;             // Io vector for the receive

    /* Set fields of 'msgh' to point to buffer used to receive (real)
       data read by recvmsg() */
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_base = &fd_info;
    iov.iov_len = sizeof (fd_info);

    msgh.msg_name = NULL;         /* We don't need address of peer */
    msgh.msg_namelen = 0;

    /* Receive real plus ancillary data */
    ssize_t read_size = recvmsg (accept_fd, &msgh, 0);
    if (read_size < 0) {
	std::cerr << "ERROR: Unable to perform recvmsg" << std::endl;
	close (accept_fd);
	fd_info.cmd = CMD_BAD;
	return (fd_info);
    }
    // We're only getting one message from this socket

    switch (fd_info.cmd) {
	case CMD_MJPEG:
	case CMD_H264:
	    struct cmsghdr *cmhp;       // Point to the message comiing in
	    cmhp = CMSG_FIRSTHDR (&msgh);
	    if (cmhp == NULL || cmhp->cmsg_len != CMSG_LEN (sizeof (int))) {
		std::cerr << "ERROR: Bad incoming message size" << std::endl;
		fd_info.cmd = CMD_BAD;
		return (fd_info);
	    }
	    if (cmhp->cmsg_level != SOL_SOCKET) {
		std::cerr << "ERROR: cmsg_level != SOL_SOCKET" << std::endl;
		fd_info.cmd = CMD_BAD;
		return (fd_info);
	    }
	    if (cmhp->cmsg_type != SCM_RIGHTS) {
		std::cerr << "ERROR: cmsg_type != SCM_RIGHTS" << std::endl;
		fd_info.cmd = CMD_BAD;
		return (fd_info);
	    }
	    if (fd_info.cmd == CMD_MJPEG) {
		// Get the data and turn it into a fd
		fd_info.data.cmd_mjpeg.fd = *((int *) CMSG_DATA (cmhp));
	    } else {
		fd_info.data.cmd_h264.fd = *((int *) CMSG_DATA (cmhp));
	    }
	    break;
	case CMD_SNAPSHOT:
	    // ### Hack
	    fd_info.data.cmd_mjpeg.fd = accept_fd;
	    return(fd_info);	// Do not close the FD, that
	case CMD_MUTE:
	case CMD_PAD:
	    // Nothing needed
	    break;
	default:
	    std::cerr << "ERROR: Bad command " << std::endl;
	    break;
    }
    close (accept_fd);
    return (fd_info);
}
/*
 * Start a new h264 stream to the given FD
 */
static void start_h264_stream(const int fd)
{
    // ## MAKE REAL ###
    static const char data[] = "H264 goes here\n";

    write(fd, data, sizeof(data));
    close(fd);
}

/*
 * handle_socket -- Create and handle incoming socket requests
 */
static void *
handle_socket (void *data)
{
  ServerElements *server_elements = (ServerElements *) data;

  while (true) {
    int socket_fd = -1;
    // Open the socket, and keep trying until we do
    while (socket_fd < 0) {
      socket_fd = init_socket ();
      if (socket_fd < 0) {
        std::cerr << "ERROR: Could not open socket" << std::endl;
        sleep (10);
      }
    }
    while (true) {
      struct fd_info fd_info;   // Information about what we are sending
      LinearSinkInfo linear_sink_info;
      int tries = 0;
      fd_info = get_fd_and_info (socket_fd, server_elements);

      if (fd_info.cmd == CMD_BAD)
        break;                  // Go to outer loop and try again

      // ### Use conditional
      master_lock.lock (__FILE__, __LINE__);
      switch (fd_info.cmd) {
        case CMD_MJPEG:

          linear_sink_info.fd = fd_info.data.cmd_mjpeg.fd;
          linear_sink_info.fps = fd_info.data.cmd_mjpeg.fps;
          linear_sink_info.width = fd_info.data.cmd_mjpeg.width;
          linear_sink_info.height = fd_info.data.cmd_mjpeg.height;
          linear_sink_info.padding = fd_info.data.cmd_mjpeg.padding;

          /*
           * What to do with request if server still not at state 5
           * At this moment ignoring
           * 
           * If server at state 5, we can continue with petition
           * TO DO: create a queue or similar?
           */
           tries = 0;
           while (server_elements->state != 5)
           {
             vdbg ("handle_socket: Waiting for server to get into state 5");
             if(tries > 9)
               break;
               
             sleep(1);
           }
           
          if (server_elements->state != 5) {
            g_printerr ("\nServer state != 5; client will timeout\n");
          } else {
            server_elements->state = 6;
            build_pipeline (server_elements, &linear_sink_info);
          }

          break;
	case CMD_H264:
	  start_h264_stream(fd_info.data.cmd_h264.fd);
	  break;
	case CMD_PAD:
	  for (int i = 0; i < MAX_AMOUNT_OF_CLIENTS; i++) {
	      if (server_elements->sink_info[i].in_use) {
		  struct _Gstlinearmjpegaudio* gst_element = 
		      reinterpret_cast<struct _Gstlinearmjpegaudio* const>(server_elements->linearmjpegaudio[i]);
		  gst_element->add_pad(gst_element, &fd_info.data.cmd_pad);
	      }
	  }
	  break;
	case CMD_SNAPSHOT:
	    // All other threads to work while we get the data
	    master_lock.unlock (__FILE__, __LINE__);
	    //## TODO: For performance reasons, for this off
	    do_snap(fd_info.data.cmd_mjpeg.fd);
	    close(fd_info.data.cmd_mjpeg.fd);
	    master_lock.lock (__FILE__, __LINE__);
	    break;
	case CMD_MUTE:
	    do_mute(fd_info.data.cmd_mute);
	    break;
        default:
          std::cout << "WARNING: Unimplemented command " <<
              static_cast < int >(fd_info.cmd) << std::endl;
          break;
      }
      master_lock.unlock (__FILE__, __LINE__);
    }
  }
  return(NULL);
}


/* 
 * Signal handler for ctrl+c 
 * Needed for closing pipeline and i2c camera elements without problems
 * i2c fails and reboot is needed if not setting v4l2src state different to PLAY
 * when closing application
 * */
void
interrupt_handler (int dummy)
{
  if (first_try) {
    first_try = false;
    g_print ("\nMaster pipeline into NULL state\n");
    gst_element_set_state (p_server_elements->master_pipeline, GST_STATE_NULL);

    // wait until it's up and running or failed
    if (gst_element_get_state (p_server_elements->master_pipeline, NULL, NULL,
            -1) == GST_STATE_CHANGE_FAILURE) {
      g_error ("Failed to set mster_pipeline into NULL state");
    }
    exit (0);
  } else {
    g_print ("\nForcing exit...\n");
    exit (0);
  }
}

// Information passed to the CB function
struct cb_info {
    GSettings* jpeg_settings;		// Settings we are using for jpeg
    GSettings* motion_settings;		// Settings for motion
    ServerElements* server_elements;	// The elements we are going to set things in
};

/********************************************************
 * MOTION configuraiton items (as given to us by the	*
 * user)						*
 *							*
 * md-mode (0,1) -- Is motion detection enabled.	*
 * md-threshold1 - md-threshold1 --			*
 * 	Motion detection threshold (0-255 default 127)	*
 * md-sensitivity1 - md-sensitivity4			*
 * 	Motion detection sensitiviuty (0-10 default 6)	*
 * md-window1 - md-window4 				*
 * 	Motion detection window based on a 640x480 	*
 * 	window.						*
 ********************************************************/


// All the information about motion settings 
// (in element units, not configuration units)
struct motion_info {
    bool enabled;	// Are we enabled
    struct {
	bool enabled;	// Is this window enabled
	int left;	// Left size
	int right;	// Right size
	int top;	// Top edge
	int bottom;	// Bottom edge
	int threshold;	// Threshold value
	int sensitivity;// Sensitivity
    } motion[N_MOTION];
};

// The currently set motion parameters
struct motion_info current_motion;

/*
 * motion_equal -- Return true if two motion info structures are equal.
 */
static bool motion_equal(const motion_info& i1, const motion_info& i2)
{
    if (i1.enabled != i2.enabled) return (false);
    for (unsigned int i = 0; i < N_MOTION; ++i) {
	if (i1.motion[i].enabled != i2.motion[i].enabled) return false;
	if (i1.motion[i].left != i2.motion[i].left) return false;
	if (i1.motion[i].right != i2.motion[i].right) return false;
	if (i1.motion[i].top != i2.motion[i].top) return false;
	if (i1.motion[i].bottom != i2.motion[i].bottom) return false;
	if (i1.motion[i].threshold != i2.motion[i].threshold) return false;
	if (i1.motion[i].sensitivity != i2.motion[i].sensitivity) return false;
    }
    return (true);
}
/*
 * read_motion -- Read the motion settings
 *
 * Parameters
 * 	info -- Place to put the results
 * 	settings -- Place to get the data from
 */
static void read_motion(struct motion_info& info)
{
    // Open up new motion setting
    GSettings* settings = g_settings_new("camera.MOTION");

    info.enabled = g_settings_get_int(settings, "md-mode");
    for (unsigned int i = 0; i < N_MOTION; ++i) {
	info.motion[i].enabled = g_settings_get_int(settings, 
		(std::string("md-switch") + static_cast<char>(i + '1')).c_str());

	// The name of the window we are reading
	std::string window_name = std::string("md-window") + static_cast<char>(i + '1');
	// The window we are looking at
	gchar* window = g_settings_get_string(settings, window_name.c_str());

	if (window == NULL) {
	    info.motion[i].enabled = false;
	}  else {
	    // The pieces of the motion window
	    gchar** vector = g_strsplit(window, ",", 4);

	    /*
	     * WARNING: The motion detection system has funny ideas about
	     * what's top and bottom.    top < bottom.
	     */
	    info.motion[i].left = atoi(vector[0]);
	    info.motion[i].top = atoi(vector[1]);
	    info.motion[i].right = atoi(vector[2]);
	    info.motion[i].bottom = atoi(vector[3]);

	    // Limit check our items
	    if (info.motion[i].left >= info.motion[i].right)
		info.motion[i].enabled = false;

	    if (info.motion[i].top >= info.motion[i].bottom)
		info.motion[i].enabled = false;

	    g_strfreev(vector);
	}
	info.motion[i].threshold = g_settings_get_int(settings, 
		(std::string("md-threshold") + static_cast<char>(i + '1')).c_str());
	info.motion[i].sensitivity = g_settings_get_int(settings, 
		(std::string("md-sensitivity") + static_cast<char>(i + '1')).c_str());
    }
    g_object_unref(settings);
}

static int max_threshold = 20;	// The threshold maximum for bg threshold
static int pix_adjust = 10;	// Adjustment for pix to motion
/*
 * set_motion_parameters -- Take current motion settings and translate them into
 * 	the gstreamer elements
 *
 * Function used by the callback, to configure Motion Detection elements
 * 
 * Parameters
 * 	server_elements -- The server elements to set things in
 */
static void set_motion_parameters(ServerElements* server_elements)
{
    if (! current_motion.enabled)	//State of all MD elements
    {
	current_motion_pad.motion_enabled = 0;
	for (unsigned int i = 0; i < N_MOTION; ++i)
	{
	    g_object_set (server_elements->motion_detect[i], "enable", false, NULL);
	}
	return;
    }
    current_motion_pad.motion_enabled = 1;
    for (unsigned int i = 0; i < N_MOTION; ++i)
    {
	if (current_motion.motion[i].enabled)
	{
	    current_motion_pad.enabled[i] = 1;
	    g_object_set (server_elements->motion_detect[i], "enable", true, NULL);
	}else {
	    current_motion_pad.enabled[i] = 0;
	    g_object_set (server_elements->motion_detect[i], "enable", false, NULL);
	    continue;
	}

	static const unsigned int MOTION_CONFIG_WIDTH=640;	// Width of the conf value of the motion detectection window
	static const unsigned int MOTION_CONFIG_HEIGHT=480;	// Height of the conf value of the motion detectection window

	current_motion_pad.motion_threshold[i] = current_motion.motion[i].threshold;

	// Configuration values that define this motion detection window (in units for the motion element)
	unsigned int left =   (current_motion.motion[i].left   * MOTION_WIDTH ) / MOTION_CONFIG_WIDTH;
	unsigned int right =  (current_motion.motion[i].right  * MOTION_WIDTH ) / MOTION_CONFIG_WIDTH;
	unsigned int top =    (current_motion.motion[i].top    * MOTION_HEIGHT) / MOTION_CONFIG_HEIGHT;
	unsigned int bottom = (current_motion.motion[i].bottom * MOTION_HEIGHT) / MOTION_CONFIG_HEIGHT;
	unsigned int fg_thresh = (current_motion.motion[i].sensitivity * max_threshold) / 10;
	unsigned int pix_to_motion = (current_motion.motion[i].threshold * (MOTION_WIDTH*MOTION_HEIGHT)) / 255;

        pix_to_motion /= pix_adjust;;

	std::cout << "DEBUG: sensistivity " << current_motion.motion[i].sensitivity << std::endl;
	std::cout << "DEBUG: pixels " << pix_to_motion << " total " << (MOTION_WIDTH*MOTION_HEIGHT) << " fract " <<
	   static_cast<double>(pix_to_motion) / static_cast<double>(MOTION_WIDTH*MOTION_HEIGHT) * 100.0 << std::endl;


	g_object_set (server_elements->motion_detect[i],
		"left", left,
		"right", right,
		"top",   top,
		"bottom", bottom,
		"fg_thresh", fg_thresh,
		"pix_to_motion", pix_to_motion,
		NULL);
	syslog(LOG_INFO, "Motion set for %d (%d,%d,%d,%d) fg_thresh %d pix_to_motion %d",
		i, left, top, right, bottom, fg_thresh, pix_to_motion);
	std::cout << "DEBUG: motion " << i << 
	    " left "      << left << 
	    " right "     << right << 
	    " top "       << top << 
	    " bottom "    << bottom << 
	    " fg_thresh " <<  fg_thresh << 
	    " pix_to_motion " <<  pix_to_motion <<  std::endl;
    }
}

/*
 * change_motion -- Callback executed when the motion detection changes
 */
static void change_motion(GAction* ptr, GParamSpec* pspec, gpointer user_data)
{
    // Get the information on the settings and the server
    struct cb_info* cb_info = (struct cb_info*)user_data;

    struct motion_info new_motion;	// New motion settings
    read_motion(new_motion);
    if (! motion_equal(new_motion, current_motion)) {
	current_motion = new_motion;
	set_motion_parameters(cb_info->server_elements);
    }
}
/*
 * change_qualty -- Callback executed when the quality level changes
 */
static void change_quality(GAction* ptr, GParamSpec* pspec, gpointer user_data)
{
    // We can't change the value on the fly -- so dump the streams and start over
    // ### TODO: Restart on the fly
    exit(0);
#if 0
    // Get the information on the settings and the server
    struct cb_info* cb_info = (struct cb_info*)user_data;
    // Get the quality level
    gint qValue = g_settings_get_int(cb_info->settings, "quality-level");

    // The encoder which we want to set the quality for
    GstElement* jpeg_encoder =
	gst_bin_get_by_name (GST_BIN (cb_info->server_elements->jpeg_pipeline),
		"jpegenc");
    if (jpeg_encoder == NULL) {
	g_printerr("Could not find element named 'jpegenc' for quality setting\n");
	return;
    }
    // Limit the value to something sane
    if (qValue < 1)
	qValue = 1;
    if (qValue > 97)
	qValue = 97;

    g_object_set (jpeg_encoder, "qValue", qValue, NULL);
#endif
}

/*
 * handle_config -- Initialize the configuration settings callbacks.
 *
 * Parameters
 * 	server_elements -- The information about the elements in the server
 */
static void handle_config(ServerElements* server_elements)
{
    // Information on the CB we are using
    static struct cb_info cb_info;

    cb_info.server_elements = server_elements;
    // Get the settings that we want to watch
    cb_info.jpeg_settings = g_settings_new("camera.JPEG");
    // Generate a callback for the quality setting
    GAction* action = g_settings_create_action(cb_info.jpeg_settings, "quality-level");
    // Add the callback
    g_signal_connect(action, "notify::state", G_CALLBACK(change_quality), &cb_info);

    // Get the motion settings
    cb_info.motion_settings = g_settings_new("camera.MOTION");

    // Generate a callback for the sequence setting (which always occurs last)
    GAction* motion_action = g_settings_create_action(cb_info.motion_settings, "sequence");

    // Add the callback
    g_signal_connect(motion_action, "notify::state", G_CALLBACK(change_motion), &cb_info);

    GSettings* tune = g_settings_new("camera.TUNE");
    max_threshold = g_settings_get_int(tune, "max-threshold");
    pix_adjust = g_settings_get_int(tune, "pix-adjust");
    g_object_unref(tune);

    read_motion(current_motion);
}    


int
main (int argc, char *argv[])
{
  std::cout << "Linear Server " << __DATE__ << " " << __TIME__ << std::endl;
  openlog(argv[0], 0, LOG_USER);
  syslog(LOG_NOTICE, "Linear Server %s %s", __DATE__, __TIME__);
  ServerElements server_elements;
  pthread_t run_thread;

  GstBus *bus;
  GOptionContext *context;
  GOptionGroup *gstreamer_group;
  GError *error = NULL;

  //Initial values
  server_elements.jpeg_pipeline_created = false;
  server_elements.pending_request = false;
  server_elements.state = 0;
  server_elements.sink_amount = 0;
  server_elements.jpeg_pipeline = NULL;
#ifdef QQQ
  server_elements.h264_pipeline = NULL;
#endif
  
  //RTSP h264 default values
  //According to documentation these values are always wired
  server_elements.rtsp_fps = RTSP_DEFAULT_FPS;
  server_elements.rtsp_height = RTSP_DEFAULT_HEIGHT;
  server_elements.rtsp_width = RTSP_DEFAULT_WIDTH;
  
  //Default MD configuration
  current_motion.enabled = false;

  //Init struct
  memset (&server_elements.sink_info, '\0', sizeof server_elements.sink_info);

  //Used for Ctrl+c handler
  p_server_elements = &server_elements;

  /* process application specific command line parameters along with GStreamer parameters */

  context = g_option_context_new ("linear server");

  gstreamer_group = gst_init_get_option_group ();
  g_option_context_add_group (context, gstreamer_group);

  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr
        ("Usage: %s [-w <v4l2src width> -h <v4l2src height> -d <run as daemon when set> -t <use videotestsrc when set> -f <frames per second>]\n",
        argv[0]);
    g_printerr
        ("VIDEO_PATTERN defines which test pattern to use\n");
    g_printerr ("ERROR: option parsing failed: %s\n", error->message);
    exit (1);
  }
  //Initialize values for buffer drop control
  server_elements.video_src_bin_src_pad_buffer_pass = false;
  server_elements.video_src_tee_to_jpeg_src_pad_buffer_pass = false;
  server_elements.audio_src_bin_src_pad_buffer_pass = false;
  server_elements.audio_src_tee_to_linear_pad_buffer_pass = false;
  server_elements.jpeg_pipeline_src_pad_buffer_pass = false;
  server_elements.video_src_tee_src_pad_for_h264_pipeline_buffer_pass = false;
  server_elements.h264_pipeline_src_pad_buffer_pass = false;

  //Add handler for ctrl+c
  signal (SIGINT, interrupt_handler);

  /* Initialize GStreamer */
  gst_init (&argc, &argv);
  server_elements.loop = g_main_loop_new (NULL, FALSE);

  //Video test or v4l2src
  if (cmd_videotest) {
    dbg ("Test source");
  } else {
    dbg ("v4l2src");
  }

  signal (SIGPIPE, SIG_IGN);

  handle_config(&server_elements);

  //Create master_pipeline
  server_elements.master_pipeline = gst_pipeline_new ("master_pipeline");

  if (server_elements.master_pipeline == NULL) {
    std::cerr << "Could not create master pipeline" << std::endl;
    abort ();
  }
  //state 0 until this point
  build_pipeline (&server_elements, NULL);

  // Create a thread for handle_socket function
  vdbg ("Creating thread for handle_socket(...)");
  if (pthread_create (&run_thread, NULL, handle_socket,
          (void *) &server_elements) != 0) {
    std::cerr << "ERROR: Unable to create run_pipeline thread" << std::endl;
    exit (99);
  }
  //Add bus callback gstreamer way
  vdbg ("Adding bus watch for gstreamer pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (server_elements.master_pipeline));
  gst_bus_add_watch (bus, bus_call, &server_elements);
  gst_object_unref (bus);

  g_print ("Running g_main_loop...");
  g_main_loop_run (server_elements.loop);

  exit (0);
}


#endif
