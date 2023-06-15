#include<iostream>
#include <string.h>
#include <random>

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif
#include<dirent.h>
#include <vector>
#include<gst/tag/tag.h>
#include <filesystem> 
#include <sstream>
#include <gst/video/video.h>
#include <gst/video/colorbalance.h>


#define FOLDER "/home/ee212930/Music/"


using namespace std;

// Global variables
std::vector< gchar*> audio_files;
std::vector<gchar*> video_files;
std::vector<string> visualizers ;
gchar *artist,*title,*album,*genre;
const gchar *mp3=".mp3";
const gchar *mp4=".mp4";
GstMessage *msg;
GstBus *bus;
guint flags;
string visualizer ;
GstTagList *tags = NULL;
GValue uri = G_VALUE_INIT;
const gchar* uri_str;
const gchar* uri_file;
gchar *filename;
gchar *ext;
static int current_song_index = 0;
static int id=0;

	


#define APP_NAME "JUKE-GENIE PLAYER"
#define APP_VERSION "1.0.0"


typedef enum {
  GST_PLAY_FLAG_VIS           = (1 << 3) /* Enable rendering of visualizations when there is no video stream. */
} GstPlayFlags;

vector<string> getAudioFiles(string path) {
    vector<string> files;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            string filename = ent->d_name;
            if (filename.size() >= 4 && filename.substr(filename.size()-4) == ".mp3") {
                files.push_back(filename);
            }
        }
        closedir(dir);
    } else {
        cout << "Error opening directory " << path << endl;
    }
    return files;
}

vector<string> getVisualizers() {
    // TODO: implement function to get available visualizers
	//vector<string> getVisualizers() {
	    vector<string> visualizers = {

	        "goom",
	        "monoscope",
	        "wavescope",
			"spectrascope",
	        "synaescope",
			"goom2k1",
			//"spacescope"
	    };
	    return visualizers;
	//}

}
 
/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData {
  GstElement *playbin,*vis_plugin=NULL;           /* Our one and only pipeline */

  GtkWidget *slider, *volume_slider;              /* Slider widget to keep track of current position */
  GtkWidget *streams_list; 
  GtkBox *filename_box;
  GtkBox *artist_box;
  GtkWidget *filename_label;
  GtkWidget *artist_label;       /* Text widget to display info about the streams */
  gulong slider_update_signal_id; /* Signal ID for the slider update signal */

  GtkWidget *end_duration;
  GtkWidget *curr_duration;
  GstState state;                 /* Current state of the pipeline */
  gint64 duration;                /* Duration of the clip, in nanoseconds */
  gboolean playing;
  gchar *current_file; 
 
   
} CustomData;

CustomData data;

/* This function is called when the GUI toolkit creates the physical window that will hold the video.
 * At this point we can retrieve its handler (which has a different meaning depending on the windowing system)
 * and pass it to GStreamer through the VideoOverlay interface. */
static void realize_cb (GtkWidget *widget, CustomData *data) {
  GdkWindow *window = gtk_widget_get_window (widget);
  guintptr window_handle;

  if (!gdk_window_ensure_native (window))
    g_error ("Couldn't create native window needed for GstVideoOverlay!");

  /* Retrieve window handler from GDK */
#if defined (GDK_WINDOWING_WIN32)
  window_handle = (guintptr)GDK_WINDOW_HWND (window);
#elif defined (GDK_WINDOWING_QUARTZ)
  window_handle = gdk_quartz_window_get_nsview (window);
#elif defined (GDK_WINDOWING_X11)
  window_handle = GDK_WINDOW_XID (window);
#endif
  /* Pass it to playbin, which implements VideoOverlay and will forward it to the video sink */
  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->playbin), window_handle);
}

/* This function is called periodically to refresh the GUI */
static gboolean refresh_ui (CustomData *data) {
  gint64 current = -1;
  data->duration=-1;

  /* We do not want to update anything unless we are in the PAUSED or PLAYING states */
  if (data->state < GST_STATE_PAUSED)
    return TRUE;

  /* If we didn't know it yet, query the stream duration */
  if (!GST_CLOCK_TIME_IS_VALID (data->duration)) {
    if (!gst_element_query_duration (data->playbin, GST_FORMAT_TIME, &data->duration)) {
      g_printerr ("Could not query current duration.\n");
    } else {
      /* Set the range of the slider to the clip duration, in SECONDS */
      gtk_range_set_range (GTK_RANGE (data->slider), 0, (gdouble)data->duration / GST_SECOND);
    }
    
    // Fetching the total duration of video and show in UI


    		gint dur,second,minute,hour;
            dur = data->duration / GST_SECOND;
			second=dur%60;
			minute=dur/60;
			hour=dur/(60*60);
		
        	string str = '0' + to_string(hour) + ":" + '0' + to_string(minute) + ":" + to_string(second);
    		gtk_label_set_label(GTK_LABEL(data->end_duration), str.c_str());
    
  }

  if (gst_element_query_position (data->playbin, GST_FORMAT_TIME, &current)) {
    /* Block the "value-changed" signal, so the slider_cb function is not called
     * (which would trigger a seek the user has not requested) */
    g_signal_handler_block (data->slider, data->slider_update_signal_id);
    /* Set the position of the slider to the current pipeline position, in SECONDS */
    gtk_range_set_value (GTK_RANGE (data->slider), (gdouble)current / GST_SECOND);
    /* Re-enable the signal */
    g_signal_handler_unblock (data->slider, data->slider_update_signal_id);
    
    // Fetching the current duration of video and show in UI

		    gint dur,second,minute,hour;
            dur = current / GST_SECOND;
			second=dur%60;
			minute=dur/60;
			hour=dur/(60*60);
        	string str = '0' + to_string(hour) + ":" + '0' + to_string(minute) + ":" + to_string(second);
    		gtk_label_set_label(GTK_LABEL(data->curr_duration), str.c_str());
  }
  
 
  
  //----filename------------
  
  g_object_get_property(G_OBJECT(data->playbin), "current-uri", &uri);

	// Extract the filename from the URI
	uri_str = g_value_get_string(&uri);
	filename = g_path_get_basename(uri_str);

	// Update the label on the video window
	
	 gchar *label_file = g_strdup_printf("%s", filename);
       gtk_label_set_text(GTK_LABEL(data->filename_label), label_file);
       g_free(label_file);
	//gtk_label_set_text(GTK_LABEL(data->filename_label), label_text);
	//g_free(label_text);
	//g_free((gchar*)filename);
	g_value_unset(&uri);
  
  
  /* Get the current playing song's metadata */
  if (GST_IS_TAG_LIST(tags)){
    if (gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist)) {
      /* Update the label on the video window */
      gchar *label_text = g_strdup_printf("%s", artist);
      gtk_label_set_text(GTK_LABEL(data->artist_label), label_text);
      g_free(label_text);
    }
    gst_tag_list_unref(tags);
    
   }
  
  
  return TRUE;
}

