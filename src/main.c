#include <gtk/gtk.h>
#include "ui.h"

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    ui_init();

    gtk_main();
    return 0;
}
