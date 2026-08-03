#include "hotkey.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static pthread_t g_thread;
static gboolean g_thread_running = FALSE;
static gboolean g_should_stop = FALSE;
static Display *g_display = NULL;

typedef struct {
    HotkeyCallback cb;
    gpointer user_data;
} IdlePayload;

static gboolean idle_dispatch(gpointer data)
{
    IdlePayload *p = (IdlePayload *)data;
    p->cb(p->user_data);
    g_free(p);
    return G_SOURCE_REMOVE;
}

/* Grab a key with every combination of the "ignorable" lock modifiers
 * (NumLock = Mod2, CapsLock = Lock) so the hotkey still fires no matter
 * what lock keys happen to be toggled on. */
static void grab_with_all_locks(Display *dpy, Window root, int keycode, unsigned int base_mask)
{
    unsigned int lock_combos[] = { 0, Mod2Mask, LockMask, Mod2Mask | LockMask };
    for (int i = 0; i < 4; i++) {
        XGrabKey(dpy, keycode, base_mask | lock_combos[i], root,
                  True, GrabModeAsync, GrabModeAsync);
    }
}

static void *hotkey_thread_main(void *arg)
{
    IdlePayload *template = (IdlePayload *)arg;

    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "clipboard-manager: could not open X display for hotkey listener "
                        "(are you running under X11? Wayland is not supported).\n");
        g_free(template);
        return NULL;
    }

    Window root = DefaultRootWindow(g_display);
    KeyCode keycode = XKeysymToKeycode(g_display, XK_l);
    if (keycode == 0) {
        fprintf(stderr, "clipboard-manager: could not map 'l' key to a keycode.\n");
        XCloseDisplay(g_display);
        g_free(template);
        return NULL;
    }

    grab_with_all_locks(g_display, root, keycode, Mod1Mask); /* Mod1Mask = Alt */

    XSelectInput(g_display, root, KeyPressMask);
    g_thread_running = TRUE;

    XEvent ev;
    while (!g_should_stop) {
        /* Poll with a timeout so we can notice g_should_stop without blocking forever. */
        if (XPending(g_display) > 0) {
            XNextEvent(g_display, &ev);
            if (ev.type == KeyPress && ev.xkey.keycode == keycode) {
                IdlePayload *p = g_new(IdlePayload, 1);
                p->cb = template->cb;
                p->user_data = template->user_data;
                g_idle_add(idle_dispatch, p);
            }
        } else {
            struct timespec ts = { 0, 50 * 1000 * 1000 }; /* 50ms */
            nanosleep(&ts, NULL);
        }
    }

    XUngrabKey(g_display, keycode, AnyModifier, root);
    XCloseDisplay(g_display);
    g_free(template);
    g_thread_running = FALSE;
    return NULL;
}

gboolean hotkey_start(HotkeyCallback cb, gpointer user_data)
{
    if (g_thread_running) return TRUE;

    IdlePayload *template = g_new(IdlePayload, 1);
    template->cb = cb;
    template->user_data = user_data;

    g_should_stop = FALSE;
    if (pthread_create(&g_thread, NULL, hotkey_thread_main, template) != 0) {
        g_free(template);
        return FALSE;
    }
    pthread_detach(g_thread);
    return TRUE;
}

void hotkey_stop(void)
{
    g_should_stop = TRUE;
}
