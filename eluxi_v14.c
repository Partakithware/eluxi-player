#include <stdio.h>
#include <stdlib.h>
#include <mpv/client.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <stdint.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <dirent.h> // For directory operations

#define SCRIPT_DIR "/home/max/Documents/Eluxi/script_modules"

// Structure to hold our MPV and GTK+ data
typedef struct {
    mpv_handle *mpv;
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *play_icon;
    GtkWidget *play_button;
    GtkWidget *pause_button;
    GtkWidget *pause_icon;
    GtkWidget *old_image;
    GtkWidget *stop_icon;
    GtkWidget *stop_button;
    GtkWidget *file_icon;
    GtkWidget *file_button;
    GtkWidget *slider;
    GtkWidget *duration_label;
    gulong slider_handler_id;
    gboolean slider_dragging;
    GtkWidget *volume_slider;
    GtkWidget *volume_icon;
    GtkWidget *playlist_box;
    GtkWidget *playlist_button;
    GtkWidget *playlist_icon;
    GtkWidget *hover_box;
    GtkWidget *hbox;
    GtkWidget *slider_hbox;
    GtkWidget *fullscreen_button;
    GtkWidget *fullscreen_icon;
    GtkWidget *subtitle_button;
    GtkWidget *subtitle_icon;
    GtkWidget *current_playlist_item; // New: Store the currently playing button
    gboolean manual_selection;
    gboolean waiting_for_manual_load;
    GtkWidget *video_track_button; // New
    GtkWidget *video_track_icon;   // New
    GtkWidget *audio_track_button; // New
    GtkWidget *audio_track_icon;   // New
} AppData;

typedef struct {
    char **tracks;
    int count;
} SubtitleTracks;

typedef struct {
    char **tracks;
    int count;
} VideoTracks;

typedef struct {
    char **tracks;
    int count;
} AudioTracks;

typedef struct {
    VideoTracks tracks;
} VideoTrackCleanupData;

typedef struct {
    AudioTracks tracks;
} AudioTrackCleanupData;


// Function declarations (prototypes)
void load_mpv_script(mpv_handle *mpv, const char *script_path);
void load_lua_scripts(mpv_handle *mpv);


GList *video_queue = NULL;  // Queue of video filenames
GList *current_video = NULL; // Pointer to the current video in the queue
static GtkWidget *cached_vmenu = NULL;
static GtkWidget *cached_amenu = NULL;

// Global variables for cursor hiding
static GdkCursor *normal_cursor = NULL;
static GdkCursor *hidden_cursor = NULL;
static guint timeout_id = 0;
static GtkWidget *target_widget = NULL; // The widget to monitor for motion

// Function to hide the cursor
static void hide_cursor() {
    if (target_widget) {
        GdkWindow *window = gtk_widget_get_window(target_widget);
        if (window) {
            gdk_window_set_cursor(window, hidden_cursor);
        }
    }
}

// Function to reset the cursor visibility timer
static gboolean reset_cursor_timer(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    if (timeout_id != 0) {
        g_source_remove(timeout_id); // Cancel the existing timer
    }

    if (normal_cursor != NULL && target_widget) {
        GdkWindow *window = gtk_widget_get_window(target_widget);
        if (window) {
            gdk_window_set_cursor(window, normal_cursor); // Show the cursor
        }
    }

    timeout_id = g_timeout_add(8000, (GSourceFunc)hide_cursor, NULL); // Set the timer (8 seconds)
    return TRUE; // Continue processing the motion event
}

void load_mpv_script(mpv_handle *mpv, const char *script_path) {
    const char *cmd[] = {"load-script", script_path, NULL};
    printf("Attempting to load script: %s\n", script_path);

    int mpv_error = mpv_command(mpv, cmd);
    if (mpv_error < 0) {
        fprintf(stderr, "Failed to load Lua script: %s (MPV Error: %s)\n", script_path, mpv_error_string(mpv_error));
    } else {
        printf("Script loaded successfully: %s\n", script_path);
    }
}




void load_lua_scripts(mpv_handle *mpv) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(SCRIPT_DIR)) != NULL) {
        printf("Successfully opened script directory: %s\n", SCRIPT_DIR);
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, ".lua") != NULL) {
                char script_path[4096];
                snprintf(script_path, sizeof(script_path), "%s/%s", SCRIPT_DIR, ent->d_name);
                printf("Found Lua script: %s\n", script_path);
                load_mpv_script(mpv, script_path); // Pass full path now
            }
        }
        closedir(dir);
    } else {
        perror("Could not open script directory");
        printf("Error opening script directory: %s (Error: %s)\n", SCRIPT_DIR, strerror(errno));
    }
}


static gboolean print_pause_state(gpointer data) {
    printf("MPV: Pause state: %s\n", *(int *)data ? "yes" : "no");
    return FALSE;
}

static gboolean print_duration(gpointer data) {
    double duration = *(double *)data;
    printf("MPV: Duration: %f\n", duration);
    return FALSE;
}

static void update_volume_slider(AppData *app); // Declare the function

// Function to play the next file in the queue
static gboolean play_next_in_queue(AppData *app) {
    if (!app || !video_queue) {
        printf("Queue empty. Nothing to play.\n");
        return FALSE; // No more videos
    }

    if (!current_video) {
        printf("No current video selected.\n");
        return FALSE;
    }

    // Advance to the next video in the queue
    current_video = g_list_next(current_video);

    if (!current_video) {
        printf("End of queue.\n");
        // Optionally, you might want to:
        // - Stop playback
        mpv_command(app->mpv, (const char *[]){"stop", NULL});
        // - Clear the playlist highlighting
        if (app->current_playlist_item) {
            GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
            gtk_style_context_remove_class(context, "playing-video");
            app->current_playlist_item = NULL;
        }
        return FALSE; // End of queue
    }

    // Get the next file path
    char *next_file = (char *)current_video->data;
    printf("Now playing next file: %s\n", next_file);
    if (next_file != NULL){
    // Play the next file
    const char *cmd[] = {"loadfile", next_file, NULL};
    mpv_command(app->mpv, cmd);
    }

    // Update playlist highlighting
    if (app->current_playlist_item) {
        GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
        gtk_style_context_remove_class(context, "playing-video");
    }

    // Find the corresponding menu item and set its color
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->playlist_box));
    for (GList *child = children; child != NULL; child = child->next) {
        if (GTK_IS_MENU_ITEM(child->data)) {
            const char *item_label = gtk_menu_item_get_label(GTK_MENU_ITEM(child->data));
            if (item_label && strcmp(item_label, next_file) == 0) {
                app->current_playlist_item = GTK_WIDGET(child->data);
                GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
                gtk_style_context_add_class(context, "playing-video");
                break;
            }
        }
    }

    return FALSE; // Only run once
}

// Function to play the next file in the queue
static gboolean play_next_in_queue_false(AppData *app) {

    // Advance to the next video in the queue
    current_video = g_list_next(video_queue);

    if (!current_video) {
        printf("End of queue.\n");
        // Optionally, you might want to:
        // - Stop playback
        mpv_command(app->mpv, (const char *[]){"stop", NULL});
        // - Clear the playlist highlighting
        if (app->current_playlist_item) {
            GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
            gtk_style_context_remove_class(context, "playing-video");
            app->current_playlist_item = NULL;
        }
        return FALSE; // End of queue
    }

    // Get the next file path
    char *next_file = (char *)current_video->data;
    printf("Now playing next file: %s\n", next_file);
    if (next_file != NULL){
    // Play the next file
    const char *cmd[] = {"loadfile", next_file, NULL};
    mpv_command(app->mpv, cmd);
    if (g_list_length(video_queue) >= 3) {
        // There are at least three elements in the list
        printf("The list contains at least three elements.\n");
    } else {
        // There are fewer than three elements in the list
        printf("The list contains fewer than three elements.\n");
        video_queue = NULL;
        g_free(video_queue);
        g_list_free(video_queue);
    }

    }

     // Update playlist highlighting
     if (app->current_playlist_item) {
        GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
        gtk_style_context_remove_class(context, "playing-video");
    }

    // Find the corresponding menu item and set its color
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->playlist_box));
    for (GList *child = children; child != NULL; child = child->next) {
        if (GTK_IS_MENU_ITEM(child->data)) {
            const char *item_label = gtk_menu_item_get_label(GTK_MENU_ITEM(child->data));
            if (item_label && strcmp(item_label, next_file) == 0) {
                app->current_playlist_item = GTK_WIDGET(child->data);
                GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
                gtk_style_context_add_class(context, "playing-video");
                break;
            }
        }
    }

    

    return FALSE; // Only run once
}


