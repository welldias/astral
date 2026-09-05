// NOTE: this file is only compiled in Windows builds. It could not be
// compiled/tested in the Linux development environment used for this
// project — validate it on a real Windows machine before relying on it.

#include "platform/default_font.h"

#include <windows.h>
#include <shlobj.h>

#include <iterator>

namespace {

std::string narrow(const std::wstring& wide) {
  if (wide.empty()) {
    return "";
  }
  int size =
      WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), result.data(), size, nullptr,
                       nullptr);
  return result;
}

std::string find_font_file_by_face_name(const std::wstring& face_name) {
  HKEY fonts_key;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ,
                     &fonts_key) != ERROR_SUCCESS) {
    return "";
  }

  std::string result;
  wchar_t value_name[512];
  BYTE value_data[1024];
  DWORD index = 0;

  while (true) {
    DWORD value_name_size = static_cast<DWORD>(std::size(value_name));
    DWORD value_data_size = static_cast<DWORD>(sizeof(value_data));
    DWORD type = 0;

    LONG status =
        RegEnumValueW(fonts_key, index, value_name, &value_name_size, nullptr, &type, value_data, &value_data_size);
    if (status == ERROR_NO_MORE_ITEMS) {
      break;
    }

    if (status == ERROR_SUCCESS && type == REG_SZ) {
      std::wstring name(value_name, value_name_size);
      if (name.find(face_name) != std::wstring::npos) {
        std::wstring file_name(reinterpret_cast<wchar_t*>(value_data), value_data_size / sizeof(wchar_t));
        while (!file_name.empty() && file_name.back() == L'\0') {
          file_name.pop_back();
        }

        wchar_t fonts_dir[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, 0, fonts_dir) == S_OK) {
          result = narrow(std::wstring(fonts_dir) + L"\\" + file_name);
        }
        break;
      }
    }

    ++index;
  }

  RegCloseKey(fonts_key);
  return result;
}

std::wstring system_ui_face_name() {
  NONCLIENTMETRICSW metrics;
  metrics.cbSize = sizeof(metrics);
  if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
    return L"";
  }
  return std::wstring(metrics.lfMessageFont.lfFaceName);
}

// Font registry entries are usually named "<Family> Bold Italic
// (TrueType)" etc.; we look for exactly that variant (via `suffix`, empty
// for the regular font) and don't accept another one as a substitute.
std::string find_variant(const wchar_t* suffix) {
  std::wstring face_name = system_ui_face_name();
  if (face_name.empty()) {
    return "";
  }
  return find_font_file_by_face_name(face_name + suffix);
}

}  // namespace

std::string get_system_default_font_path() {
  return find_variant(L"");
}

std::string get_system_bold_font_path() {
  return find_variant(L" Bold");
}

std::string get_system_italic_font_path() {
  return find_variant(L" Italic");
}

std::string get_system_bold_italic_font_path() {
  return find_variant(L" Bold Italic");
}

std::string get_system_monospace_font_path() {
  // There's no monospace equivalent of SPI_GETNONCLIENTMETRICS; we try
  // the most common Windows monospace fonts, in order of preference
  // (newest/most legible first).
  static const wchar_t* kCandidates[] = {L"Cascadia Mono", L"Consolas", L"Courier New"};

  for (const wchar_t* candidate : kCandidates) {
    std::string path = find_font_file_by_face_name(candidate);
    if (!path.empty()) {
      return path;
    }
  }
  return "";
}
