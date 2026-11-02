#include "search_manager.h"

#include "include/cef_parser.h"
#include <windows.h>
#include <fstream>
#include <filesystem>
#include "local_data.h"

namespace {
std::filesystem::path SettingsPath() {
  auto dir=LocalDataPath();
  std::filesystem::create_directories(dir);
  return dir/L"settings.txt";
}
}

SearchManager::SearchManager() {
  int value=0;
  if (std::ifstream(SettingsPath()) >> value && value>=0 && value<=5)
    engine_=static_cast<Engine>(value);
}

SearchManager& SearchManager::Get() {
  static SearchManager instance;
  return instance;
}

void SearchManager::SetEngine(Engine engine) {
  engine_ = engine;
  std::ofstream(SettingsPath(),std::ios::trunc) << static_cast<int>(engine);
}

const char* SearchManager::HomeURL() const {
  switch (engine_) {
    case Engine::Baidu: return "https://www.baidu.com/";
    case Engine::Google: return "https://www.google.com/";
    case Engine::DuckDuckGo: return "https://duckduckgo.com/";
    case Engine::Firefox: return "https://www.google.com/";
    case Engine::ZhijiDiscovery: return "https://laptop-s5mos3ql.tail00db87.ts.net/";
    default: return "https://www.bing.com/";
  }
}

const char* SearchManager::EngineName() const {
  switch (engine_) {
    case Engine::Baidu: return "Baidu";
    case Engine::Google: return "Google";
    case Engine::DuckDuckGo: return "DuckDuckGo";
    case Engine::Firefox: return "Firefox (Google)";
    case Engine::ZhijiDiscovery: return "枝己·发现";
    default: return "Bing";
  }
}

std::string SearchManager::BuildSearchURL(const std::wstring& query) const {
  const std::string encoded = CefURIEncode(CefString(query), true).ToString();
  switch (engine_) {
    case Engine::Baidu: return "https://www.baidu.com/s?wd=" + encoded;
    case Engine::Google: return "https://www.google.com/search?q=" + encoded;
    case Engine::DuckDuckGo: return "https://duckduckgo.com/?q=" + encoded;
    case Engine::Firefox: return "https://www.google.com/search?client=firefox-b-d&q=" + encoded;
    case Engine::ZhijiDiscovery: return "https://laptop-s5mos3ql.tail00db87.ts.net/?q=" + encoded;
    default: return "https://www.bing.com/search?q=" + encoded;
  }
}
