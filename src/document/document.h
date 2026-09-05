#pragma once

#include <optional>
#include <string>

struct Document {
  std::string source_path;
  std::string text;  // raw file content, in UTF-8
};

// Reads the entire file at `path`. Returns std::nullopt if the file
// doesn't exist or can't be read; an empty file is valid (empty text).
std::optional<Document> load_document(const std::string& path);