static void get_Filename(CustomData *data)
{
	g_object_get_property(G_OBJECT(data->playbin), "current-uri", &uri);

	// Extract the filename from the URI
	uri_str = g_value_get_string(&uri);
	filename = g_path_get_basename(uri_str);

	// Update the label on the video window
	gchar *label_text = g_strdup_printf("%s", filename);
	cout<<"title:"<<filename;
	cout<<"file:"<<filename<<endl;
	//gtk_label_set_text(GTK_LABEL(data->filename_label), label_text);
	//g_free(label_text);
	//g_free((gchar*)filename);
	g_value_unset(&uri);


}

static void get_artist(CustomData *data)
{
    artist = NULL;
    while(TRUE)
	{
    		
    		msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (data->playbin),GST_CLOCK_TIME_NONE,(GstMessageType)(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_TAG | GST_MESSAGE_ERROR));
    		gst_message_parse_tag(msg, &tags);

    		gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist);
    		
    		if(artist != NULL)
    		{
       		 break;
    		}
   		gst_tag_list_unref(tags);
   		 //gst_message_unref(msg);
	}
	
	//gst_element_set_state(playbin, GST_STATE_NULL);
	g_print("Artist : %s\n", artist);
	//return artist;
}

static void visualizer_cb(GtkButton *button,CustomData *data)
{

	random_device rd;
        mt19937 rng(rd());
        uniform_int_distribution<int> visDist(0, visualizers.size()-1);
        visualizer = visualizers[visDist(rng)];

        // Set the audio file as the source for playbin
        //g_object_set(pipeline, "uri", ("file://" + audioDir + "/" + audioFile).c_str(), NULL);

        /* Set the visualization flag */
            g_object_get (data->playbin, "flags", &flags, NULL);
            flags |= GST_PLAY_FLAG_VIS;
            g_object_set (data->playbin, "flags", flags, NULL);

        // Set the visualizer as the sink for playbin
        GstElement *vis_plugin = gst_element_factory_make(visualizer.c_str(), "visualization");
        if (visualizer == "spacescope" ) {
                    g_object_set(vis_plugin, "shade-amount", "rainbow","style",3, NULL);
                    g_object_set(vis_plugin, "shader", 9, NULL);
                } else if (visualizer == "goom" || visualizer == "goom2k1" ||visualizer == "Oscilloscope") {
                    g_object_set(vis_plugin, "shader",5, NULL);
                } else if (visualizer == "spectrascope") {
                    g_object_set(vis_plugin, "shade-amount", "Coolwarm","shader",3, NULL);
                }else if (visualizer == "synaescope") {
                    g_object_set(vis_plugin,"shader",9, NULL);
                }else if (visualizer == "wavescope") {
                    g_object_set(vis_plugin,"shader",4, "shade-amount","Viridis",NULL);
                }


        //g_object_set (vis_plugin, "style", "lines", NULL);
           g_object_set (data->playbin, "vis-plugin", vis_plugin, NULL);
}

static void play_pause(GtkButton *button,CustomData *data)
{
     GstState state;
     gst_element_get_state(data->playbin,&state, NULL, GST_CLOCK_TIME_NONE);
     if(state==GST_STATE_PLAYING)
     {
         gst_element_set_state(data->playbin,GST_STATE_PAUSED);
     }
     else
     {
     	 gst_element_set_state(data->playbin,GST_STATE_PLAYING);
     }
}

//This function is called when a key for play_pause is pressed 
static gboolean key_play_pause_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_space) // Also we can use ASCII value for 'space' 32
       {
       	play_pause(NULL, data); // call play_pause_cb with NULL button arguement
       	return TRUE;
       }
       return FALSE;
} 



/* This function is called when the STOP button is clicked */
static void stop_cb (GtkButton *button, CustomData *data) {
  gst_element_set_state (data->playbin, GST_STATE_READY);
  play_pause(NULL, data);
}

//This function is called when a key for stop is pressed 
static gboolean key_stop_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	 if (event->keyval == GDK_KEY_BackSpace) // Also we can use ASCII value for 'BackSpace' 65288.
       {
       	stop_cb(NULL, data); // call stop_cb with NULL button arguement
       	return TRUE;
       }
       return FALSE;
}

/* This function is called when the main window is closed */
static void delete_event_cb (GtkWidget *widget, GdkEvent *event, CustomData *data) {
  //stop_cb (NULL, data);
  gtk_main_quit ();
}

/* This function is called when the VOLUME UP button is clicked */
static void volume_up_cb (GtkButton *button, CustomData *data) {
  gdouble volume;
  g_object_get (data->playbin, "volume", &volume, NULL);
  volume += 0.1;
  if (volume > 1.0)
    volume = 1.0;
  g_object_set (data->playbin, "volume", volume, NULL);
}

//This function is called when a key for volume up is pressed 
static gboolean key_volume_up_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{
  	if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add) //Also we can use ASCII value for '+' 43
     	{
        	volume_up_cb(NULL, data); // Call volume_up_cb with NULL button argument
        	return TRUE;
     	}
        return FALSE;
}

/* This function is called when the VOLUME DOWN button is clicked */
static void volume_down_cb (GtkButton *button, CustomData *data) {
  gdouble volume;
  g_object_get (data->playbin, "volume", &volume, NULL);
  volume -= 0.1;
  if (volume < 0.0)
    volume = 0.0;
  g_object_set (data->playbin, "volume", volume, NULL);
}

//This function is called when a key for volume up is pressed 
static gboolean key_volume_down_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{
  	if(event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) //Also we can use ASCII value for '-' 45
       {
        	volume_down_cb(NULL, data); // Call volume down with NULL button argument
        	return TRUE;
       }
        return FALSE;
}



void play_previous_song(int* current_song_index, GstElement* playbin, CustomData *data) {
    g_print("song index: %s\n",audio_files[*current_song_index]);
    gst_element_set_state(playbin, GST_STATE_NULL);
    (*current_song_index)--;
    if (*current_song_index < 0) {
        *current_song_index = audio_files.size() - 1;
    }
    
    g_object_set(G_OBJECT(playbin), "uri", g_filename_to_uri(audio_files[*current_song_index], NULL, NULL), NULL);
    gst_element_set_state(playbin, GST_STATE_PLAYING);
    //current_song_index=0;
    get_artist(data);
    get_Filename(data);
    g_print("song index: %s\n",audio_files[*current_song_index]);

}

