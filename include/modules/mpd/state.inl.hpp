#pragma once

namespace detail {

inline bool Context::is_connected() const { return mpd_module_->connection_ != nullptr; }
inline bool Context::is_playing() const { return mpd_module_->playing(); }
inline bool Context::is_paused() const { return mpd_module_->paused(); }
inline bool Context::is_stopped() const { return mpd_module_->stopped(); }

constexpr inline std::size_t Context::interval() const { return mpd_module_->interval_.count(); }
inline void Context::tryConnect() const { mpd_module_->tryConnect(); }
inline unique_connection& Context::connection() { return mpd_module_->connection_; }
constexpr inline mpd_state Context::state() const { return mpd_module_->state_; }

inline void Context::do_update() { mpd_module_->setLabel(); }

inline void Context::checkErrors(mpd_connection* conn) const { mpd_module_->checkErrors(conn); }
inline void Context::queryMPD() const { mpd_module_->queryMPD(); }
inline void Context::fetchState() const { mpd_module_->fetchState(); }
inline void Context::emit() const { mpd_module_->emit(); }

}  // namespace detail
