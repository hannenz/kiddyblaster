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
	void *data;
	void *next;
	void *prev;
} List;

void list_free(List *list);
List *list_append(List *list, void* data);


typedef struct {
    char base_path[256];
    List *directories;
    List *selected_dir;
} Browser;


Browser *browser_new(const char *base_path);
void browser_start_browsing(Browser *browser);
void browser_next(Browser *browser);
void browser_previous(Browser *browser);
void browser_up(Browser *browser);
const char *browser_get_selected_directory(Browser *browser);
// static void read_directories(const char *path, int depth);

#endif