// Callback function for the "Previous" button
void play_previous_cb(GtkButton *button, CustomData *data,std::vector<gchar*>& audio_files)
{
    g_print("previous button clicked\n");
    play_previous_song(&current_song_index, data->playbin,data);
}


//This function is called when a key for previous song button is pressed 
static gboolean key_prev_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_less) 
       {
       	play_previous_song(&current_song_index, data->playbin,data); // call play_previous_song function
       	return TRUE;
       }
       return FALSE;
} 


void play_next_song(int* current_song_index, GstElement* playbin, CustomData *data) {
    gst_element_set_state(playbin, GST_STATE_NULL);
    (*current_song_index)++;
    if (*current_song_index >= audio_files.size()) {
        *current_song_index = 0;
    }
    g_object_set(G_OBJECT(playbin), "uri", g_filename_to_uri(audio_files[*current_song_index], NULL, NULL), NULL);
    gst_element_set_state(playbin, GST_STATE_PLAYING);
    get_artist(data);
    get_Filename(data);

	
}

static void play_next_cb(GtkButton *button, CustomData *data,std::vector<gchar*>& audio_files) {
     
     play_next_song(&current_song_index, data->playbin,data);
     
}


// This function is called when a key for nxt song button is pressed 
static gboolean key_next_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_greater) 
        {
        	play_next_song(&current_song_index, data->playbin,data); // call play_next_song function
       	return TRUE;
        }
        return FALSE;
}  

/* Callback function for the forward button */
static void forward_cb_5(GtkButton *button, CustomData *data) {
  gint64 current_position;
  gint64 new_position;

  /* Get the current position of the pipeline */
  gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current_position);

  /* Calculate the new position by adding 5 seconds */
  new_position = current_position + GST_SECOND * 5;

  /* Seek to the new position */
  gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, new_position);
}


// This function is called when a key for 5sec forward button is pressed 
static gboolean key_forward_5_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_Up) 
        {
       	forward_cb_5(NULL, data); // call forward_cb_5 function
       	return TRUE;
        }
        return FALSE;
} 

/* Callback function for the backward button */
static void backward_cb_5(GtkButton *button, CustomData *data) {
  gint64 current_position;
  gint64 new_position;

  /* Get the current position of the pipeline */
  gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current_position);

  /* Calculate the new position by subtracting 60 seconds */
  new_position = current_position - GST_SECOND * 5;

  /* Seek to the new position */
  gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, new_position);
}

// This function is called when a key for 5sec backward button is pressed 
static gboolean key_backward_5_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_Down) 
       {
       	backward_cb_5(NULL, data); // call backward_cb_5 function
       	return TRUE;
       }
       return FALSE;
}  


/* Callback function for the forward button */
static void forward_cb(GtkButton *button, CustomData *data) {
  gint64 current_position;
  gint64 new_position;
  gint64 total_duration;
  
  /*Get the total duration of the palybin*/
  gst_element_query_duration(data->playbin, GST_FORMAT_TIME, &total_duration);

  /* Get the current position of the playbin */
  gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current_position);

  /* Calculate the new position by adding 60 seconds */
  new_position = current_position + GST_SECOND * 60;
  
  if(new_position < total_duration) {
    /* Seek to the new position */
    gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, new_position);
  }
}


// This function is called when a key for 60sec forward button is pressed 
static gboolean key_forward_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_Right) 
       {
       	forward_cb(NULL, data); // call forward_cb function
       	return TRUE;
       }
       return FALSE;
}  


/* Callback function for the backward button */
static void backward_cb(GtkButton *button, CustomData *data) {
  gint64 current_position;
  gint64 new_position;

  /* Get the current position of the pipeline */
  gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current_position);

  /* Calculate the new position by subtracting 60 seconds */
  new_position = current_position - GST_SECOND * 60;

  /* Seek to the new position */
  gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, new_position);
}

// This function is called when a key for 60sec backward button is pressed 
static gboolean key_backward_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_Left) 
       {
       	backward_cb(NULL, data); // call backward_cb function
       	return TRUE;
       }
       return FALSE;
}  

/*callback function for the forward button for 10%*/
static void forward_cb_per(GtkButton *button, CustomData *data){
    gint64 total_duration;
    gint64 current_position;
    
    /*Get the total duration of the pipeline*/
    gst_element_query_duration(data->playbin, GST_FORMAT_TIME, &total_duration);
    
    /*Get the current position for the pipeline*/
    gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current_position);
    
    /*Calculate new position*/
    gint64 new_position = current_position + (total_duration / 10);
    
    /*seek to the new position*/
    gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, new_position);
}


// This function is called when a key for 10per forward button is pressed 
static gboolean key_forward_per_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_braceleft) 
        {
        	forward_cb_per(NULL, data); // call forward_cb_per function
       	return TRUE;
        }
        return FALSE;
}  

/*callback function for the backward button for 10%*/
static void backward_cb_per(GtkButton *button, CustomData *data){
    gint64 total_duration;
    gint64 current_position;
    
    /*Get the total duration of the pipeline*/
    gst_element_query_duration(data->playbin, GST_FORMAT_TIME, &total_duration);
    
    /*Get the current position for the pipeline*/
    gst_element_query_position(data->playbin, GST_FORMAT_TIME, &current_position);
    
    /*Calculate new position*/
    gint64 new_position = current_position - (total_duration / 10);
    
    /*seek to the new position*/
    gst_element_seek_simple(data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, new_position);
}


void play_index_song(int* current_song_index, GstElement* playbin, CustomData *data, std::vector<gchar*>& audio_files) {
    if (!playbin) {
        g_printerr("Error: playbin is NULL\n");
        return;
    }

    gst_element_set_state(playbin, GST_STATE_NULL);
    gst_element_set_state(playbin, GST_STATE_READY);
    if (*current_song_index < 0 || *current_song_index >= audio_files.size()) {
        g_printerr("Error: Invalid song index\n");
        return;
    }
	

    g_object_set(G_OBJECT(playbin), "uri", g_filename_to_uri(audio_files[*current_song_index], NULL, NULL), NULL);
    gst_element_set_state(playbin, GST_STATE_PLAYING);

    get_artist(data);
    get_Filename(data);
}

static void playlist_cb(GtkButton *button, CustomData *data) {
    cout << "playlist cb called" << endl;
    int file_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "file_index"));
    cout << "index = " << file_index << endl;
    
    g_print("Size of the list = %ld\n", audio_files.size());
    
    if (file_index < 0 || file_index >= audio_files.size()) {
        g_printerr("Error: Invalid song index\n");
        return;
    }

    g_print("song index: %s\n", audio_files[file_index]);
    g_print("song index: %d\n", file_index);
     
    play_index_song(&file_index, data->playbin, data, audio_files);
}


