

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
  GstElement *pipeline;
  GstMessage *msg;
  GMainLoop *loop;
  GstBus *bus;
  GError *error;

  guint bus_watch_id;

} PipeElements;
PipeElements pipe_elements;

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

void nice_close() {
  
  gst_element_set_state (pipe_elements.pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipe_elements.pipeline));
  g_source_remove (pipe_elements.bus_watch_id);
  g_main_loop_unref (pipe_elements.loop);
}

static void sig_handler(int signo) {
  switch (signo) {
    case SIGINT:
      g_print("\nCtrl+c received. Closing pipeline\n");
      nice_close();
      /* Out of the main loop, clean up nicely */
      exit(0);
    break;
            
    default:
      dbg("Unknown signal: %d", signo);
  }
}

int
main (int argc, char *argv[])
{
  PipeElements *pipe_elements_ptr;
  pipe_elements_ptr = &pipe_elements;
  pipe_elements_ptr->error = NULL;

  /* Register signal and signal handler */
  signal(SIGINT, sig_handler);

  gst_init (&argc, &argv);

  pipe_elements_ptr->loop = g_main_loop_new (NULL, FALSE);

  pipe_elements_ptr->pipeline = gst_parse_launch ("videotestsrc ! xvimagesink", &pipe_elements_ptr->error);
  if (!pipe_elements_ptr->pipeline) {
    g_print ("Parse error: %s\n", pipe_elements_ptr->error->message);
    exit (1);
  }

  pipe_elements_ptr->bus = gst_pipeline_get_bus (GST_PIPELINE (pipe_elements_ptr->pipeline));
  pipe_elements_ptr->bus_watch_id = gst_bus_add_watch (pipe_elements_ptr->bus, bus_call, pipe_elements_ptr->loop);
  gst_object_unref (pipe_elements_ptr->bus);
  
  gst_element_set_state (pipe_elements_ptr->pipeline, GST_STATE_PLAYING);

  /* Iterate */
  g_print ("Running...\n");
  g_main_loop_run (pipe_elements_ptr->loop);

  nice_close();
  
  return 0;
}