void cleanup_cached_menus() {
    if (cached_vmenu) {
        gtk_widget_destroy(cached_vmenu);
        cached_vmenu = NULL;
    }
    if (cached_amenu) {
        gtk_widget_destroy(cached_amenu);
        cached_amenu = NULL;
    }
}

// Function to handle MPV events
static void handle_mpv_events(void *data) {
    AppData *app = (AppData *)data;
    mpv_handle *mpv = app->mpv;

    while (1) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event == NULL) {
            break;
        }

        switch (event->event_id) {
            case MPV_EVENT_NONE:
                break;
            case MPV_EVENT_SHUTDOWN:
                printf("MPV: Shutdown event received.\n");
                g_idle_add((GSourceFunc)gtk_main_quit, NULL);
                return;
                case MPV_EVENT_FILE_LOADED:
                if (app->waiting_for_manual_load) {
                    printf("MPV: Manual load completed.\n");
                    app->waiting_for_manual_load = FALSE;
                    app->manual_selection = FALSE;  // Clear manual mode after successful load
                }
                break;
            case MPV_EVENT_PLAYBACK_RESTART:
                printf("MPV: Playback started.\n");
                break;
            case MPV_EVENT_PROPERTY_CHANGE: {
                mpv_event_property *prop = (mpv_event_property *)event->data;
                if (strcmp(prop->name, "pause") == 0) {
                    if (prop->data) {
                        g_idle_add((GSourceFunc)print_pause_state, prop->data);
                    }
                } else if (strcmp(prop->name, "duration") == 0) {
                    if (prop->data) {
                        g_idle_add((GSourceFunc)print_duration, prop->data);
                    }
                } else if (strcmp(prop->name, "volume") == 0) {
                    // Update volume slider when volume changes
                    if (prop->data) {
                         g_idle_add((GSourceFunc)update_volume_slider, app);
                    }
                } 
                break;
            }
            case MPV_EVENT_END_FILE:
            printf("MPV: End of file.\n");
            if (app->manual_selection) {
                printf("MPV: Manual selection detected. Skipping auto-play.\n");
                app->manual_selection = FALSE;  // Reset it
            } else {
                g_idle_add((GSourceFunc)play_next_in_queue, app);
            }
            cleanup_cached_menus();
            break;
            default:
                printf("MPV: Unhandled event: %s\n", mpv_event_name(event->event_id));
        }
    }
}

// Correct helper function to find a property in an mpv_node_list
static mpv_node* mpv_node_list_find_property(mpv_node_list *list, const char *key) {
    if (!list || !key)
        return NULL;

    for (int i = 0; i < list->num; ++i) {
        if (list->keys && list->keys[i] && strcmp(list->keys[i], key) == 0) {
            return &list->values[i];
        }
    }

    return NULL;
}

// Function to load a file into MPV
static void load_file_in_mpv(mpv_handle *mpv, const char *filename) {
    if (mpv && filename) {
        const char *cmd[] = {"loadfile", filename, NULL};
        mpv_command(mpv, cmd);
    }
}

// Function to toggle pause (GTK callback)
static void on_play_pause_clicked(GtkWidget *button, AppData *app) {
    int pause = 0;
    if (!app->mpv) {
        fprintf(stderr, "MPV is not initialized.\n");
        return;
    }
    if (mpv_get_property(app->mpv, "pause", MPV_FORMAT_FLAG, &pause) >= 0) {
        pause = !pause;
        mpv_set_property(app->mpv, "pause", MPV_FORMAT_FLAG, &pause);
    } else {
        fprintf(stderr, "Failed to get pause property.\n");
    }

    // Update button label
    // Update button label
    if (pause) {
        // GtkWidget *old_image = gtk_button_get_image(GTK_BUTTON(app->play_button));
        // if (old_image) {
        //     gtk_widget_destroy(old_image);  // Free the old image
        // }
        GtkWidget *play_icon = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(app->play_button), play_icon);
    } else {
        // GtkWidget *old_image = gtk_button_get_image(GTK_BUTTON(app->play_button));
        // if (old_image) {
        //     gtk_widget_destroy(old_image);  // Free the old image
        // }
        GtkWidget *pause_icon = gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(app->play_button), pause_icon);
    }
}

// Function to stop playback (GTK callback)
static void on_stop_clicked(GtkWidget *button, AppData *app) {
    const char *cmd[] = {"stop", NULL};
    mpv_command(app->mpv, cmd);
}

/*static gboolean on_playlist_item_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    const char *filename = gtk_label_get_text(GTK_LABEL(widget));
    load_file_in_mpv(app->mpv, filename);
    return FALSE;
}*/
static void on_playlist_item_clicked(GtkWidget *button, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    const char *filename = gtk_button_get_label(GTK_BUTTON(button));
    app->manual_selection = TRUE;
    app->waiting_for_manual_load = TRUE;
    // Reset color of the previously playing item
    if (app->current_playlist_item) {
        GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
        gtk_style_context_remove_class(context, "playing-video");
    }

    // Find the corresponding GList element
    GList *l;
    for (l = video_queue; l != NULL; l = l->next) {
        if (strcmp((char *)l->data, filename) == 0) {
            current_video = l;
            break;
        }
    }

    app->current_playlist_item = button;
    GtkStyleContext *context = gtk_widget_get_style_context(button);
    gtk_style_context_add_class(context, "playing-video");

    load_file_in_mpv(app->mpv, filename);
}

static void on_playlist_menuitem_clicked(GtkWidget *menu_item, gpointer user_data) {
    AppData *app = (AppData *)user_data;
    const char *filename = gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item)); // Get label from MenuItem
    app->manual_selection = TRUE;
    app->waiting_for_manual_load = TRUE;

    // Reset color of the previously playing item
    if (app->current_playlist_item) {
        GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
        gtk_style_context_remove_class(context, "playing-video");
    }

    // Find the corresponding GList element
    GList *l;
    for (l = video_queue; l != NULL; l = l->next) {
        if (strcmp((char *)l->data, filename) == 0) {
            current_video = l;
            break;
        }
    }

    //Find the corresponding menu item and set its color
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->playlist_box));
    for (GList *child = children; child != NULL; child = child->next) {
        if (GTK_IS_MENU_ITEM(child->data)) {
            const char *item_label = gtk_menu_item_get_label(GTK_MENU_ITEM(child->data));
            if (item_label && strcmp(item_label, filename) == 0) {
                app->current_playlist_item = GTK_WIDGET(child->data);
                GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
                gtk_style_context_add_class(context, "playing-video");
                break;
            }
        }
    }

    load_file_in_mpv(app->mpv, filename);
}

// Helper function to toggle visibility
void toggle_playlist_visibility(GtkWidget *playlist_box) {
    gboolean is_visible = gtk_widget_get_visible(playlist_box);
    gtk_widget_set_visible(playlist_box, !is_visible);  // Toggle the visibility
}