// This function is called when a key for 10per backward button is pressed 
gboolean add_cb(GtkButton *button, CustomData *data) 
{ 
  GtkWindow *parent_window = GTK_WINDOW(data);
  
  // Create a new vertical box to hold the buttons
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  
  //for (const auto& path : audio_files) 
  for(int i=0;i<audio_files.size();i++)
 
  {
    // Get the base name of the file
    gchar* path=audio_files[i];
    gchar* file = g_path_get_basename(path);
    std::string file_str(file);
    g_free(file);
    
    // Create a new button for the file
  GtkWidget *file_button = gtk_button_new_with_label(file_str.c_str());
  
  g_signal_connect(G_OBJECT(file_button), "clicked", G_CALLBACK(playlist_cb), data);
  // Set the index of the file as a data property of the button
  g_object_set_data(G_OBJECT(file_button), "file_index", GINT_TO_POINTER(i));
  
  // Add the button to the vertical box
  gtk_box_pack_start(GTK_BOX(vbox), file_button, FALSE, FALSE, 0);
  }
  
  
  // Create a new scrolled window to show the buttons
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(scrolled_window), vbox);
  
  // Set the size of the scrolled window
  gtk_widget_set_size_request(scrolled_window, 400, 300);
  
  
  // Create a new dialog to show the buttons
  GtkWidget *dialog = gtk_dialog_new_with_buttons("Playlist",
                                                  parent_window,
                                                  GTK_DIALOG_MODAL,
                                                  "_Close",
                                                  GTK_RESPONSE_CLOSE,
                                                  NULL);
  
  // Add the vertical box to the dialog
  GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);
  
  gtk_widget_show_all(dialog);
  
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  
 
  
  return FALSE;
  
 }
 
 
 gboolean keyboard(GtkWidget *widget, gpointer data) {
  GtkWindow *parent_window = GTK_WINDOW(data);
  GtkWidget *dialog = gtk_message_dialog_new(parent_window,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_QUESTION,
                                             GTK_BUTTONS_OK,
                                             "Keyboard-Shortcuts");
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                           "Space-Bar     Play/Pause \n"
                                           "Up-Arrow      Forward 5 Seconds \n"
                                           "Down-Arrow     Backward 5 Seconds \n"
                                           "Right-Arrow   Forward 60 Seconds \n"
                                           "Left-Arrow     Backward 60 Seconds \n"
                                           "\" < \"        Previous \n"
                                           "\" > \"        Next \n"
                                           "\" { \"        10 Percentage Forward \n"
                                           "\" } \"        10 Percentage Backward \n"
                                           " Enter         About \n");
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  return 0;
}

gboolean on_button_clicked(GtkWidget *widget, gpointer data) {
  GtkWindow *parent_window = GTK_WINDOW(data);
  GtkWidget *dialog = gtk_message_dialog_new(parent_window,
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_INFO,
                                             GTK_BUTTONS_OK,
                                             "App Name and Version");
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                           "%s - %s\n\n"
                                           "Developed by:\n"
                                           "Dharani S\n"
                                           "Geethanjali Patil\n"
                                           "Kavya Naik\n"
                                           "Megha L\n"
                                           "Ramyashree M\n"
                                           "Pooja Baadkar\n"
                                           "Adarsha N Y\n"
                                           "Saurabh Patel\n",
                                            APP_NAME, APP_VERSION);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  return 0;
}

static void mute_cb(GtkButton *button, CustomData *data)
{
    gboolean current_mute;
    g_object_get(data->playbin, "mute", &current_mute, nullptr);
    // Toggle the mute state
    gboolean new_mute = !current_mute;

    g_object_set(data->playbin, "mute", new_mute, nullptr);
   
	
}
static gboolean key_mute_cb(GtkWidget *widget, GdkEventKey *event, CustomData *data) 
{ 
	if (event->keyval == GDK_KEY_F1) 
       {
       	mute_cb(NULL, data); // call mute_cb function
       	return TRUE;
       }
       return FALSE;
}


/* This function is called when the slider changes its position. We perform a seek to the
 * new position here. */
static void slider_cb (GtkRange *range, CustomData *data) {
  gdouble value = gtk_range_get_value (GTK_RANGE (data->slider));
  gst_element_seek_simple (data->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
      (gint64)(value * GST_SECOND));
}

//setting volume 
void set_volume(GstElement *playbin, double volume) {
  if (playbin != NULL) {
    g_object_set (playbin, "volume", volume, NULL);
  }
}

//fetching the volume range
static void volume_slider_cb (GtkRange *range, CustomData *data) {
  gdouble value = gtk_range_get_value (GTK_RANGE (data->volume_slider));
  set_volume(data->playbin, value);
}

/* This function is called everytime the video window needs to be redrawn (due to damage/exposure,
 *rescaling, etc). GStreamer takes care of this in the PAUSED and PLAYING states, otherwise,
 * we simply draw a black rectangle to avoid garbage showing up. */
 gboolean draw_cb (GtkWidget *widget, cairo_t *cr, CustomData *data) {
  if (data->state < GST_STATE_PAUSED) {
    GtkAllocation allocation;

    /* Cairo is a 2D graphics library which we use here to clean the video window.
     * It is used by GStreamer for other reasons, so it will always be available to us. */
    gtk_widget_get_allocation (widget, &allocation);
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
    cairo_fill (cr);
  }

  return FALSE;
}


