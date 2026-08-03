#include "ui.h"
#include "storage.h"
#include "hotkey.h"
#include <string.h>
#include <stdio.h>

#define WINDOW_WIDTH  340
#define WINDOW_HEIGHT 420
#define THUMB_SIZE    64
#define PREVIEW_MAXLEN 120

static GtkWidget *g_window = NULL;
static GtkWidget *g_listbox = NULL;
static GtkWidget *g_spin_expiry = NULL;
static GtkWidget *g_count_label = NULL;
static int g_timeout_minutes = 30;

static void refresh_list(void);

/* ---------- small helpers ---------- */

static char *make_preview(const char *text)
{
    /* Collapse newlines to spaces and cap length so rows stay one line & tidy. */
    GString *s = g_string_new(NULL);
    for (const char *p = text; *p && s->len < PREVIEW_MAXLEN; p++) {
        g_string_append_c(s, (*p == '\n' || *p == '\r' || *p == '\t') ? ' ' : *p);
    }
    if (strlen(text) > s->len) {
        g_string_append(s, "\xE2\x80\xA6"); /* ellipsis */
    }
    return g_string_free(s, FALSE);
}

static void position_window_near_pointer(void)
{
    GdkDisplay *display = gdk_display_get_default();
    if (!display) return;

    GdkSeat *seat = gdk_display_get_default_seat(display);
    GdkDevice *pointer = gdk_seat_get_pointer(seat);

    gint x = 0, y = 0;
    GdkScreen *screen = NULL;
    gdk_device_get_position(pointer, &screen, &x, &y);

    gint scr_w = gdk_screen_get_width(screen);
    gint scr_h = gdk_screen_get_height(screen);

    gint win_x = x - WINDOW_WIDTH / 2;
    gint win_y = y + 16;

    if (win_x < 0) win_x = 8;
    if (win_x + WINDOW_WIDTH > scr_w) win_x = scr_w - WINDOW_WIDTH - 8;
    if (win_y + WINDOW_HEIGHT > scr_h) win_y = y - WINDOW_HEIGHT - 16;
    if (win_y < 0) win_y = 8;

    gtk_window_move(GTK_WINDOW(g_window), win_x, win_y);
}

/* ---------- clipboard actions ---------- */

static void copy_item_to_clipboard(ClipboardItem *item)
{
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    storage_set_ignore_next(TRUE);

    if (item->type == ITEM_TEXT) {
        gtk_clipboard_set_text(clipboard, item->text, -1);
    } else {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(item->filepath, NULL);
        if (pixbuf) {
            gtk_clipboard_set_image(clipboard, pixbuf);
            g_object_unref(pixbuf);
        } else {
            storage_set_ignore_next(FALSE); /* nothing was actually set */
        }
    }
}

/* ---------- row click handling ---------- */

static gboolean on_row_button_press(GtkWidget *row_widget, GdkEventButton *event, gpointer user_data)
{
    (void)user_data;
    if (event->type != GDK_BUTTON_PRESS || event->button != 1) return FALSE;

    gboolean ctrl_held = (event->state & GDK_CONTROL_MASK) != 0;
    if (ctrl_held) {
        /* Let GtkListBox's own default handling perform the multi-select toggle. */
        return FALSE;
    }

    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row_widget), "item-id"));
    ClipboardItem *cur = storage_get_all();
    while (cur) {
        if (cur->id == id) {
            copy_item_to_clipboard(cur);
            break;
        }
        cur = cur->next;
    }

    /* Behave like a typical "clipboard history" popup: pick one, window gets
     * out of the way, then Ctrl+V wherever you like. */
    gtk_widget_hide(g_window);
    return FALSE; /* still allow normal single-row selection to be applied */
}

/* ---------- building rows ---------- */