void update_playlist(GtkWidget *playlist_box, GList *queue, AppData *app) {
    // IMPORTANT: Reset current_playlist_item before destroying buttons
    app->current_playlist_item = NULL;

    GList *children = gtk_container_get_children(GTK_CONTAINER(playlist_box));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }

    for (GList *l = queue; l != NULL; l = l->next) {
        GtkWidget *button = gtk_button_new_with_label((char *)l->data); // Create a button with the filename as label
        g_signal_connect_data(button, "clicked", // Connect "clicked" signal, not "button-press-event"
            G_CALLBACK(on_playlist_item_clicked),
            app, NULL, 0);
        gtk_box_pack_start(GTK_BOX(playlist_box), button, FALSE, FALSE, 5);
    }
    gtk_widget_show_all(playlist_box);
    toggle_playlist_visibility(playlist_box);
}

void update_playlist_menu(AppData *app, GList *queue) {
    // Destroy any existing menu items (if necessary - handle cases where you update)
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->playlist_box));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }

    // Repurpose playlist_box as the menu
    GtkWidget *menu = GTK_WIDGET(app->playlist_box); // Reuse the box as the menu

    for (GList *l = queue; l != NULL; l = l->next) {
        GtkWidget *menu_item = gtk_menu_item_new_with_label((char *)l->data);
        g_signal_connect(menu_item, "activate", G_CALLBACK(on_playlist_menuitem_clicked), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        gtk_widget_show(menu_item);
    }

    
}

static void on_playlist_button_clicked(GtkWidget *button, AppData *app_data) {
    // Reuse playlist_box as the menu
    GtkWidget *menu = GTK_WIDGET(app_data->playlist_box);

    //If the menu hasn't been created, create it
    if (!GTK_IS_MENU(menu)) {
        menu = gtk_menu_new();
        app_data->playlist_box = menu;
        update_playlist_menu(app_data, video_queue);
    }
    
    gtk_menu_popup_at_widget(GTK_MENU(menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}




static void on_file_open_clicked(GtkWidget *button, AppData *app) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new(
        "Open File", GTK_WINDOW(app->window), action,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    // Allow multiple file selection
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        // Clear previous video queue if there was one
        if (video_queue != NULL) {
            g_list_free(video_queue);
            video_queue = NULL;
            current_video = NULL;
        }

        GSList *filenames, *iter;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filenames = gtk_file_chooser_get_filenames(chooser);
        iter = filenames;

        // Create a "fake blank" video to force play_next_in_queue to run
        video_queue = g_list_append(video_queue, g_strdup("Current Playlist"));

        // Add the selected files to the video queue
        for (; iter != NULL; iter = iter->next) {
            char *filename = (char *)iter->data;

            // Add each selected file to the video_queue
            video_queue = g_list_append(video_queue, g_strdup(filename));
            
            g_free(filename);
             
        }

        g_slist_free(filenames);

        // Update the playlist UI to reflect the new video queue
        update_playlist(app->playlist_box, video_queue, app);
        update_playlist_menu(app, video_queue); // Update the menu

        

        // Add the play_next_in_queue function to the idle loop
        //g_idle_add((GSourceFunc)play_next_in_queue, app);
        current_video = video_queue->next;
        g_timeout_add(2000, (GSourceFunc)play_next_in_queue_false, app);
        gtk_widget_destroy(dialog);
    }
}



// Function to draw the video (GTK draw callback)
static gboolean on_draw(GtkWidget *widget, cairo_t *cr, AppData *app) {
    GdkRectangle rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = gtk_widget_get_allocated_width(widget);
    rect.height = gtk_widget_get_allocated_height(widget);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
    cairo_fill(cr);
    return TRUE;
}

static void on_drawing_area_realized(GtkWidget *widget, AppData *app) {
    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    if (!gdk_window) {
        fprintf(stderr, "Error: Could not get GdkWindow from drawing area.\n");
        return;
    }
    unsigned long window_id = GDK_WINDOW_XID(gdk_window);
    mpv_set_option(app->mpv, "wid", MPV_FORMAT_INT64, &window_id);
    mpv_set_option_string(app->mpv, "vo", "x11");
    mpv_set_option_string(app->mpv, "osc", "no");
    mpv_set_option_string(app->mpv, "hwdec", "auto");
    mpv_command(app->mpv, (const char *[]){"initialize", NULL});

    // Set target_widget here, after the window is realized
    target_widget = widget;

    // Get the normal cursor
    GdkWindow *mainwindow = gtk_widget_get_window(target_widget);
    if (mainwindow) {
        normal_cursor = gdk_window_get_cursor(mainwindow);
        if (!normal_cursor) {
            normal_cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_LEFT_PTR); // Default cursor
            gdk_window_set_cursor(mainwindow, normal_cursor); // Ensure it's set initially
        }
    } else {
        fprintf(stderr, "Error: Could not get GdkWindow for cursor handling.\n");
        // Handle this error appropriately (e.g., don't try to hide cursor)
    }
}

static gboolean update_slider(gpointer user_data) {
    AppData *app = (AppData *)user_data;
    if (!app || !app->mpv) return TRUE;

    double position = 0;
    double duration = 0;

    // Get current playback position
    if (mpv_get_property(app->mpv, "time-pos", MPV_FORMAT_DOUBLE, &position) < 0)
        position = 0;

    // Get total duration
    if (mpv_get_property(app->mpv, "duration", MPV_FORMAT_DOUBLE, &duration) < 0)
        duration = 0;

    if (!app->slider_dragging) {
        gtk_range_set_range(GTK_RANGE(app->slider), 0, (gint)duration);
        gtk_range_set_value(GTK_RANGE(app->slider), (gint)position);
    }

    // Update duration label with current time / total duration format
    int current_hours = (int)position / 3600;
    int current_minutes = ((int)position % 3600) / 60;
    int current_seconds = (int)position % 60;
    int total_hours = (int)duration / 3600;
    int total_minutes = ((int)duration % 3600) / 60;
    int total_seconds = (int)duration % 60;
    char label_text[64]; // Increased size to accommodate the longer string

    if (duration > 3600) { //Use HH:MM:SS if duration is over 1 hour
        snprintf(label_text, sizeof(label_text), "%d:%02d:%02d/%d:%02d:%02d",
                 current_hours, current_minutes, current_seconds,
                 total_hours, total_minutes, total_seconds);
    } else {
        snprintf(label_text, sizeof(label_text), "%02d:%02d/%02d:%02d",
                 current_minutes, current_seconds,
                 total_minutes, total_seconds);
    }
    gtk_label_set_text(GTK_LABEL(app->duration_label), label_text);

    return TRUE;
}

static gboolean on_slider_pressed(GtkWidget *widget, GdkEventButton *event, AppData *app) {
    app->slider_dragging = TRUE;
    return FALSE;
}

static gboolean on_slider_released(GtkWidget *widget, GdkEventButton *event, AppData *app) {
    app->slider_dragging = FALSE;
    double new_pos = gtk_range_get_value(GTK_RANGE(app->slider));
    mpv_set_property(app->mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_pos);
    return FALSE;
}

static void on_slider_moved(GtkRange *range, AppData *app) {
    if (app->slider_dragging) {
        // Optional: preview while dragging
    }
}