/* This creates all the GTK+ widgets that compose our application, and registers the callbacks */
static void create_ui (CustomData *data) {
  GtkWidget *main_window;  /* The uppermost window, containing all other windows */
  //GtkWidget *artist_box;
  //GtkWidget *vvbox;
  GtkWidget *video_window; /* The drawing area where the video will be shown */
  GtkWidget *main_box;     /* VBox to hold main_hbox and the controls */
  GtkWidget *main_hbox;    /* HBox to hold the video_window and the stream info text widget */
  GtkWidget *controls, *controls1;     /* HBox to hold the buttons and the slider */
  GtkWidget *lock,*mute_button,*visualizer_button,*playpause_button, *start,*end,*play_button, *pause_button, *stop_button,*button_volume_up, *button_volume_down,*next_button,*previous_button,*backward_button, *forward_button, *backward_button_5, *forward_button_5, *per_forward_button, *per_backward_button,*list_button; /* Buttons */

  GtkWidget *menubar;
  GtkWidget *fileMenu;

  GtkWidget *sep;
  GtkWidget *fileMi;
 
  GtkWidget *playlist;
  GtkWidget *playlistmi;
  GtkWidget *properties;
  GtkWidget *propertiesmi;
  GtkWidget *keyboardshortcut;
  GtkWidget *keyboardshortcutmi;
  GtkWidget *aboutmi;
  GtkWidget *about;
  
  GtkWidget *quitMi;
  
  
  main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (main_window), "delete-event", G_CALLBACK (delete_event_cb), data);

  video_window = gtk_drawing_area_new ();
  GdkScreen *screen = gdk_screen_get_default(); 
  GdkVisual *visual = gdk_screen_get_system_visual(screen);
  
  //gtk_widget_set_double_buffered (video_window, FALSE);


  g_signal_connect (video_window, "realize", G_CALLBACK (realize_cb), data);
  g_signal_connect (video_window, "draw", G_CALLBACK (draw_cb), data);
  
  
  //vvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

 /// gtk_container_add(GTK_CONTAINER(main_window), vvbox);

  
  //-------visualizer----------------------------------
  
  visualizer_button=gtk_button_new();
  
  GtkBox *visualizer_box=GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(visualizer_button), GTK_WIDGET(visualizer_box));
  
   GtkWidget *visualizer_icon = gtk_image_new_from_icon_name("audio-x-generic-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
   gtk_box_pack_start(visualizer_box, visualizer_icon, FALSE, FALSE, 0);

   GtkWidget *visualizer_label = gtk_label_new("Visualizer");
   gtk_box_pack_start(visualizer_box, visualizer_label, FALSE, FALSE, 0);
   
   g_signal_connect (G_OBJECT (visualizer_button), "clicked", G_CALLBACK (visualizer_cb), data);
  
  
  //play-pause-----------------------------------------
  
 
   playpause_button=gtk_button_new();
  
   GtkBox *playpause_box=GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
   gtk_container_add(GTK_CONTAINER(playpause_button), GTK_WIDGET(playpause_box));
  	
   GtkWidget *play_icon = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_SMALL_TOOLBAR);
   gtk_box_pack_start(playpause_box, play_icon, FALSE, FALSE, 0);

   GtkWidget *play_label = gtk_label_new("Play/Pause");
   gtk_box_pack_start(playpause_box, play_label, FALSE, FALSE, 0);
    
   g_signal_connect (G_OBJECT (playpause_button), "clicked", G_CALLBACK (play_pause), data);
   
   gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
   g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_play_pause_cb), data);
        
        
  //Restart----------------------------------------------------
  
  stop_button=gtk_button_new();
  
  GtkBox *stop_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(stop_button), GTK_WIDGET(stop_button_box));
  
  GtkWidget *stop_icon = gtk_image_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(stop_button_box, stop_icon, FALSE, FALSE, 0);

  GtkWidget *stop_label = gtk_label_new("Restart");
  gtk_box_pack_start(stop_button_box, stop_label, FALSE, FALSE, 0);

  //stop_button = gtk_button_new_from_icon_name ("media-playback-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (stop_button), "clicked", G_CALLBACK (stop_cb), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
   g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_stop_cb), data);
  
  //volume up ---------------------------------------------------
  
  button_volume_up=gtk_button_new();
  
  GtkBox *up_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(button_volume_up), GTK_WIDGET(up_button_box));
  
  GtkWidget *up_icon = gtk_image_new_from_icon_name("audio-volume-high", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(up_button_box, up_icon, FALSE, FALSE, 0);

  GtkWidget *up_label = gtk_label_new("+");
  gtk_box_pack_start(up_button_box, up_label, FALSE, FALSE, 0);
  
  //button_volume_up = gtk_button_new_from_icon_name ("audio-volume-high", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (button_volume_up), "clicked", G_CALLBACK (volume_up_cb), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_volume_up_cb), data);
  
  //volume down----------------------------------------------------
  
  button_volume_down=gtk_button_new();
  
  GtkBox *down_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(button_volume_down), GTK_WIDGET(down_button_box));
  
  GtkWidget *down_icon = gtk_image_new_from_icon_name("audio-volume-low", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(down_button_box, down_icon, FALSE, FALSE, 0);

  GtkWidget *down_label = gtk_label_new("-");
  gtk_box_pack_start(down_button_box, down_label, FALSE, FALSE, 0);

  //button_volume_down = gtk_button_new_from_icon_name ("audio-volume-low", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (button_volume_down), "clicked", G_CALLBACK (volume_down_cb), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_volume_down_cb), data);
  
  //-----------------------------------------------------------------
  
  //gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  //g_signal_connect(G_OBJECT(main_window), "key-volume-event", G_CALLBACK(key_volume_cb), data);
  
  // Create the volume slider
data->volume_slider = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 10, 0.5);
//gtk_orientable_set_orientation(GTK_ORIENTABLE(data->volume_slider), GTK_ORIENTATION_VERTICAL); // Set the orientation
gtk_range_set_inverted(GTK_RANGE(data->volume_slider), TRUE);  // Invert the direction
gtk_widget_set_size_request(data->volume_slider, 20, -1);

// Connect the "value-changed" signal of the slider to a callback function
g_signal_connect(G_OBJECT(data->volume_slider), "value-changed", G_CALLBACK(volume_slider_cb), data);

// Set the padding and style of the volume_slider widget
GtkStyleContext *context = gtk_widget_get_style_context(data->volume_slider);
gtk_style_context_add_class(context, "volume-slider");
gtk_style_context_add_class(context, "scale-horizontal");


