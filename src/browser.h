/**
 * File System Browser
 *
 * @author Johannes Braun <johannes.braun@hannenzn.de>
 * @package kiddyblaster
 * @version Mon Jan  6 20:40:45 GMT 2020
 */
#ifndef __BROWSER_H__
#define __BROWSER_H__

typedef struct {
    gchar base_path[256];
    GList *directories;
    GList *selected_dir;
} Browser;


Browser *browser_new(const gchar *base_path);
void browser_start_browsing(Browser *browser);
void browser_next(Browser *browser);
void browser_previous(Browser *browser);
void browser_up(Browser *browser);
const gchar *browser_get_selected_directory(Browser *browser);
// static void read_directories(const char *path, int depth);

#endif