// Function to handle volume changes
static void on_volume_changed(GtkRange *range, AppData *app) {
    double volume = gtk_range_get_value(range);
    mpv_set_property(app->mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
}

static void update_volume_slider(AppData *app) {
    double volume = 0;
    if (mpv_get_property(app->mpv, "volume", MPV_FORMAT_DOUBLE, &volume) == 0) {
        gtk_range_set_value(GTK_RANGE(app->volume_slider), volume);
    }
}

static void on_play_button_clicked(GtkWidget *button, AppData *app) {
     int pause = 0;
     if (!app->mpv) {
        fprintf(stderr, "MPV is not initialized.\n");
        return;
    }
    if (mpv_get_property(app->mpv, "pause", MPV_FORMAT_FLAG, &pause) >= 0) {
        pause = !pause;
        mpv_set_property(app->mpv, "pause", MPV_FORMAT_FLAG, &pause);
    } else {
        fprintf(stderr, "Failed to get pause property.\n");
    }

    // Update button label
    if (pause) {
        // GtkWidget *old_image = gtk_button_get_image(GTK_BUTTON(app->play_button));
        // if (old_image) {
        //     gtk_widget_destroy(old_image);  // Free the old image
        // }
        GtkWidget *play_icon = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(app->play_button), play_icon);
    } else {
        // GtkWidget *old_image = gtk_button_get_image(GTK_BUTTON(app->play_button));
        // if (old_image) {
        //     gtk_widget_destroy(old_image);  // Free the old image
        // }
        GtkWidget *pause_icon = gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(app->play_button), pause_icon);
    }
}



// Function to add files to the video queue
void add_to_video_queue(const char *filename) {
    video_queue = g_list_append(video_queue, g_strdup(filename));
}


void load_next_video(mpv_handle *mpv, AppData *app) {
    if (video_queue && video_queue->next) {
        // Reset color of the previously playing item
        if (app->current_playlist_item) {
            GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
            gtk_style_context_remove_class(context, "playing-video");
        }

        current_video = current_video ? current_video->next : video_queue;

        // Find the corresponding menu item and set its color
        GList *children = gtk_container_get_children(GTK_CONTAINER(app->playlist_box));
        for (GList *child = children; child != NULL; child = child->next) {
            if (GTK_IS_MENU_ITEM(child->data)) {
                const char *item_label = gtk_menu_item_get_label(GTK_MENU_ITEM(child->data));
                if (item_label && strcmp(item_label, (char *)current_video->data) == 0) {
                    app->current_playlist_item = GTK_WIDGET(child->data);
                    GtkStyleContext *context = gtk_widget_get_style_context(app->current_playlist_item);
                    gtk_style_context_add_class(context, "playing-video");
                    break;
                }
            }
        }

        load_file_in_mpv(app->mpv, (char *)current_video->data);
    }
}

// Event handler to handle when the current video finishes
static void on_video_end(mpv_handle *mpv, AppData *app) {
    load_next_video(mpv, app);
}



// When hovering over the rightmost 20%, show the playlist
static gboolean on_hover_enter(GtkWidget *widget, GdkEvent *event, GtkWidget *playlist_box) {
    //gtk_widget_set_visible(playlist_box, TRUE);
    return FALSE;
}

// Hide the playlist when mouse leaves the area
static gboolean on_hover_leave(GtkWidget *widget, GdkEvent *event, GtkWidget *playlist_box) {
    //gtk_widget_set_visible(playlist_box, FALSE);
    return FALSE;
}



void toggle_playbar_visibility(GtkWidget *hbox) {
    gboolean is_visible = gtk_widget_get_visible(hbox);
    gtk_widget_set_visible(hbox, !is_visible);  // Toggle the visibility
}

void toggle_playbar_visibility_s(GtkWidget *slider_hbox) {
    gboolean is_visible = gtk_widget_get_visible(slider_hbox);
    gtk_widget_set_visible(slider_hbox, !is_visible);  // Toggle the visibility
}

// Function to toggle fullscreen
void toggle_fullscreen(GtkWidget *window) {
    GdkWindow *gdk_window = gtk_widget_get_window(window);
    GdkWindowState state = gdk_window_get_state(gdk_window);
    
    if (state & GDK_WINDOW_STATE_FULLSCREEN) {
        // If already fullscreen, unfullscreen the window
        gtk_window_unfullscreen(GTK_WINDOW(window));
    } else {
        // If not fullscreen, make the window fullscreen
        gtk_window_fullscreen(GTK_WINDOW(window));
    }
}

void toggle_fullscreen_via_button(GtkWidget *fullscreen_button, AppData *app) {
    GdkWindow *gdk_window = gtk_widget_get_window(app->window);
    GdkWindowState state = gdk_window_get_state(gdk_window);
    
    if (state & GDK_WINDOW_STATE_FULLSCREEN) {
        // If already fullscreen, unfullscreen the window
        gtk_window_unfullscreen(GTK_WINDOW(app->window));
    } else {
        // If not fullscreen, make the window fullscreen
        gtk_window_fullscreen(GTK_WINDOW(app->window));
    }
}

void escape_fullscreen(GtkWidget *window) {
    GdkWindow *gdk_window = gtk_widget_get_window(window);
    GdkWindowState state = gdk_window_get_state(gdk_window);
    
    if (state & GDK_WINDOW_STATE_FULLSCREEN) {
        // If already fullscreen, unfullscreen the window
        gtk_window_unfullscreen(GTK_WINDOW(window));
    } else {
        // If not fullscreen, make the window fullscreen
        //gtk_window_fullscreen(GTK_WINDOW(window));
    }
}

// Key press event handler for F11
gboolean on_key_press_f1(GtkWidget *hbox, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_F1) {
        // Call the toggle fullscreen function when F11 is pressed
        toggle_playbar_visibility((GtkWidget *)user_data);
        return TRUE;  // Indicate that the event has been handled
    }
    return FALSE;  // Allow other key events to be processed
}

// Key press event handler for F11
gboolean on_key_press_f1_slider(GtkWidget *slider_hbox, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_F2) {
        // Call the toggle fullscreen function when F11 is pressed
        toggle_playbar_visibility_s((GtkWidget *)user_data);
        return TRUE;  // Indicate that the event has been handled
    }
    return FALSE;  // Allow other key events to be processed
}

// Key press event handler for F11
gboolean on_key_press_f11(GtkWidget *window, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_F11) {
        // Call the toggle fullscreen function when F11 is pressed
        toggle_fullscreen(window);
        return TRUE;  // Indicate that the event has been handled
    }
    return FALSE;  // Allow other key events to be processed
}

// Key press event handler for Escape
gboolean on_key_press_escape(GtkWidget *window, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_Escape) {
        // Call the toggle fullscreen function when F11 is pressed
        escape_fullscreen(window);
        return TRUE;  // Indicate that the event has been handled
    }
    return FALSE;  // Allow other key events to be processed
}

// Key press event handler for Space
gboolean on_key_press_space(GtkWidget *window, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_space) {
        // Call the toggle function with the AppData
        on_play_pause_clicked(NULL, (AppData*)user_data); // Pass NULL for button, AppData for app
        return TRUE;
    }
    return FALSE;
}

// Key press event handler
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    // Check if F2 is pressed
    if (event->keyval == GDK_KEY_F3) {
        // Toggle the visibility of the playlist box
        toggle_playlist_visibility((GtkWidget *)user_data);
        return TRUE;  // Event handled
    }
    return FALSE;  // Event not handled
}