// Create the volume icon image
GtkImage *icon = GTK_IMAGE(gtk_image_new_from_icon_name("audio-volume-high-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR));



// Connect the "value-changed" signal of the slider to a callback function
g_signal_connect(G_OBJECT(data->volume_slider), "value-changed", G_CALLBACK(volume_slider_cb), data); 
  //..................................................................................................
  //backward 60 -------------------------------------------------------------------------
  
  backward_button=gtk_button_new();
  
  GtkBox *backward_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(backward_button), GTK_WIDGET(backward_button_box));
  
  GtkWidget *backward_icon = gtk_image_new_from_icon_name("media-seek-backward-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(backward_button_box, backward_icon, FALSE, FALSE, 0);

  GtkWidget *backward_label = gtk_label_new("60-");
  gtk_box_pack_start(backward_button_box, backward_label, FALSE, FALSE, 0);
  
  //backward_button = gtk_button_new_from_icon_name ("backward", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (backward_button), "clicked", G_CALLBACK (backward_cb), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_backward_cb), data);
  
  
  //forward 60 --------------------------------------------------------------------------
  
  forward_button=gtk_button_new();
  
  GtkBox *forward_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(forward_button), GTK_WIDGET(forward_button_box));
  
  GtkWidget *forward_label = gtk_label_new("60+");
  gtk_box_pack_start(forward_button_box, forward_label, FALSE, FALSE, 0);
  
  GtkWidget *forward_icon = gtk_image_new_from_icon_name("media-seek-forward-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(forward_button_box, forward_icon, FALSE, FALSE, 0);
  
  //forward_button = gtk_button_new_from_icon_name ("forward", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (forward_button), "clicked", G_CALLBACK (forward_cb), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_forward_cb), data);
  
  //backward 5--------------------------------------------------------------------------
  
  backward_button_5=gtk_button_new();
  
  GtkBox *backward5_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(backward_button_5), GTK_WIDGET(backward5_button_box));
  
  GtkWidget *backward5_icon = gtk_image_new_from_icon_name("pan-start-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(backward5_button_box, backward5_icon, FALSE, FALSE, 0);

  GtkWidget *backward5_label = gtk_label_new("5-");
  gtk_box_pack_start(backward5_button_box, backward5_label, FALSE, FALSE, 0);
  
  //backward_button_5 = gtk_button_new_from_icon_name ("backward_5", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (backward_button_5), "clicked", G_CALLBACK (backward_cb_5), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_backward_5_cb), data);
  
  //forward 5-----------------------------------------------------------------------------
  
  forward_button_5=gtk_button_new();
  
  GtkBox *forward5_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(forward_button_5), GTK_WIDGET(forward5_button_box));
  
  GtkWidget *forward5_label = gtk_label_new("5+");
  gtk_box_pack_start(forward5_button_box, forward5_label, FALSE, FALSE, 0);
  
  GtkWidget *forward5_icon = gtk_image_new_from_icon_name("pan-end-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(forward5_button_box, forward5_icon, FALSE, FALSE, 0);

  //forward_button_5 = gtk_button_new_from_icon_name ("forward_5", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (forward_button_5), "clicked", G_CALLBACK (forward_cb_5), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_forward_5_cb), data);
  
  
  //next 10%--------------------------------------------------------------------------
  
  per_forward_button=gtk_button_new();
  
  GtkBox *per_for_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(per_forward_button), GTK_WIDGET(per_for_button_box));
  
  GtkWidget *per_for_label = gtk_label_new("10% +");
  gtk_box_pack_start(per_for_button_box, per_for_label, FALSE, FALSE, 0);
  
  GtkWidget *per_for_icon = gtk_image_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(per_for_button_box, per_for_icon, FALSE, FALSE, 0);

  //per_forward_button = gtk_button_new_from_icon_name ("forward_10per", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (per_forward_button), "clicked", G_CALLBACK (forward_cb_per), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_forward_per_cb), data);
  
  //prev 10%------------------------------------------------------------------------------------
  
  per_backward_button=gtk_button_new();
  
  GtkBox *per_back_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(per_backward_button), GTK_WIDGET(per_back_button_box));
  
  GtkWidget *per_back_icon = gtk_image_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(per_back_button_box, per_back_icon, FALSE, FALSE, 0);

  GtkWidget *per_back_label = gtk_label_new("10% -");
  gtk_box_pack_start(per_back_button_box, per_back_label, FALSE, FALSE, 0);
  
  //per_backward_button = gtk_button_new_from_icon_name ("backward_10per", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect (G_OBJECT (per_backward_button), "clicked", G_CALLBACK (backward_cb_per), data);
  
 // gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  //g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_backward_per_cb), data);
  
  //previous song  ---------------------------------------------------------------------------
  
  previous_button = gtk_button_new();
  
  GtkBox *pre_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(previous_button), GTK_WIDGET(pre_button_box));
  
  GtkWidget *pre_icon = gtk_image_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(pre_button_box, pre_icon, FALSE, FALSE, 0);

  GtkWidget *pre_label = gtk_label_new("Previous");
  gtk_box_pack_start(pre_button_box, pre_label, FALSE, FALSE, 0);
  
  //previous_button = gtk_button_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect(G_OBJECT(previous_button), "clicked", G_CALLBACK(play_previous_cb), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_prev_cb), data);

  //next song --------------------------------------------------------------------------------
  
  next_button = gtk_button_new();
  
  GtkBox *next_button_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(next_button), GTK_WIDGET(next_button_box));

  GtkWidget *next_label = gtk_label_new("Next");
  gtk_box_pack_start(next_button_box, next_label, FALSE, FALSE, 0);
  
  GtkWidget *next_icon = gtk_image_new_from_icon_name("media-skip-forward", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(next_button_box, next_icon, FALSE, FALSE, 0);
  
  //next_button = gtk_button_new_from_icon_name("media-skip-forward", GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_signal_connect(G_OBJECT(next_button), "clicked", G_CALLBACK(play_next_cb), data);
  
  gtk_widget_add_events(main_window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(main_window), "key-press-event", G_CALLBACK(key_next_cb), data);
  
  //-Mute-------------------------------------------------------------------------------------------------------
  
  mute_button=gtk_button_new();
  
  GtkBox *mute_box=GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
  gtk_container_add(GTK_CONTAINER(mute_button), GTK_WIDGET(mute_box));
  
  GtkWidget *mute_icon = gtk_image_new_from_icon_name("audio-volume-muted", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(mute_box, mute_icon, FALSE, FALSE, 0);

 GtkWidget *mute_label = gtk_label_new("Mute");
gtk_box_pack_start(mute_box, mute_label, FALSE, FALSE, 0);
   
  g_signal_connect (G_OBJECT (mute_button), "clicked", G_CALLBACK (mute_cb), data);
  
  //----------------------------------------------------------------------------------------------------------
 
  // start----------------------------------------------------------------------------------------------------------------------
    
    start=gtk_button_new();
	GtkBox *start_box=GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
	gtk_container_add(GTK_CONTAINER(start), GTK_WIDGET(start_box));
	data->curr_duration = gtk_label_new("0:00:00");
	gtk_box_pack_start(start_box, data->curr_duration, FALSE, FALSE, 0);
  
  //slider-------------------------------------------------------------------------------------
  
  data->slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (data->slider), 0);
  data->slider_update_signal_id = g_signal_connect (G_OBJECT (data->slider), "value-changed", G_CALLBACK (slider_cb), data);
  
  
  // end----------------------------------------------------------------------------------------------------------------------
    
    end=gtk_button_new();
	GtkBox *end_box=GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
	gtk_container_add(GTK_CONTAINER(end), GTK_WIDGET(end_box));
	data->end_duration = gtk_label_new("end");
	gtk_box_pack_start(end_box, data->end_duration, FALSE, FALSE, 0);
	
  //------------------------------------------------------------------------------------------------------------

  GtkWidget *add = gtk_button_new();
  GtkWidget *add1 = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON);
  //GtkWidget *add_label = gtk_label_new("Playlist");
  gtk_button_set_image(GTK_BUTTON(add), add1);
  // Connect the button to the callback function
  g_signal_connect(add, "clicked", G_CALLBACK(add_cb), data);


