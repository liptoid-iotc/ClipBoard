#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static ClipboardItem *g_list = NULL;   /* newest-first singly linked list */
static int g_next_id = 1;
static char g_dir[512];
static char g_img_dir[512];
static gboolean g_ignore_next = FALSE;

void storage_set_ignore_next(gboolean ignore) { g_ignore_next = ignore; }
gboolean storage_get_ignore_next(void) { return g_ignore_next; }

const char *storage_dir(void) { return g_dir; }

void storage_init(void)
{
    snprintf(g_dir, sizeof(g_dir), "/tmp/clipboard-manager");
    snprintf(g_img_dir, sizeof(g_img_dir), "/tmp/clipboard-manager/images");
    mkdir(g_dir, 0700);
    mkdir(g_img_dir, 0700);
}

static void free_item(ClipboardItem *it)
{
    if (!it) return;
    g_free(it->text);
    g_free(it->filepath);
    free(it);
}

int storage_add_text(const char *text)
{
    if (!text || !*text) return -1;

    /* Avoid pushing an exact duplicate of the current top item (just refresh timestamp). */
    if (g_list && g_list->type == ITEM_TEXT && g_list->text && strcmp(g_list->text, text) == 0) {
        g_list->timestamp = time(NULL);
        return g_list->id;
    }

    ClipboardItem *it = calloc(1, sizeof(ClipboardItem));
    it->id = g_next_id++;
    it->type = ITEM_TEXT;
    it->text = g_strdup(text);
    it->timestamp = time(NULL);
    it->next = g_list;
    g_list = it;
    return it->id;
}

int storage_add_image(GdkPixbuf *pixbuf)
{
    if (!pixbuf) return -1;

    int id = g_next_id++;
    char path[600];
    snprintf(path, sizeof(path), "%s/img_%d.png", g_img_dir, id);

    GError *err = NULL;
    if (!gdk_pixbuf_save(pixbuf, path, "png", &err, NULL)) {
        g_warning("Failed to save clipboard image: %s", err ? err->message : "unknown error");
        if (err) g_error_free(err);
        g_next_id--; /* give the id back */
        return -1;
    }

    ClipboardItem *it = calloc(1, sizeof(ClipboardItem));
    it->id = id;
    it->type = ITEM_IMAGE;
    it->filepath = g_strdup(path);
    it->timestamp = time(NULL);
    it->next = g_list;
    g_list = it;
    return it->id;
}

void storage_delete(int id)
{
    ClipboardItem *prev = NULL, *cur = g_list;
    while (cur) {
        if (cur->id == id) {
            if (cur->type == ITEM_IMAGE && cur->filepath) {
                unlink(cur->filepath);
            }
            if (prev) prev->next = cur->next;
            else g_list = cur->next;
            free_item(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void storage_clear_all(void)
{
    ClipboardItem *cur = g_list;
    while (cur) {
        ClipboardItem *next = cur->next;
        if (cur->type == ITEM_IMAGE && cur->filepath) {
            unlink(cur->filepath);
        }
        free_item(cur);
        cur = next;
    }
    g_list = NULL;
}

ClipboardItem *storage_get_all(void)
{
    return g_list;
}

gboolean storage_cleanup_expired(int timeout_minutes)
{
    if (timeout_minutes <= 0) return FALSE; /* 0 or negative = never expire */

    time_t now = time(NULL);
    time_t max_age = (time_t)timeout_minutes * 60;
    gboolean changed = FALSE;

    ClipboardItem *prev = NULL, *cur = g_list;
    while (cur) {
        ClipboardItem *next = cur->next;
        if (now - cur->timestamp >= max_age) {
            if (cur->type == ITEM_IMAGE && cur->filepath) {
                unlink(cur->filepath);
            }
            if (prev) prev->next = next;
            else g_list = next;
            free_item(cur);
            changed = TRUE;
        } else {
            prev = cur;
        }
        cur = next;
    }
    return changed;
}
