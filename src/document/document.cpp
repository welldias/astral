#include "document/document.h"

#include <fstream>
#include <sstream>

std::optional<Document> load_document(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();

  Document document;
  document.source_path = path;
  document.text = buffer.str();
  return document;
}