// Function to get available subtitle tracks from MPV
static SubtitleTracks get_available_sub_tracks(mpv_handle *mpv) {
    SubtitleTracks sub_tracks = {NULL, 0};
    mpv_node node;

    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) == 0 && node.format == MPV_FORMAT_NODE_ARRAY) {
        mpv_node_list *track_list = node.u.list;

        // First, count subtitle tracks
        int subtitle_count = 0;
        for (int i = 0; i < track_list->num; ++i) {
            if (track_list->values[i].format == MPV_FORMAT_NODE_MAP) {
                mpv_node_list *track = track_list->values[i].u.list;
                mpv_node *type_node = mpv_node_list_find_property(track, "type");
                if (type_node && type_node->format == MPV_FORMAT_STRING &&
                    strcmp(type_node->u.string, "sub") == 0) {
                    subtitle_count++;
                }
            }
        }

        if (subtitle_count > 0) {
            sub_tracks.tracks = g_malloc((subtitle_count + 1) * sizeof(char*));
            if (!sub_tracks.tracks) {
                fprintf(stderr, "Memory allocation failed in get_available_sub_tracks\n");
                mpv_free_node_contents(&node);
                return sub_tracks; // Return empty structure
            }

            int current_index = 0;
            for (int i = 0; i < track_list->num; ++i) {
                if (track_list->values[i].format == MPV_FORMAT_NODE_MAP) {
                    mpv_node_list *track = track_list->values[i].u.list;
                    mpv_node *type_node = mpv_node_list_find_property(track, "type");
                    if (type_node && type_node->format == MPV_FORMAT_STRING &&
                        strcmp(type_node->u.string, "sub") == 0) {
                        mpv_node *lang_node = mpv_node_list_find_property(track, "lang");
                        mpv_node *id_node = mpv_node_list_find_property(track, "id");

                        if (id_node && id_node->format == MPV_FORMAT_INT64) {
                            if (lang_node && lang_node->format == MPV_FORMAT_STRING) {
                                sub_tracks.tracks[current_index++] = g_strdup_printf("%s (ID: %lld)",
                                                                                    lang_node->u.string,
                                                                                    (long long)id_node->u.int64);
                            } else {
                                sub_tracks.tracks[current_index++] = g_strdup_printf("Track %lld",
                                                                                    (long long)id_node->u.int64);
                            }
                        }
                    }
                }
            }
            sub_tracks.tracks[current_index] = NULL;
            sub_tracks.count = current_index;
        }
        mpv_free_node_contents(&node);
    } else {
        fprintf(stderr, "Failed to get track-list from MPV\n");
    }

    return sub_tracks;
}

// Function to free subtitle track data
static void free_subtitle_tracks(SubtitleTracks *sub_tracks) {
    if (sub_tracks->tracks) {
        for (int i = 0; i < sub_tracks->count; ++i) {
            g_free(sub_tracks->tracks[i]);
        }
        g_free(sub_tracks->tracks);
        sub_tracks->tracks = NULL;
        sub_tracks->count = 0;
    }
}

// Function to free subtitle track data
static void free_video_tracks(VideoTracks *sub_tracks) {
    if (sub_tracks->tracks) {
        for (int i = 0; i < sub_tracks->count; ++i) {
            //g_free(sub_tracks->tracks[i]);
            sub_tracks->tracks[i] = NULL;
        }
        //g_free(sub_tracks->tracks);
        sub_tracks->tracks = NULL;
        sub_tracks->count = 0;
    }
}

// Function to free subtitle track data
static void free_audio_tracks(AudioTracks *sub_tracks) {
    if (sub_tracks->tracks) {
        for (int i = 0; i < sub_tracks->count; ++i) {
            //g_free(sub_tracks->tracks[i]);
            sub_tracks->tracks[i] = NULL;
        }
        //g_free(sub_tracks->tracks);
        sub_tracks->tracks = NULL;
        sub_tracks->count = 0;
    }
}

static void on_vmenu_destroyed(gpointer data, GObject *where_the_object_was) {
    VideoTrackCleanupData *cleanup = (VideoTrackCleanupData *)data;
    free_video_tracks(&cleanup->tracks);
    //g_free(cleanup);
}

static void on_amenu_destroyed(gpointer data, GObject *where_the_object_was) {
    AudioTrackCleanupData *cleanup = (AudioTrackCleanupData *)data;
    free_audio_tracks(&cleanup->tracks);
    //g_free(cleanup);
}



// Function to handle subtitle selection
static void on_subtitle_selected(GtkWidget *menu_item, AppData *app_data) {
    const gchar *label = gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));
    if (strcmp(label, "None") == 0) {
        mpv_set_property_string(app_data->mpv, "sid", "no");
    } else {
        // Improved ID extraction: Look for "ID: " followed by digits
        const char *id_start = strstr(label, "ID: ");
        if (id_start) {
            long long subtitle_id = strtoll(id_start + 4, NULL, 10); // Move past "ID: "
            if (errno == ERANGE) {
                fprintf(stderr, "Subtitle ID out of range: %s\n", label);
                return; // Or handle the error as appropriate
            }
            mpv_set_property(app_data->mpv, "sid", MPV_FORMAT_INT64, &subtitle_id);
        } else {
            fprintf(stderr, "Could not extract subtitle ID from label: %s\n", label);
        }
    }
}

// Function to create the subtitle menu
static GtkWidget *create_subtitle_menu(AppData *app_data) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *none_item = gtk_menu_item_new_with_label("None");
    g_signal_connect(none_item, "activate", G_CALLBACK(on_subtitle_selected), app_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), none_item);
    gtk_widget_show(none_item);

    SubtitleTracks sub_tracks = get_available_sub_tracks(app_data->mpv);
    if (sub_tracks.tracks) {
        for (int i = 0; i < sub_tracks.count; ++i) {
            GtkWidget *track_item = gtk_menu_item_new_with_label(sub_tracks.tracks[i]);
            g_signal_connect(track_item, "activate", G_CALLBACK(on_subtitle_selected), app_data);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), track_item);
            gtk_widget_show(track_item);
        }
    }
    free_subtitle_tracks(&sub_tracks); // Free immediately after use!

    return menu;
}

// Function to handle subtitle button clicks
static void on_subtitle_button_clicked(GtkWidget *button, AppData *app_data) {
    GtkWidget *subtitle_menu = create_subtitle_menu(app_data);
    gtk_menu_popup_at_widget(GTK_MENU(subtitle_menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
    // GTK will handle destroying the menu when it's closed
}


// Function to get available video tracks from MPV
static VideoTracks get_available_video_tracks(mpv_handle *mpv) {
    VideoTracks video_tracks = {NULL, 0}; // Reuse SubtitleTracks struct
    mpv_node node;

    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) == 0 && node.format == MPV_FORMAT_NODE_ARRAY) {
        mpv_node_list *track_list = node.u.list;

        // First, count video tracks
        int video_count = 0;
        for (int i = 0; i < track_list->num; ++i) {
            if (track_list->values[i].format == MPV_FORMAT_NODE_MAP) {
                mpv_node_list *track = track_list->values[i].u.list;
                mpv_node *type_node = mpv_node_list_find_property(track, "type");
                if (type_node && type_node->format == MPV_FORMAT_STRING &&
                    strcmp(type_node->u.string, "video") == 0) {
                    video_count++;
                }
            }
        }

        if (video_count > 0) {
            video_tracks.tracks = g_malloc0((video_count + 1) * sizeof(char*));
            for (int i = 0; i < video_count + 1; ++i)
            video_tracks.tracks[i] = NULL;
            if (!video_tracks.tracks) {
                fprintf(stderr, "Memory allocation failed in get_available_video_tracks\n");
                mpv_free_node_contents(&node);
                return video_tracks;
            }

            int current_index = 0;
            for (int i = 0; i < track_list->num; ++i) {
                if (track_list->values[i].format == MPV_FORMAT_NODE_MAP) {
                    mpv_node_list *track = track_list->values[i].u.list;
                    mpv_node *id_node = mpv_node_list_find_property(track, "id");
                    mpv_node *lang_node = mpv_node_list_find_property(track, "lang");
                    if (id_node && id_node->format == MPV_FORMAT_INT64) {
                        if (lang_node && lang_node->format == MPV_FORMAT_STRING) {
                            video_tracks.tracks[current_index] = g_strdup_printf("%s (ID: %lld)",
                                                                                 lang_node->u.string,
                                                                                 (long long)id_node->u.int64);
                        } else {
                            video_tracks.tracks[current_index] = g_strdup_printf("Track %lld",
                                                                                 (long long)id_node->u.int64);
                        }
                        current_index++;
                    }
                }
            }
            video_tracks.tracks[current_index] = NULL;
            video_tracks.count = current_index;
        }
        mpv_free_node_contents(&node);
    } else {
        fprintf(stderr, "Failed to get track-list from MPV\n");
    }

    return video_tracks;
}

