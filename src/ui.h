#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>

/* Builds the (initially hidden) floating window, wires up clipboard monitoring,
 * the expiry cleanup timer, and the global hotkey. Call once at startup. */
void ui_init(void);

/* Shows the window near the mouse cursor if hidden, hides it if shown.
 * Safe to call from a GTK idle callback (used by the hotkey listener). */
void ui_toggle_window(gpointer user_data);

#endif /* UI_H */
