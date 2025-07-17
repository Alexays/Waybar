
#include "waybar_cffi_module.h"

typedef struct {
  wbcffi_module* waybar_module;
  GtkBox* container;
  GtkButton* button;
} ExampleMod;

// This static variable is shared between all instances of this module
static int instance_count = 0;

void onclicked(GtkButton* button) {
  char text[256];
  snprintf(text, 256, "Dice throw result: %d", rand() % 6 + 1);
  gtk_button_set_label(button, text);
}

// You must
const size_t wbcffi_version = 2;

void* wbcffi_init(const wbcffi_init_info* init_info, const wbcffi_config_entry* config_entries,
                  size_t config_entries_len) {
  printf("cffi_example: init config:\n");
  for (size_t i = 0; i < config_entries_len; i++) {
    printf("  %s = %s\n", config_entries[i].key, config_entries[i].value);
  }

  // Allocate the instance object
  ExampleMod* inst = malloc(sizeof(ExampleMod));
  inst->waybar_module = init_info->obj;

  GtkContainer* root = init_info->get_root_widget(init_info->obj);

  // Add a container for displaying the next widgets
  inst->container = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5));
  gtk_container_add(GTK_CONTAINER(root), GTK_WIDGET(inst->container));

  // Add a label
  GtkLabel* label = GTK_LABEL(gtk_label_new("[Example C FFI Module:"));
  gtk_container_add(GTK_CONTAINER(inst->container), GTK_WIDGET(label));

  // Add a button
  inst->button = GTK_BUTTON(gtk_button_new_with_label("click me !"));
  g_signal_connect(inst->button, "clicked", G_CALLBACK(onclicked), NULL);
  gtk_container_add(GTK_CONTAINER(inst->container), GTK_WIDGET(inst->button));

  // Add a label
  label = GTK_LABEL(gtk_label_new("]"));
  gtk_container_add(GTK_CONTAINER(inst->container), GTK_WIDGET(label));

  // Return instance object
  printf("cffi_example inst=%p: init success ! (%d total instances)\n", inst, ++instance_count);
  return inst;
}

void wbcffi_deinit(void* instance) {
  printf("cffi_example inst=%p: free memory\n", instance);
  free(instance);
}

void wbcffi_update(void* instance) { printf("cffi_example inst=%p: Update request\n", instance); }

void wbcffi_refresh(void* instance, int signal) {
  printf("cffi_example inst=%p: Received refresh signal %d\n", instance, signal);
}

void wbcffi_doaction(void* instance, const char* name) {
  printf("cffi_example inst=%p: doAction(%s)\n", instance, name);
}
