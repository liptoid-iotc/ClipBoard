#ifndef STORAGE_H
#define STORAGE_H

#include <gtk/gtk.h>
#include <time.h>

typedef enum {
    ITEM_TEXT,
    ITEM_IMAGE
} ItemType;

typedef struct ClipboardItem {
    int id;
    ItemType type;
    char *text;          /* used when type == ITEM_TEXT */
    char *filepath;      /* used when type == ITEM_IMAGE, points to a PNG under storage_dir() */
    time_t timestamp;
    struct ClipboardItem *next;
} ClipboardItem;

/* Creates /tmp/clipboard-manager (and /tmp/clipboard-manager/images) if missing. */
void storage_init(void);

/* Returns the base storage directory path (e.g. "/tmp/clipboard-manager"). */
const char *storage_dir(void);

/* Adds a text item to the top of the list. Returns the new item's id, or -1 on duplicate/ignore. */
int storage_add_text(const char *text);

/* Adds an image item (PNG saved to disk) to the top of the list. Returns new item's id. */
int storage_add_image(GdkPixbuf *pixbuf);

/* Deletes a single item by id (removes file from disk too). */
void storage_delete(int id);

/* Deletes every item and wipes the storage directory contents. */
void storage_clear_all(void);

/* Returns the current list head (newest first). Do not free; owned by storage module. */
ClipboardItem *storage_get_all(void);

/* Removes any item older than timeout_minutes. Returns TRUE if anything changed. */
gboolean storage_cleanup_expired(int timeout_minutes);

/* Set/get the ignore flag used to avoid re-capturing our own clipboard writes. */
void storage_set_ignore_next(gboolean ignore);
gboolean storage_get_ignore_next(void);

#endif /* STORAGE_H */
