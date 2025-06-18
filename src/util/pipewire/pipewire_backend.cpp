#include "util/pipewire/pipewire_backend.hpp"

#include "util/pipewire/privacy_node_info.hpp"

namespace waybar::util::PipewireBackend {

static void getNodeInfo(void *data_, const struct pw_node_info *info) {
  auto *pNodeInfo = static_cast<PrivacyNodeInfo *>(data_);
  pNodeInfo->handleNodeEventInfo(info);

  static_cast<PipewireBackend *>(pNodeInfo->data)->privacy_nodes_changed_signal_event.emit();
}

static const struct pw_node_events NODE_EVENTS = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = getNodeInfo,
};

static void proxyDestroy(void *data) {
  static_cast<PrivacyNodeInfo *>(data)->handleProxyEventDestroy();
}

static const struct pw_proxy_events PROXY_EVENTS = {
    .version = PW_VERSION_PROXY_EVENTS,
    .destroy = proxyDestroy,
};

static void registryEventGlobal(void *_data, uint32_t id, uint32_t permissions, const char *type,
                                uint32_t version, const struct spa_dict *props) {
  static_cast<PipewireBackend *>(_data)->handleRegistryEventGlobal(id, permissions, type, version,
                                                                   props);
}

static void registryEventGlobalRemove(void *_data, uint32_t id) {
  static_cast<PipewireBackend *>(_data)->handleRegistryEventGlobalRemove(id);
}

static const struct pw_registry_events REGISTRY_EVENTS = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = registryEventGlobal,
    .global_remove = registryEventGlobalRemove,
};

PipewireBackend::PipewireBackend(PrivateConstructorTag tag)
    : mainloop_(nullptr), context_(nullptr), core_(nullptr) {
  pw_init(nullptr, nullptr);
  mainloop_ = pw_thread_loop_new("waybar", nullptr);
  if (mainloop_ == nullptr) {
    throw std::runtime_error("pw_thread_loop_new() failed.");
  }

  pw_thread_loop_lock(mainloop_);

  context_ = pw_context_new(pw_thread_loop_get_loop(mainloop_), nullptr, 0);
  if (context_ == nullptr) {
    pw_thread_loop_unlock(mainloop_);
    throw std::runtime_error("pa_context_new() failed.");
  }
  core_ = pw_context_connect(context_, nullptr, 0);
  if (core_ == nullptr) {
    pw_thread_loop_unlock(mainloop_);
    throw std::runtime_error("pw_context_connect() failed");
  }
  registry_ = pw_core_get_registry(core_, PW_VERSION_REGISTRY, 0);

  spa_zero(registryListener_);
  pw_registry_add_listener(registry_, &registryListener_, &REGISTRY_EVENTS, this);
  if (pw_thread_loop_start(mainloop_) < 0) {
    pw_thread_loop_unlock(mainloop_);
    throw std::runtime_error("pw_thread_loop_start() failed.");
  }
  pw_thread_loop_unlock(mainloop_);
}

PipewireBackend::~PipewireBackend() {
  if (mainloop_ != nullptr) {
    pw_thread_loop_lock(mainloop_);
  }

  if (registry_ != nullptr) {
    pw_proxy_destroy((struct pw_proxy *)registry_);
  }

  spa_zero(registryListener_);

  if (core_ != nullptr) {
    pw_core_disconnect(core_);
  }

  if (context_ != nullptr) {
    pw_context_destroy(context_);
  }

  if (mainloop_ != nullptr) {
    pw_thread_loop_unlock(mainloop_);
    pw_thread_loop_stop(mainloop_);
    pw_thread_loop_destroy(mainloop_);
  }
}

std::shared_ptr<PipewireBackend> PipewireBackend::getInstance() {
  PrivateConstructorTag tag;
  return std::make_shared<PipewireBackend>(tag);
}

void PipewireBackend::handleRegistryEventGlobal(uint32_t id, uint32_t permissions, const char *type,
                                                uint32_t version, const struct spa_dict *props) {
  if (props == nullptr || strcmp(type, PW_TYPE_INTERFACE_Node) != 0) return;

  const char *lookupStr = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
  if (lookupStr == nullptr) return;
  std::string mediaClass = lookupStr;
  enum PrivacyNodeType mediaType = PRIVACY_NODE_TYPE_NONE;
  if (mediaClass == "Stream/Input/Video") {
    mediaType = PRIVACY_NODE_TYPE_VIDEO_INPUT;
  } else if (mediaClass == "Stream/Input/Audio") {
    mediaType = PRIVACY_NODE_TYPE_AUDIO_INPUT;
  } else if (mediaClass == "Stream/Output/Audio") {
    mediaType = PRIVACY_NODE_TYPE_AUDIO_OUTPUT;
  } else {
    return;
  }

  auto *proxy = (pw_proxy *)pw_registry_bind(registry_, id, type, version, sizeof(PrivacyNodeInfo));

  if (proxy == nullptr) return;

  auto *pNodeInfo = (PrivacyNodeInfo *)pw_proxy_get_user_data(proxy);
  new (pNodeInfo) PrivacyNodeInfo{};
  pNodeInfo->id = id;
  pNodeInfo->data = this;
  pNodeInfo->type = mediaType;
  pNodeInfo->media_class = mediaClass;

  pw_proxy_add_listener(proxy, &pNodeInfo->proxy_listener, &PROXY_EVENTS, pNodeInfo);

  pw_proxy_add_object_listener(proxy, &pNodeInfo->object_listener, &NODE_EVENTS, pNodeInfo);

  privacy_nodes.insert_or_assign(id, pNodeInfo);
}

void PipewireBackend::handleRegistryEventGlobalRemove(uint32_t id) {
  mutex_.lock();
  auto iter = privacy_nodes.find(id);
  if (iter != privacy_nodes.end()) {
    privacy_nodes[id]->~PrivacyNodeInfo();
    privacy_nodes.erase(id);
  }
  mutex_.unlock();

  privacy_nodes_changed_signal_event.emit();
}

}  // namespace waybar::util::PipewireBackend
