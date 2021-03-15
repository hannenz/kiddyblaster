/**
 * File System Browser
 *
 * @author Johannes Braun <johannes.braun@hannenzn.de>
 * @package kiddyblaster
 * @version Mon Jan  6 20:40:45 GMT 2020
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include "browser.h"

void list_free(List *list) {
	List *p;
	for (p = list; p != NULL; p = p->next) {
		if (p->data != NULL) {
			free(p->data);
		}
		free(p);
	}
}


List *list_append(List *list, void *data) {
	List *p, *p0;
	if (list == NULL) {
		list = malloc(sizeof(List));
		if (list != NULL) {
			list->prev = NULL;
			list->next = NULL;
		}
	}
	else {
		for (p = list; p->next != NULL; p = p->next) 
			;

		p->next = malloc(sizeof(List));
		p0 = p;
		p = p->next;
		p->data = data;
		p->prev = p0;
	}
	return list;
}



static void read_directories(Browser *browser, const char *path, int depth);


/**
 * Creates a new FileBrowser
 *
 * @param const gchar*  base_path
 * @return Object       FileBrowser object, free with `free`
 */
Browser *browser_new(const char *base_path) {

    Browser *browser = malloc(sizeof(Browser));
    if (browser == NULL) {
        return NULL;
    }

    browser->directories = NULL;
    browser->selected_dir = NULL;
    
    strncpy(browser->base_path, base_path, sizeof(browser->base_path));
    return browser;
}


void browser_start_browsing(Browser *browser) {
    list_free(browser->directories);
    browser->directories = NULL;

	printf("Start browsing at: %s\n", browser->base_path);
    read_directories(browser, browser->base_path, 0);
    browser->selected_dir = browser->directories;
}


const char *browser_get_selected_directory(Browser *browser) {
	if (browser == NULL) {
		return NULL;
	}
	if (browser->selected_dir == NULL) {
		return NULL;
	}

    return  browser->selected_dir->data;
}


void browser_previous(Browser *browser) {
    if (browser->selected_dir->prev != NULL) {
        browser->selected_dir = browser->selected_dir->prev;
    }
}



void browser_next(Browser *browser) {
    if (browser->selected_dir->next != NULL) {
        browser->selected_dir = browser->selected_dir->next;
    }
}



static void read_directories(Browser *browser, const char *path, int depth) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(path)) == NULL) {
        return;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_DIR) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }

            char new_path[1024];

            snprintf(new_path, sizeof(new_path), "%s/%s", path, ent->d_name);
            browser->directories = list_append(browser->directories, strdup(new_path));
            read_directories(browser, new_path, depth + 4);
        }
    }

    closedir(dir);
    return;
}
