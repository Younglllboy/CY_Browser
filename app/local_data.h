#pragma once

#include <filesystem>
#include <windows.h>

inline std::filesystem::path LocalDataPath() {
  wchar_t path[MAX_PATH] = {};
  const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
  if (length > 0 && length < MAX_PATH)
    return std::filesystem::path(path) / L"CYBROWSER";
  return std::filesystem::temp_directory_path() / L"CYBROWSER";
}