// Function to get available audio tracks from MPV
static AudioTracks get_available_audio_tracks(mpv_handle *mpv) {
    AudioTracks audio_tracks = {NULL, 0}; // Reuse SubtitleTracks struct
    mpv_node node;

    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &node) == 0 && node.format == MPV_FORMAT_NODE_ARRAY) {
        mpv_node_list *track_list = node.u.list;

        // First, count audio tracks
        int audio_count = 0;
        for (int i = 0; i < track_list->num; ++i) {
            if (track_list->values[i].format == MPV_FORMAT_NODE_MAP) {
                mpv_node_list *track = track_list->values[i].u.list;
                mpv_node *type_node = mpv_node_list_find_property(track, "type");
                if (type_node && type_node->format == MPV_FORMAT_STRING &&
                    strcmp(type_node->u.string, "audio") == 0) {
                    audio_count++;
                }
            }
        }

        if (audio_count > 0) {
            audio_tracks.tracks = g_malloc0((audio_count + 1) * sizeof(char*));
            for (int i = 0; i < audio_count + 1; ++i)
            audio_tracks.tracks[i] = NULL;
            if (!audio_tracks.tracks) {
                fprintf(stderr, "Memory allocation failed in get_available_audio_tracks\n");
                mpv_free_node_contents(&node);
                return audio_tracks;
            }

            int current_index = 0;
            for (int i = 0; i < track_list->num; ++i) {
                if (track_list->values[i].format == MPV_FORMAT_NODE_MAP) {
                    mpv_node_list *track = track_list->values[i].u.list;
                    mpv_node *id_node = mpv_node_list_find_property(track, "id");
                    mpv_node *lang_node = mpv_node_list_find_property(track, "lang");

                    if (id_node && id_node->format == MPV_FORMAT_INT64) {
                        if (lang_node && lang_node->format == MPV_FORMAT_STRING) {
                            audio_tracks.tracks[current_index] = g_strdup_printf("%s (ID: %lld)",
                                                                                 lang_node->u.string,
                                                                                 (long long)id_node->u.int64);
                        } else {
                            audio_tracks.tracks[current_index] = g_strdup_printf("Track %lld",
                                                                                 (long long)id_node->u.int64);
                        }
                        current_index++;
                    }
                }
            }
            audio_tracks.tracks[current_index] = NULL;
            audio_tracks.count = current_index;
        }
        mpv_free_node_contents(&node);
    } else {
        fprintf(stderr, "Failed to get track-list from MPV\n");
    }

    return audio_tracks;
}

// Function to handle video track selection
static void on_video_track_selected(GtkWidget *menu_item, AppData *app_data) {
    const gchar *label = gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));
    if (strcmp(label, "None") == 0) {
        mpv_set_property_string(app_data->mpv, "vid", "no");
    } else {
        // Improved ID extraction: Look for "ID: " followed by digits
        const char *id_start = strstr(label, "ID: ");
        if (id_start) {
            long long video_id = strtoll(id_start + 4, NULL, 10); // Move past "ID: "
            if (errno == ERANGE) {
                fprintf(stderr, "Video ID out of range: %s\n", label);
                return; // Or handle the error as appropriate
            }
            mpv_set_property(app_data->mpv, "vid", MPV_FORMAT_INT64, &video_id);
        } else {
            fprintf(stderr, "Could not extract video ID from label: %s\n", label);
        }
    }
}

// Function to handle audio track selection
static void on_audio_track_selected(GtkWidget *menu_item, AppData *app_data) {
    const gchar *label = gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));
    if (strcmp(label, "None") == 0) {
        mpv_set_property_string(app_data->mpv, "aid", "no");
    } else {
        // Improved ID extraction: Look for "ID: " followed by digits
        const char *id_start = strstr(label, "ID: ");
        if (id_start) {
            long long audio_id = strtoll(id_start + 4, NULL, 10); // Move past "ID: "
            if (errno == ERANGE) {
                fprintf(stderr, "Audio ID out of range: %s\n", label);
                return; // Or handle the error as appropriate
            }
            mpv_set_property(app_data->mpv, "aid", MPV_FORMAT_INT64, &audio_id);
        } else {
            fprintf(stderr, "Could not extract audio ID from label: %s\n", label);
        }
    }
}

// Function to create the video track menu
static GtkWidget *create_video_track_menu(AppData *app_data) {
    if (cached_vmenu) {
        gtk_widget_show_all(cached_vmenu);
        return cached_vmenu;
    }
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *none_item = gtk_menu_item_new_with_label("None");
    g_signal_connect(none_item, "activate", G_CALLBACK(on_video_track_selected), app_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), none_item);
    gtk_widget_show(none_item);

    VideoTracks video_tracks = get_available_video_tracks(app_data->mpv);
    if (video_tracks.tracks) {
        for (int i = 0; i < video_tracks.count; ++i) {
            GtkWidget *track_item = gtk_menu_item_new_with_label(video_tracks.tracks[i]);
            g_signal_connect(track_item, "activate", G_CALLBACK(on_video_track_selected), app_data);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), track_item);
            gtk_widget_show(track_item);
        }
    }
    //free_video_tracks(&video_tracks); // Reuse the subtitle track freeing function
    VideoTrackCleanupData *cleanup = g_new0(VideoTrackCleanupData, 1);
    cleanup->tracks = video_tracks;
    g_object_weak_ref(G_OBJECT(menu), on_vmenu_destroyed, cleanup);
    cached_vmenu = menu; // Save it for reuse
    return menu;
}

// Function to create the audio track menu
static GtkWidget *create_audio_track_menu(AppData *app_data) {
    if (cached_amenu) {
        gtk_widget_show_all(cached_amenu);
        return cached_amenu;
    }
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *none_item = gtk_menu_item_new_with_label("None");
    g_signal_connect(none_item, "activate", G_CALLBACK(on_audio_track_selected), app_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), none_item);
    gtk_widget_show(none_item);

    AudioTracks audio_tracks = get_available_audio_tracks(app_data->mpv);
    if (audio_tracks.tracks) {
        for (int i = 0; i < audio_tracks.count; ++i) {
            GtkWidget *track_item = gtk_menu_item_new_with_label(audio_tracks.tracks[i]);
            g_signal_connect(track_item, "activate", G_CALLBACK(on_audio_track_selected), app_data);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), track_item);
            gtk_widget_show(track_item);
        }
    }
    //free_audio_tracks(&audio_tracks); // Reuse the subtitle track freeing function
    AudioTrackCleanupData *cleanup = g_new0(AudioTrackCleanupData, 1);
    cleanup->tracks = audio_tracks;
    g_object_weak_ref(G_OBJECT(menu), on_amenu_destroyed, cleanup);
    cached_amenu = menu;
    return menu;
}



