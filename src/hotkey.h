#ifndef HOTKEY_H
#define HOTKEY_H

#include <glib.h>

/* Callback fired (on the GTK main thread, via g_idle_add) whenever Alt+L is pressed. */
typedef void (*HotkeyCallback)(gpointer user_data);

/* Starts a background thread that grabs Alt+L globally via X11 (XGrabKey)
 * and invokes cb whenever it's pressed. Returns TRUE on success. Requires X11. */
gboolean hotkey_start(HotkeyCallback cb, gpointer user_data);

/* Stops the listener thread and ungrabs the key. Safe to call even if never started. */
void hotkey_stop(void);

#endif /* HOTKEY_H */
