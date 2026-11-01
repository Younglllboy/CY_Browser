// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include <list>

#include "include/cef_client.h"
#include "include/cef_request_handler.h"
#include "include/cef_download_handler.h"
#include <vector>
#include <string>
#include <map>

class SimpleHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefRequestHandler,
                      public CefDownloadHandler {
 public:
  explicit SimpleHandler(bool is_alloy_style);
  ~SimpleHandler() override;

  // Provide access to the single global instance of this object.
  static SimpleHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request, bool user_gesture,
                      bool is_redirect) override;
  bool OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDownloadItem> item,
                        const CefString& suggested_name,
                        CefRefPtr<CefBeforeDownloadCallback> callback) override;
  void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefDownloadItem> item,
                         CefRefPtr<CefDownloadItemCallback> callback) override;
  void OpenManager(CefRefPtr<CefBrowser> browser, const std::string& section);
  bool OpenURL(const std::string& url);
  std::vector<std::pair<int,std::string>> BookmarkFolders() const;
  struct Shortcut { std::string url, title, icon; };
  std::vector<Shortcut> Shortcuts() const;
  bool IsBookmarked(const std::string& url) const;
  void AddBookmark(CefRefPtr<CefBrowser> browser, int folder_id);
  void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                          const std::vector<CefString>& icon_urls) override;

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading,
                            bool canGoBack, bool canGoForward) override;
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override;

  void ShowMainWindow();

  // Request that all existing browser windows close.
  void CloseAllBrowsers(bool force_close);

  bool IsClosing() const { return is_closing_; }

 private:
  // Platform-specific implementation.
  void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title);
  void PlatformShowWindow(CefRefPtr<CefBrowser> browser);

  // True if this client is Alloy style, otherwise Chrome style.
  const bool is_alloy_style_;

  // List of existing browser windows. Only accessed on the CEF UI thread.
  typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
  BrowserList browser_list_;

  bool is_closing_ = false;
  std::vector<std::pair<std::string, std::string>> history_;
  struct Folder { int id; std::string name; };
  struct Bookmark { int id, folder_id; std::string url, title, icon; };
  std::vector<Folder> folders_;
  std::vector<Bookmark> bookmarks_;
  int next_folder_id_=1, next_bookmark_id_=1;
  std::map<int,std::string> page_titles_, page_icons_;
  void LoadBookmarks();
  void SaveBookmarks();
  struct Download { uint64_t id; uint32_t cef_id; std::string name, path, url; int percent; bool complete, canceled; CefRefPtr<CefDownloadItemCallback> callback; };
  std::vector<Download> downloads_;
  uint64_t next_download_id_=1;
  void LoadDownloads();
  void SaveDownloads();
  std::string manager_section_ = "history";
  void SaveHistory();
  void LoadHistory();
  void RenderManager(CefRefPtr<CefBrowser> browser);

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