//----------------------------------------------------------------------------------------------------

  data->streams_list = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (data->streams_list), FALSE);
  gtk_widget_set_can_focus(main_window, TRUE); // Set the window to be focusable
  gtk_widget_grab_focus(main_window); // Grab focus on the window to receive key events


  gtk_window_set_title(GTK_WINDOW(main_window), "Juke-box");
  
  controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  
  //--------------------------------------------------------------------------------------------------
  
  menubar = gtk_menu_bar_new();
  //GtkImage *icon2;
  
  //menubar = gtk_button_new();
  
  fileMenu = gtk_menu_new();
  fileMi = gtk_menu_item_new();
  
  GtkImage *icon2 = GTK_IMAGE(gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR));
  
  gtk_menu_item_set_label(GTK_MENU_ITEM(fileMi), "Menu");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileMi), fileMenu);

  playlist = gtk_menu_new();
  playlistmi = gtk_menu_item_new_with_label("Playlist");
  
  properties = gtk_menu_new();
  propertiesmi = gtk_menu_item_new_with_label("Properties");
  
  keyboardshortcut = gtk_menu_new();
  keyboardshortcutmi = gtk_menu_item_new_with_label("Keyboard Shortcut ");
  
  
  about= gtk_menu_new();
 aboutmi = gtk_menu_item_new_with_label("About");
  
  
  sep = gtk_separator_menu_item_new();  
  quitMi = gtk_menu_item_new_with_label("Quit");

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileMi), fileMenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), playlistmi);
      gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), keyboardshortcutmi);
        gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), aboutmi);
  gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), sep);
  gtk_menu_shell_append(GTK_MENU_SHELL(fileMenu), quitMi);
  
  // Add the "File" menu to the right side of the menu bar
  gtk_menu_shell_insert(GTK_MENU_SHELL(menubar), fileMi, 0);
  

  
  //---------------------------------------------
  
  
  //gtk_box_pack_start (GTK_BOX (controls), pause_button, FALSE, FALSE, 2);
  //gtk_box_pack_start (GTK_BOX (controls), stop_button, FALSE, FALSE, 2);
 // gtk_box_pack_start (GTK_BOX (controls), button_volume_up, FALSE, FALSE, 2);
 // gtk_box_pack_start (GTK_BOX (controls), button_volume_down, FALSE, FALSE, 2);
 gtk_box_pack_start (GTK_BOX (controls), per_backward_button, FALSE, FALSE, 2);
 gtk_box_pack_start (GTK_BOX (controls), backward_button_5, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), backward_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), previous_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), playpause_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), stop_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), next_button, FALSE, FALSE, 2);
  
  gtk_box_pack_start (GTK_BOX (controls), forward_button, FALSE, FALSE, 2);
  
  gtk_box_pack_start (GTK_BOX (controls), forward_button_5, FALSE, FALSE, 2);
  
  gtk_box_pack_start (GTK_BOX (controls), per_forward_button, FALSE, FALSE, 2);
 // gtk_box_pack_start (GTK_BOX (controls), lock_button, FALSE, FALSE, 2);
   //gtk_box_pack_start (GTK_BOX (controls), add, FALSE, FALSE, 2);
  
  
 // gtk_box_pack_start(GTK_BOX(controls), menubar, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(main_window), "destroy",G_CALLBACK(gtk_main_quit), NULL);

  g_signal_connect(G_OBJECT(quitMi), "activate",G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(aboutmi), "activate",G_CALLBACK(on_button_clicked), NULL);
  g_signal_connect(G_OBJECT(keyboardshortcutmi), "activate", G_CALLBACK(keyboard), NULL);
 
  g_signal_connect(G_OBJECT( playlistmi), "activate", G_CALLBACK(add_cb), data);
 
  
  controls1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (controls1), visualizer_button, FALSE, FALSE, 2);
   gtk_box_pack_start (GTK_BOX (controls1), start, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls1), data->slider, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (controls1), end, FALSE, FALSE, 2);
  
  
  GtkWidget *main_hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (main_hbox1), GTK_WIDGET(icon2), FALSE, FALSE, 2);
 gtk_box_pack_start(GTK_BOX(main_hbox1), menubar, FALSE, FALSE, 0);
 
  //volume_box--------------------------------------------------------
  GtkWidget *volume_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  
 gtk_box_pack_start (GTK_BOX (volume_box), main_hbox1, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (volume_box), GTK_WIDGET(data->volume_slider), TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (volume_box), GTK_WIDGET(icon), FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (volume_box),mute_button, FALSE, FALSE, 2);
  
  //metadata----------------------------------------------
  // Get the artist name from the get_artist function
  //gchar *artist_name = (gchar*) G_CALLBACK(get_artist);
  //cout<<"artist: "<<artist_name;
  // val=G_CALLBACK (get_artist);
  
  cout<<"in gui"<<filename;
  cout<<"in gui:: artist"<<artist;
 
  data->filename_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
  data->filename_label = gtk_label_new(filename);
  gtk_box_pack_start(data->filename_box, data->filename_label, FALSE, FALSE, 0);
  
  data->artist_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
  data->artist_label = gtk_label_new(artist);
  gtk_box_pack_start(data->artist_box, data->artist_label, FALSE, FALSE, 0);
   
  
  main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (main_hbox), video_window, TRUE, TRUE, 0);
  //gtk_box_pack_start (GTK_BOX (main_hbox), data->streams_list, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (main_hbox), volume_box, FALSE, FALSE, 0);

  main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL,0);
  gtk_box_pack_start(GTK_BOX(main_box), GTK_WIDGET(data->filename_box), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(main_box), GTK_WIDGET(data->artist_box), FALSE, FALSE, 0);
 
  
  //gtk_box_pack_start (GTK_BOX (main_box),artist_box,TRUE,FALSE,0);
  gtk_box_pack_start (GTK_BOX (main_box), main_hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (main_box), controls, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_box), controls1, FALSE, FALSE, 0);
  
 // gtk_container_add(GTK_CONTAINER(main_window),menubar);
  gtk_container_add (GTK_CONTAINER (main_window), main_box);
  
  //gtk_window_set_resizable(GTK_WINDOW(main_window), FALSE);
  //gtk_window_maximize(GTK_WINDOW(main_window));
  gtk_window_set_default_size (GTK_WINDOW (main_window), 640, 480);

  gtk_widget_show_all (main_window);
}