// Function to handle video track button clicks
static void on_video_track_button_clicked(GtkWidget *button, AppData *app_data) {
    GtkWidget *video_track_menu = create_video_track_menu(app_data);
    gtk_menu_popup_at_widget(GTK_MENU(video_track_menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
    // GTK will handle destroying the menu when it's closed
}

// Function to handle audio track button clicks
static void on_audio_track_button_clicked(GtkWidget *button, AppData *app_data) {
    GtkWidget *audio_track_menu = create_audio_track_menu(app_data);
    gtk_menu_popup_at_widget(GTK_MENU(audio_track_menu), button, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
    // GTK will handle destroying the menu when it's closed
}

int main(int argc, char *argv[]) {
    // 0. Force the locale *before* anything else
    setenv("LC_NUMERIC", "C", 1);
    printf("Current LC_NUMERIC: %s\n", setlocale(LC_NUMERIC, NULL));

    // 1. Initialize GTK+
    gtk_init(&argc, &argv);
    if (!gtk_init_check(&argc, &argv)) {
        fprintf(stderr, "GTK initialization failed.\n");
        return 1;
    }

    // 2. Create the main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (!window) {
        fprintf(stderr, "Failed to create main window.\n");
        return 1;
    }
    gtk_window_set_title(GTK_WINDOW(window), "Eluxi-Player");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);


    // Create a CSS provider
    GtkCssProvider *cssProvider = gtk_css_provider_new();
    if (!cssProvider) {
        fprintf(stderr, "Failed to create CSS provider.\n");
        return 1;
    }

    // Load CSS
    if (gtk_css_provider_load_from_data(cssProvider,
                                      ".playing-video { color: royalblue; }",
                                      -1, NULL) == FALSE) {
        fprintf(stderr, "Failed to load CSS.\n");
        g_object_unref(cssProvider);
        return 1;
    }

    // Apply the CSS to the application's style context
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), // Get the default screen
        GTK_STYLE_PROVIDER(cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    g_object_unref(cssProvider);

    // 3. Create a vertical box to hold widgets
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    if (!vbox) {
        fprintf(stderr, "Failed to create vbox.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // 1. Create a new horizontal box (hboxa) to hold the video panel and playlist box side by side
GtkWidget *hboxa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
if (!hboxa) {
    fprintf(stderr, "Failed to create hboxa.\n");
    gtk_widget_destroy(window);
    return 1;
}

    // 4. Create the drawing area for video
    GtkWidget *drawing_area = gtk_drawing_area_new();
    if (!drawing_area) {
        fprintf(stderr, "Failed to create drawing area.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    gtk_widget_set_size_request(drawing_area, 800, 600);
    gtk_widget_set_hexpand(drawing_area, TRUE);
    gtk_widget_set_vexpand(drawing_area, TRUE);
    //gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);


    
        //Queue-list widget for playlist
        GtkWidget *playlist_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        //gtk_widget_set_size_request(playlist_box, 400, 300);  // Set an appropriate size
        //gtk_widget_set_visible(playlist_box, FALSE);  // Initially hidden
        //gtk_box_pack_start(GTK_BOX(hbox), playlist_box, TRUE, TRUE, 0);

            // 4. Pack the video panel (drawing_area) into hboxa
gtk_box_pack_start(GTK_BOX(hboxa), drawing_area, TRUE, TRUE, 0);

// 5. Pack the playlist box into hboxa (will appear to the right of drawing_area)
//gtk_box_pack_start(GTK_BOX(hboxa), playlist_box, FALSE, FALSE, 0);  // FALSE ensures it doesn't expand

// 6. Add hboxa to the main vbox
gtk_box_pack_start(GTK_BOX(vbox), hboxa, TRUE, TRUE, 0);

    // 5.5 Create a horizontal box for slider and duration
    GtkWidget *slider_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    if (!slider_hbox) {
        fprintf(stderr, "Failed to create slider_hbox.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    gtk_box_pack_start(GTK_BOX(vbox), slider_hbox, FALSE, FALSE, 5);

    // 5. Create a horizontal box for buttons
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    if (!hbox) {
        fprintf(stderr, "Failed to create hbox.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    // 6. Create the Play/Pause button with icon
    GtkWidget *play_button = gtk_button_new();
    GtkWidget *play_icon = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
    GtkWidget *pause_icon = gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(play_button), pause_icon);
    gtk_box_pack_start(GTK_BOX(hbox), play_button, FALSE, FALSE, 0);
    if (!play_button) {
        fprintf(stderr, "Failed to create play button.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    // gtk_box_pack_start(GTK_BOX(hbox), play_button, FALSE, FALSE, 0); // REMOVE THIS LINE

      // 7. Create the Stop button with icon
      GtkWidget *stop_icon = gtk_image_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_BUTTON);
      GtkWidget *stop_button = gtk_button_new();
      gtk_button_set_image(GTK_BUTTON(stop_button), stop_icon);
      gtk_box_pack_start(GTK_BOX(hbox), stop_button, FALSE, FALSE, 0);
    if (!stop_button) {
        fprintf(stderr, "Failed to create stop button.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    // gtk_box_pack_start(GTK_BOX(hbox), stop_button, FALSE, FALSE, 0); // REMOVE THIS LINE

    // 8. Create the file chooser button
    GtkWidget *file_icon = gtk_image_new_from_icon_name("file-manager", GTK_ICON_SIZE_BUTTON);
    GtkWidget *file_button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(file_button), file_icon);
    gtk_box_pack_start(GTK_BOX(hbox), file_button, FALSE, FALSE, 0);
    if (!file_button) {
        fprintf(stderr, "Failed to create file button.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    // gtk_box_pack_start(GTK_BOX(hbox), file_button, FALSE, FALSE, 0); // REMOVE THIS LINE



    // Create the slider
    GtkWidget *slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    if (!slider) {
        fprintf(stderr, "Failed to create slider.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
    gtk_widget_set_hexpand(slider, TRUE);
    gtk_box_pack_start(GTK_BOX(slider_hbox), slider, TRUE, TRUE, 5);

    // Create the duration label
    GtkWidget *duration_label = gtk_label_new("00:00");
    if (!duration_label) {
        fprintf(stderr, "Failed to create duration label.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    gtk_box_pack_start(GTK_BOX(slider_hbox), duration_label, FALSE, FALSE, 5);

    // Create a horizontal box for volume slider
    GtkWidget *volume_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    if (!volume_hbox) {
        fprintf(stderr, "Failed to create volume hbox.\n");
        gtk_widget_destroy(window);
        return 1;
    }

    // Create a horizontal slider for volume control
    GtkWidget *volume_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    if (!volume_slider) {
        fprintf(stderr, "Failed to create volume slider.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    gtk_scale_set_draw_value(GTK_SCALE(volume_slider), FALSE);
    gtk_widget_set_size_request(volume_slider, 150, 30);
    // Create volume icon
    GtkWidget *volume_icon = gtk_image_new_from_icon_name("audio-volume-medium", GTK_ICON_SIZE_SMALL_TOOLBAR);
    if (!volume_icon) {
        fprintf(stderr, "Failed to create volume icon.\n");
        gtk_widget_destroy(window);
        return 1;
    }

    GtkWidget *subtitle_icon = gtk_image_new_from_icon_name("text-plain", GTK_ICON_SIZE_BUTTON);
GtkWidget *subtitle_button = gtk_button_new();
gtk_button_set_image(GTK_BUTTON(subtitle_button), subtitle_icon);
gtk_box_pack_start(GTK_BOX(hbox), subtitle_button, FALSE, FALSE, 0);
if (!subtitle_button) {
    fprintf(stderr, "Failed to create subtitle button.\n");
    gtk_widget_destroy(window);
    return 1;
}

    // Video track button
    GtkWidget *video_track_icon = gtk_image_new_from_icon_name("video-display", GTK_ICON_SIZE_BUTTON); // Choose an appropriate icon
    GtkWidget *video_track_button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(video_track_button), video_track_icon);
    gtk_box_pack_start(GTK_BOX(hbox), video_track_button, FALSE, FALSE, 0);
    
    
    // Audio track button
    GtkWidget *audio_track_icon = gtk_image_new_from_icon_name("audio-headphones-symbolic", GTK_ICON_SIZE_BUTTON); // Choose an appropriate icon
    GtkWidget *audio_track_button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(audio_track_button), audio_track_icon);
    gtk_box_pack_start(GTK_BOX(hbox), audio_track_button, FALSE, FALSE, 0);


    GtkWidget *playlist_icon = gtk_image_new_from_icon_name("playlist-symbolic", GTK_ICON_SIZE_BUTTON); 
    GtkWidget *playlist_button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(playlist_button), playlist_icon);
    gtk_box_pack_start(GTK_BOX(hbox), playlist_button, FALSE, FALSE, 0);
    

    gtk_box_pack_start(GTK_BOX(volume_hbox), volume_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(volume_hbox), volume_slider, FALSE, FALSE, 0);


    


    GtkWidget *fullscreen_icon = gtk_image_new_from_icon_name("view-fullscreen-symbolic", GTK_ICON_SIZE_BUTTON);
    GtkWidget *fullscreen_button = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(fullscreen_button), fullscreen_icon);
    gtk_box_pack_start(GTK_BOX(hbox), fullscreen_button, FALSE, FALSE, 0);
    if (!fullscreen_button) {
        fprintf(stderr, "Failed to create file button.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    //gtk_box_pack_start(GTK_BOX(vbox), volume_hbox, FALSE, FALSE, 0); // Removed from here







    // Add a spacer to push volume_hbox to the right
    GtkWidget *spacer = gtk_label_new("");  // Create an empty label
    gtk_widget_set_hexpand(spacer, TRUE);    // Make it expand horizontally
    gtk_box_pack_start(GTK_BOX(hbox), spacer, TRUE, TRUE, 0); // Add spacer to hbox
    gtk_box_pack_end(GTK_BOX(hbox), volume_hbox, FALSE, FALSE, 0);  // Add volume_hbox to the end of hbox



    //track mouse for widget display
    GtkWidget *hover_box = gtk_event_box_new();
    gtk_widget_set_size_request(hover_box, 0, -1);  // Will expand vertically only
    gtk_box_pack_end(GTK_BOX(vbox), hover_box, FALSE, TRUE, 0);



    // Show the playlist when hovering
    // Connect key press event to the window
    gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), playlist_box);

    //g_signal_connect(hover_box, "enter-notify-event", G_CALLBACK(on_hover_enter), playlist_box);
   // g_signal_connect(hover_box, "leave-notify-event", G_CALLBACK(on_hover_leave), playlist_box);

   // gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_f11), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_escape), NULL);
    

    //gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_f1), hbox);

    //gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_f1_slider), slider_hbox);


    


    // After creating the window and drawing_area:
    target_widget = drawing_area;  // Or the window, depending on your needs

    // Create a blank cursor to hide the mouse
    hidden_cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_BLANK_CURSOR);

    // Get the normal cursor
    GdkWindow *mainwindow = gtk_widget_get_window(target_widget);
    if (mainwindow) {
        normal_cursor = gdk_window_get_cursor(mainwindow);
        if (!normal_cursor) {
            normal_cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_LEFT_PTR); // Default cursor
            gdk_window_set_cursor(mainwindow, normal_cursor); // Ensure it's set initially
        }
    } else {
        fprintf(stderr, "Error: Could not get GdkWindow for cursor handling.\n");
        // Handle this error appropriately (e.g., don't try to hide cursor)
    }

    // Connect the motion notify event to reset the timer
    gtk_widget_add_events(target_widget, GDK_POINTER_MOTION_MASK);
    g_signal_connect(target_widget, "motion-notify-event", G_CALLBACK(reset_cursor_timer), NULL);


    // 9. Initialize MPV
    mpv_handle *mpv = mpv_create();
    if (!mpv) {
        fprintf(stderr, "Error: could not create mpv client.\n");
        gtk_widget_destroy(window);
        return 1;
    }
    if (mpv_initialize(mpv) < 0) {
        fprintf(stderr, "Failed to initialize mpv context.\n");
        mpv_destroy(mpv);
        gtk_widget_destroy(window);
        return 1;
    }
    
    // 10. Set up AppData and connect signals
    AppData app_data;
    app_data.mpv = mpv;
    app_data.window = window;
    app_data.drawing_area = drawing_area;
    app_data.play_icon = play_icon;
    app_data.pause_icon = pause_icon;
    app_data.play_button = play_button;
    app_data.stop_button = stop_button;
    app_data.file_button = file_button;
    app_data.slider = slider;
    app_data.duration_label = duration_label;
    app_data.slider_dragging = FALSE;
    app_data.volume_slider = volume_slider;
    app_data.volume_icon = volume_icon;
    app_data.playlist_box = playlist_box;
    app_data.hover_box = hover_box;
    app_data.fullscreen_button = fullscreen_button;
    app_data.fullscreen_icon = fullscreen_icon;
    app_data.playlist_button = playlist_button; // Store it in AppData
    app_data.video_track_button = video_track_button; // Store in AppData
    app_data.audio_track_button = audio_track_button; // Store in AppData

    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), &app_data);
    g_signal_connect(drawing_area, "realize", G_CALLBACK(on_drawing_area_realized), &app_data);
    g_signal_connect(play_button, "clicked", G_CALLBACK(on_play_pause_clicked), &app_data);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), &app_data);
    g_signal_connect(file_button, "clicked", G_CALLBACK(on_file_open_clicked), &app_data);
    g_signal_connect(fullscreen_button, "clicked", G_CALLBACK(toggle_fullscreen_via_button), &app_data);
    g_signal_connect(slider, "button-press-event", G_CALLBACK(on_slider_pressed), &app_data);
    g_signal_connect(slider, "button-release-event", G_CALLBACK(on_slider_released), &app_data);
    g_signal_connect(slider, "value-changed", G_CALLBACK(on_slider_moved), &app_data);
    g_signal_connect(volume_slider, "value-changed", G_CALLBACK(on_volume_changed), &app_data);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press_space), &app_data);
    g_signal_connect(subtitle_button, "clicked", G_CALLBACK(on_subtitle_button_clicked), &app_data);
    g_signal_connect(playlist_button, "clicked", G_CALLBACK(on_playlist_button_clicked), &app_data);
    g_signal_connect(video_track_button, "clicked", G_CALLBACK(on_video_track_button_clicked), &app_data);
    g_signal_connect(audio_track_button, "clicked", G_CALLBACK(on_audio_track_button_clicked), &app_data);
    load_lua_scripts(mpv);  //  <--  Load the scripts here
    // 12. Create a thread to handle MPV events
    GThread *mpv_thread = g_thread_new("mpv_event_thread", (GThreadFunc)handle_mpv_events, &app_data);
    if (!mpv_thread) {
        fprintf(stderr, "Error creating MPV event thread.\n");
        mpv_destroy(mpv);
        gtk_widget_destroy(window);
        return 1;
    }

    // 13. Load file from command line
    if (argc > 1) {
        load_file_in_mpv(mpv, argv[1]);
    }

    // Set initial volume to max (100)
    mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &(double){100});
    gtk_range_set_value(GTK_RANGE(volume_slider), 100);


    // 14. Show the window *before* entering the main loop
    gtk_widget_show_all(window);
    toggle_playlist_visibility(playlist_box);
    // 15. Start the GTK+ main loop
    g_timeout_add(500, (GSourceFunc)update_slider, &app_data);
    gtk_main();

    // 16. Clean up
    mpv_command(mpv, (const char *[]){"quit", NULL});
    g_thread_join(mpv_thread);
    mpv_destroy(mpv);
    g_free(video_queue);
    return 0;
}

