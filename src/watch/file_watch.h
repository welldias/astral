#pragma once

#include <filesystem>
#include <string>

struct FileWatchState {
  std::string path;
  std::filesystem::file_time_type last_write_time;     // last mtime that already triggered a reload
  std::filesystem::file_time_type pending_write_time;   // mtime observed on the last check
  bool has_pending;
  double last_check_time;  // GetTime() of the last check, for throttling
};

FileWatchState make_file_watch_state(const std::string& path);

// Checks the file's mtime (at most every ~300ms). Returns true only when
// a "settled" change is detected (the same mtime observed on two
// consecutive checks), avoiding reloading a file that's being written
// halfway through.
bool poll_file_watch(FileWatchState& state, double now);