/* This function is called when new metadata is discovered in the stream */
static void tags_cb (GstElement *playbin, gint stream, CustomData *data) {
  /* We are possibly in a GStreamer working thread, so we notify the main
   * thread of this event through a message in the bus */
  gst_element_post_message (playbin,
    gst_message_new_application (GST_OBJECT (playbin),
      gst_structure_new_empty ("tags-changed")));
}

/* This function is called when an error message is posted on the bus */
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  gst_message_parse_error (msg, &err, &debug_info);
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
  g_clear_error (&err);
  g_free (debug_info);

  /* Set the pipeline to READY (which stops playback) */
  gst_element_set_state (data->playbin, GST_STATE_READY);
}

/* This function is called when an End-Of-Stream message is posted on the bus.
 * We just set the pipeline to READY (which stops playback) */
static void eos_cb (GstBus *bus, GstMessage *msg, CustomData *data,GstElement* playbin,std::vector<gchar*>& audio_files) {
  g_print ("End-Of-Stream reached.\n");
   play_next_song(&current_song_index, data->playbin,data);

}

/* This function is called when the pipeline changes states. We use it to
 * keep track of the current state. */
static void state_changed_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
  GstState old_state, new_state, pending_state;
  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
    data->state = new_state;
    g_print ("State set to %s\n", gst_element_state_get_name (new_state));
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      /* For extra responsiveness, we refresh the GUI as soon as we reach the PAUSED state */
      refresh_ui (data);
    }
  }
}




int main(int argc, char *argv[]) {

  DIR *dir;
  CustomData data;
  struct dirent *entry;
  
  GstStateChangeReturn ret;
  
  data.playing = FALSE;
    
  /* Initialize GTK */
  gtk_init (&argc, &argv);

  /* Initialize GStreamer */
  gst_init (&argc, &argv);
  
  /* Initialize our data structure */
  memset (&data, 0, sizeof (data));
  data.duration = GST_CLOCK_TIME_NONE;

  /* Create the elements */
  data.playbin = gst_element_factory_make ("playbin", "playbin");


  if (!data.playbin) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }
  
  dir = opendir(FOLDER);

  if (dir == NULL) 
  {
     g_printerr("Could not open images folder. Exiting.\n");
     return -1;
  }

  while((entry= readdir(dir))!=NULL)
  {
    if(entry->d_type== DT_REG && (strstr(entry->d_name,".mp3")!=NULL || strstr(entry->d_name,".mp4")!=NULL || strstr(entry->d_name,".webm")!=NULL || strstr(entry->d_name,".avi")!=NULL || strstr(entry->d_name,".flac")!=NULL || strstr(entry->d_name,".wma")!=NULL || strstr(entry->d_name,".ogg")!=NULL) )
    {
      gchar *full_path = g_strdup_printf("%s/%s",FOLDER,entry->d_name);
      audio_files.push_back(full_path);
    }
  }
  
  current_song_index = 0; // start with the first song in the list
  
  g_signal_connect (G_OBJECT (data.playbin), "video-tags-changed", (GCallback) tags_cb, &data);
  g_signal_connect (G_OBJECT (data.playbin), "audio-tags-changed", (GCallback) tags_cb, &data);
  g_signal_connect (G_OBJECT (data.playbin), "text-tags-changed", (GCallback) tags_cb, &data);

  bus = gst_element_get_bus (data.playbin);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, &data);
  g_signal_connect(G_OBJECT(bus), "message::eos", reinterpret_cast<GCallback>(eos_cb), &data);
  g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)state_changed_cb, &data);
 // g_signal_connect (G_OBJECT (bus), "message::application", (GCallback)application_cb, &data);
  
  //---visualizer--------------------
  
  // Get available visualizers
  visualizers = getVisualizers();
    
        // Select a random audio file and visualizer
        random_device rd;
        mt19937 rng(rd());
        uniform_int_distribution<int> visDist(0, visualizers.size()-1);
        visualizer = visualizers[visDist(rng)];

        /* Start playing the first song */
 	 g_object_set(G_OBJECT(data.playbin),"uri",g_filename_to_uri(audio_files[current_song_index],NULL,NULL),NULL);

       /* Set the visualization flag */
	g_object_get (data.playbin, "flags", &flags, NULL);
	flags |= GST_PLAY_FLAG_VIS;
	g_object_set (data.playbin, "flags", flags, NULL);

	/* set vis plugin for playbin */
	g_object_set (data.playbin, "vis-plugin", data.vis_plugin, NULL);
	
  	ret = gst_element_set_state (data.playbin, GST_STATE_PLAYING);
  	
  	//-------------filename----------------
  	
  	g_object_get_property(G_OBJECT(data.playbin), "current-uri", &uri);

	// Extract the filename from the URI
	uri_str = g_value_get_string(&uri);
	filename = g_path_get_basename(uri_str);

	// Update the label on the video window
	gchar *label_text = g_strdup_printf("%s", filename);
	cout<<"title:"<<filename;
	g_value_unset(&uri);
	
	
	
	//---------------artist name-------------------
  	
  	while(TRUE)
	{
    		msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (data.playbin),GST_CLOCK_TIME_NONE,(GstMessageType)(GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_TAG | GST_MESSAGE_ERROR));
    		gst_message_parse_tag(msg, &tags);
    		
    		gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist);
    		
    		if(artist != NULL)
    		{
       		 break;
    		}
   		gst_tag_list_unref(tags);
   		 //gst_message_unref(msg);
	}
	
	//gst_element_set_state(data.playbin, GST_STATE_NULL);
	g_print("Artist : %s\n", artist);
  	
  	if (ret == GST_STATE_CHANGE_FAILURE) {
   	g_printerr ("Unable to set the pipeline to the playing state.\n");
    	//gst_object_unref (data.playbin);
    	return -1; 
    	}
    	
    	 /* Create the GUI */
        create_ui (&data);
  	
  	/* Register a function that GLib will call every second */
  	g_timeout_add_seconds (1, (GSourceFunc)refresh_ui, &data);

  	/* Start the GTK main loop. We will not regain control until gtk_main_quit is called. */
  	gtk_main ();

  	/* Free resources */
  	gst_element_set_state (data.playbin, GST_STATE_NULL);
  	gst_object_unref (data.playbin);
  	gst_deinit();
  
  	return 0;

}






