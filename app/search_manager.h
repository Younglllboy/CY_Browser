#pragma once

#include <string>

// Search provider selection is shared by every browser window.
class SearchManager {
 public:
  enum class Engine { Bing, Baidu, Google, DuckDuckGo, Firefox };

  static SearchManager& Get();
  Engine engine() const { return engine_; }
  void SetEngine(Engine engine);
  const char* EngineName() const;
  const char* HomeURL() const;
  std::string BuildSearchURL(const std::wstring& query) const;

 private:
  SearchManager();
  Engine engine_ = Engine::Bing;
};
