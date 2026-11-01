// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_

#include "include/cef_app.h"

class CefBrowser;
void UpdateAddressBar(CefRefPtr<CefBrowser> browser, const CefString& url);
void UpdateNavigationButtons(CefRefPtr<CefBrowser> browser, bool can_go_back,
                             bool can_go_forward);
void UpdateBookmarkButton(CefRefPtr<CefBrowser> browser, const CefString& url);
void UpdateSearchEngineButton(CefRefPtr<CefBrowser> browser);
void UpdateShortcutBar();
void UpdateTabTitle(CefRefPtr<CefBrowser> browser, const CefString& title);
void UpdateTabIcon(CefRefPtr<CefBrowser> browser, const CefString& icon_url);
void NotifyTabClosed(CefRefPtr<CefBrowser> browser);
bool HasMainWindow();

// Implement application-level callbacks for the browser process.
class SimpleApp : public CefApp, public CefBrowserProcessHandler {
 public:
  SimpleApp();

  // CefApp methods:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler methods:
  void OnContextInitialized() override;
  bool OnAlreadyRunningAppRelaunch(
      CefRefPtr<CefCommandLine> command_line,
      const CefString& current_directory) override;
  CefRefPtr<CefClient> GetDefaultClient() override;

 private:
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(SimpleApp);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
