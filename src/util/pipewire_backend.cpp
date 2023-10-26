#include "util/pipewire/pipewire_backend.hpp"

namespace waybar::util::PipewireBackend {

// TODO: Refresh on suspend wake
static void get_node_info(void *data_, const struct pw_node_info *info) {
  PrivacyNodeInfo *p_node_info = static_cast<PrivacyNodeInfo *>(data_);
  PipewireBackend *backend = (PipewireBackend *)p_node_info->data;

  p_node_info->state = info->state;

  const struct spa_dict_item *item;
  spa_dict_for_each(item, info->props) {
    if (strcmp(item->key, PW_KEY_CLIENT_ID) == 0) {
      p_node_info->client_id = strtoul(item->value, NULL, 10);
    } else if (strcmp(item->key, PW_KEY_MEDIA_CLASS) == 0) {
      p_node_info->media_class = item->value;
      if (strcmp(p_node_info->media_class.c_str(), "Stream/Input/Video") == 0) {
        p_node_info->type = PRIVACY_NODE_TYPE_VIDEO_INPUT;
      } else if (strcmp(p_node_info->media_class.c_str(), "Stream/Input/Audio") == 0) {
        p_node_info->type = PRIVACY_NODE_TYPE_AUDIO_INPUT;
      } else if (strcmp(p_node_info->media_class.c_str(), "Stream/Output/Audio") == 0) {
        p_node_info->type = PRIVACY_NODE_TYPE_AUDIO_OUTPUT;
      }
    } else if (strcmp(item->key, PW_KEY_MEDIA_NAME) == 0) {
      p_node_info->media_name = item->value;
    } else if (strcmp(item->key, PW_KEY_NODE_NAME) == 0) {
      p_node_info->node_name = item->value;
    }
  }

  if (p_node_info->type != PRIVACY_NODE_TYPE_NONE) {
    backend->mutex_.lock();
    p_node_info->changed = true;
    backend->privacy_nodes.insert_or_assign(info->id, p_node_info);
    backend->mutex_.unlock();

    backend->privacy_nodes_changed_signal_event.emit();
  } else {
    if (p_node_info->changed) {
      backend->mutex_.lock();
      PrivacyNodeInfo *node = backend->privacy_nodes.at(info->id);
      delete node;
      backend->privacy_nodes.erase(info->id);
      backend->mutex_.unlock();

      backend->privacy_nodes_changed_signal_event.emit();
    }
  }
}

static const struct pw_node_events node_events = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = get_node_info,
};

static void registry_event_global(void *_data, uint32_t id, uint32_t permissions, const char *type,
                                  uint32_t version, const struct spa_dict *props) {
  if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0) return;

  PipewireBackend *backend = static_cast<PipewireBackend *>(_data);
  struct pw_proxy *proxy = (pw_proxy *)pw_registry_bind(backend->registry, id, type, version, 0);
  if (proxy) {
    PrivacyNodeInfo *p_node_info;
    backend->mutex_.lock();
    if (backend->privacy_nodes.contains(id)) {
      p_node_info = backend->privacy_nodes.at(id);
    } else {
      p_node_info = new PrivacyNodeInfo(id, backend);
    }
    backend->mutex_.unlock();
    pw_proxy_add_object_listener(proxy, &p_node_info->node_listener, &node_events, p_node_info);
  }
}

static void registry_event_global_remove(void *_data, uint32_t id) {
  auto backend = static_cast<PipewireBackend *>(_data);

  backend->mutex_.lock();
  if (backend->privacy_nodes.contains(id)) {
    PrivacyNodeInfo *node_info = backend->privacy_nodes.at(id);
    delete node_info;
    backend->privacy_nodes.erase(id);
  }
  backend->mutex_.unlock();

  backend->privacy_nodes_changed_signal_event.emit();
}

static const struct pw_registry_events registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
    .global_remove = registry_event_global_remove,
};

PipewireBackend::PipewireBackend(private_constructor_tag tag)
    : mainloop_(nullptr), context_(nullptr), core_(nullptr) {
  pw_init(nullptr, nullptr);
  mainloop_ = pw_thread_loop_new("waybar", nullptr);
  if (mainloop_ == nullptr) {
    throw std::runtime_error("pw_thread_loop_new() failed.");
  }
  context_ = pw_context_new(pw_thread_loop_get_loop(mainloop_), nullptr, 0);
  if (context_ == nullptr) {
    throw std::runtime_error("pa_context_new() failed.");
  }
  core_ = pw_context_connect(context_, nullptr, 0);
  if (core_ == nullptr) {
    throw std::runtime_error("pw_context_connect() failed");
  }
  registry = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);

  spa_zero(registry_listener);
  pw_registry_add_listener(registry, &registry_listener, &registry_events, this);
  if (pw_thread_loop_start(mainloop_) < 0) {
    throw std::runtime_error("pw_thread_loop_start() failed.");
  }
}

PipewireBackend::~PipewireBackend() {
  for (auto &node : privacy_nodes) {
    delete node.second;
  }

  if (registry != nullptr) {
    pw_proxy_destroy((struct pw_proxy *)registry);
  }

  spa_zero(registry_listener);

  if (core_ != nullptr) {
    pw_core_disconnect(core_);
  }

  if (context_ != nullptr) {
    pw_context_destroy(context_);
  }

  if (mainloop_ != nullptr) {
    pw_thread_loop_stop(mainloop_);
    pw_thread_loop_destroy(mainloop_);
  }
}

std::shared_ptr<PipewireBackend> PipewireBackend::getInstance() {
  private_constructor_tag tag;
  return std::make_shared<PipewireBackend>(tag);
}
}  // namespace waybar::util::PipewireBackend
