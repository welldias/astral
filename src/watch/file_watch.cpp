#include "watch/file_watch.h"

#include <system_error>

namespace {
constexpr double kPollIntervalSeconds = 0.3;
}

FileWatchState make_file_watch_state(const std::string& path) {
  FileWatchState state;
  state.path = path;

  std::error_code error;
  state.last_write_time = std::filesystem::last_write_time(path, error);
  state.pending_write_time = state.last_write_time;
  state.has_pending = false;
  state.last_check_time = 0.0;

  return state;
}

bool poll_file_watch(FileWatchState& state, double now) {
  if (now - state.last_check_time < kPollIntervalSeconds) {
    return false;
  }
  state.last_check_time = now;

  std::error_code error;
  std::filesystem::file_time_type current_write_time = std::filesystem::last_write_time(state.path, error);
  if (error) {
    return false;
  }

  bool changed_since_last_reload = current_write_time != state.last_write_time;

  if (state.has_pending && current_write_time == state.pending_write_time && changed_since_last_reload) {
    state.last_write_time = current_write_time;
    state.has_pending = false;
    return true;
  }

  state.pending_write_time = current_write_time;
  state.has_pending = changed_since_last_reload;
  return false;
}