static GtkWidget *build_row_for_item(ClipboardItem *item)
{
    GtkWidget *row = gtk_list_box_row_new();
    g_object_set_data(G_OBJECT(row), "item-id", GINT_TO_POINTER(item->id));
    gtk_widget_add_events(row, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(row, "button-press-event", G_CALLBACK(on_row_button_press), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);

    if (item->type == ITEM_IMAGE) {
        GdkPixbuf *thumb = gdk_pixbuf_new_from_file_at_scale(item->filepath, THUMB_SIZE, THUMB_SIZE, TRUE, NULL);
        GtkWidget *image = thumb ? gtk_image_new_from_pixbuf(thumb) : gtk_image_new_from_icon_name("image-missing", GTK_ICON_SIZE_DIALOG);
        if (thumb) g_object_unref(thumb);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

        GtkWidget *label = gtk_label_new("Image");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(label), "dim-label");
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    } else {
        char *preview = make_preview(item->text);
        GtkWidget *label = gtk_label_new(preview);
        g_free(preview);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    }

    gtk_container_add(GTK_CONTAINER(row), hbox);
    gtk_widget_show_all(row);
    return row;
}

static void refresh_list(void)
{
    /* Clear existing rows. */
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_listbox));
    for (GList *l = children; l; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    int count = 0;
    for (ClipboardItem *it = storage_get_all(); it; it = it->next) {
        GtkWidget *row = build_row_for_item(it);
        gtk_list_box_insert(GTK_LIST_BOX(g_listbox), row, -1);
        count++;
    }

    char count_text[64];
    snprintf(count_text, sizeof(count_text), "%d item%s", count, count == 1 ? "" : "s");
    gtk_label_set_text(GTK_LABEL(g_count_label), count_text);
}

/* ---------- button callbacks ---------- */

static void on_clear_all_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn; (void)user_data;
    storage_clear_all();
    refresh_list();
}

static void on_delete_selected_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn; (void)user_data;
    GList *rows = gtk_list_box_get_selected_rows(GTK_LIST_BOX(g_listbox));
    for (GList *l = rows; l; l = l->next) {
        int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(l->data), "item-id"));
        storage_delete(id);
    }
    g_list_free(rows);
    refresh_list();
}

static void on_copy_selected_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn; (void)user_data;
    GList *rows = gtk_list_box_get_selected_rows(GTK_LIST_BOX(g_listbox));
    if (!rows) return;

    /* If exactly one image is selected, copy the image itself.
     * Otherwise join all selected text items with newlines. */
    if (g_list_length(rows) == 1) {
        int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(rows->data), "item-id"));
        for (ClipboardItem *it = storage_get_all(); it; it = it->next) {
            if (it->id == id) { copy_item_to_clipboard(it); break; }
        }
    } else {
        GString *joined = g_string_new(NULL);
        for (GList *l = rows; l; l = l->next) {
            int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(l->data), "item-id"));
            for (ClipboardItem *it = storage_get_all(); it; it = it->next) {
                if (it->id == id && it->type == ITEM_TEXT) {
                    if (joined->len > 0) g_string_append_c(joined, '\n');
                    g_string_append(joined, it->text);
                    break;
                }
            }
        }
        if (joined->len > 0) {
            GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            storage_set_ignore_next(TRUE);
            gtk_clipboard_set_text(clipboard, joined->str, -1);
        }
        g_string_free(joined, TRUE);
    }

    g_list_free(rows);
    gtk_widget_hide(g_window);
}

static void on_expiry_changed(GtkSpinButton *spin, gpointer user_data)
{
    (void)user_data;
    g_timeout_minutes = gtk_spin_button_get_value_as_int(spin);
}

static gboolean on_window_focus_out(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)event; (void)user_data;
    gtk_widget_hide(widget);
    return FALSE;
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)event; (void)user_data;
    gtk_widget_hide(widget);
    return TRUE; /* don't destroy, just hide */
}

/* ---------- clipboard monitor ---------- */

static void on_clipboard_owner_change(GtkClipboard *clipboard, GdkEvent *event, gpointer user_data)
{
    (void)event; (void)user_data;

    if (storage_get_ignore_next()) {
        storage_set_ignore_next(FALSE);
        return;
    }

    if (gtk_clipboard_wait_is_text_available(clipboard)) {
        gchar *text = gtk_clipboard_wait_for_text(clipboard);
        if (text) {
            storage_add_text(text);
            g_free(text);
            if (gtk_widget_get_visible(g_window)) refresh_list();
        }
    } else if (gtk_clipboard_wait_is_image_available(clipboard)) {
        GdkPixbuf *pixbuf = gtk_clipboard_wait_for_image(clipboard);
        if (pixbuf) {
            storage_add_image(pixbuf);
            g_object_unref(pixbuf);
            if (gtk_widget_get_visible(g_window)) refresh_list();
        }
    }
}

