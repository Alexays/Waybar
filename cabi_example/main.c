#include <gtk/gtk.h>
#include <stdint.h>

typedef struct {
  GtkContainer* root;
  GtkBox* container;
  GtkLabel* label;
  GtkButton* button;
} Instance;

//
const size_t wbcabi_version = 1;

void onclicked() { printf("You clicked the button\n"); }

void* wbcabi_init(GtkContainer* root) {
  // Allocate the instance object
  Instance* inst = malloc(sizeof(Instance));
  inst->root = root;

  // Add a container for displaying the next widgets
  inst->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
  gtk_container_add(GTK_CONTAINER(inst->root), GTK_WIDGET(inst->container));

  // Add a label
  inst->label = GTK_LABEL(gtk_label_new("This is a button:"));
  gtk_container_add(GTK_CONTAINER(inst->container), GTK_WIDGET(inst->label));

  // Add a button
  inst->button = GTK_BUTTON(gtk_button_new_with_label("click me !"));
  g_signal_connect(inst->button, "clicked", G_CALLBACK(onclicked), NULL);
  gtk_container_add(GTK_CONTAINER(inst->container), GTK_WIDGET(inst->button));

  // Return instance object
  return inst;
}
void wbcabi_deinit(void* instance) { free(instance); }

char* wbcabi_last_error_str(void* instance) { return NULL; }