static gboolean on_cleanup_timer(gpointer user_data)
{
    (void)user_data;
    if (storage_cleanup_expired(g_timeout_minutes)) {
        if (gtk_widget_get_visible(g_window)) refresh_list();
    }
    return G_SOURCE_CONTINUE;
}

/* ---------- hotkey callback ---------- */

static void on_hotkey_pressed(gpointer user_data)
{
    (void)user_data;
    ui_toggle_window(NULL);
}

void ui_toggle_window(gpointer user_data)
{
    (void)user_data;
    if (gtk_widget_get_visible(g_window)) {
        gtk_widget_hide(g_window);
    } else {
        refresh_list();
        position_window_near_pointer();
        gtk_widget_show_all(g_window);
        gtk_window_present(GTK_WINDOW(g_window));
    }
}

/* ---------- window construction ---------- */

void ui_init(void)
{
    storage_init();

    g_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_window), "Clipboard Manager");
    gtk_window_set_decorated(GTK_WINDOW(g_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(g_window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(g_window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(g_window), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_window_set_default_size(GTK_WINDOW(g_window), WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(g_window), 4);

    g_signal_connect(g_window, "focus-out-event", G_CALLBACK(on_window_focus_out), NULL);
    g_signal_connect(g_window, "delete-event", G_CALLBACK(on_window_delete), NULL);

    GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(g_window), root_box);

    /* --- top bar: title, expiry spinner, clear all --- */
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(root_box), top_bar, FALSE, FALSE, 4);

    GtkWidget *title = gtk_label_new("Clipboard");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title-4");
    gtk_box_pack_start(GTK_BOX(top_bar), title, FALSE, FALSE, 4);

    GtkWidget *expiry_label = gtk_label_new("Expire (min):");
    gtk_box_pack_start(GTK_BOX(top_bar), expiry_label, FALSE, FALSE, 0);

    g_spin_expiry = gtk_spin_button_new_with_range(1, 1440, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_spin_expiry), g_timeout_minutes);
    gtk_widget_set_tooltip_text(g_spin_expiry, "Items older than this are deleted automatically");
    g_signal_connect(g_spin_expiry, "value-changed", G_CALLBACK(on_expiry_changed), NULL);
    gtk_box_pack_start(GTK_BOX(top_bar), g_spin_expiry, FALSE, FALSE, 0);

    GtkWidget *clear_btn = gtk_button_new_with_label("Clear All");
    gtk_style_context_add_class(gtk_widget_get_style_context(clear_btn), "destructive-action");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_all_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(top_bar), clear_btn, FALSE, FALSE, 0);

    /* --- scrollable list --- */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(root_box), scroll, TRUE, TRUE, 0);

    g_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_listbox), GTK_SELECTION_MULTIPLE);
    gtk_container_add(GTK_CONTAINER(scroll), g_listbox);

    /* --- bottom bar: count + bulk actions --- */
    GtkWidget *bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(root_box), bottom_bar, FALSE, FALSE, 4);

    g_count_label = gtk_label_new("0 items");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_count_label), "dim-label");
    gtk_box_pack_start(GTK_BOX(bottom_bar), g_count_label, FALSE, FALSE, 4);

    GtkWidget *copy_sel_btn = gtk_button_new_with_label("Copy Selected");
    g_signal_connect(copy_sel_btn, "clicked", G_CALLBACK(on_copy_selected_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(bottom_bar), copy_sel_btn, FALSE, FALSE, 0);

    GtkWidget *delete_sel_btn = gtk_button_new_with_label("Delete Selected");
    g_signal_connect(delete_sel_btn, "clicked", G_CALLBACK(on_delete_selected_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(bottom_bar), delete_sel_btn, FALSE, FALSE, 0);

    /* --- wire up clipboard monitor, cleanup timer, and hotkey --- */
    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    g_signal_connect(clipboard, "owner-change", G_CALLBACK(on_clipboard_owner_change), NULL);

    g_timeout_add_seconds(15, on_cleanup_timer, NULL);

    if (!hotkey_start(on_hotkey_pressed, NULL)) {
        fprintf(stderr, "clipboard-manager: warning - could not register the Alt+L global hotkey.\n");
    }

    refresh_list();
    /* Window starts hidden; Alt+L (or ui_toggle_window) reveals it. */
}
