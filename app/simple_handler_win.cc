// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "simple_handler.h"

#include <windows.h>

#include <string>
#include <shellapi.h>

#include "include/cef_browser.h"

namespace {
bool SetRegistryString(const wchar_t* key, const wchar_t* name,
                       const std::wstring& value) {
  return RegSetKeyValueW(HKEY_CURRENT_USER, key, name, REG_SZ, value.c_str(),
                         static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}
}  // namespace

void OpenDefaultBrowserSettings() {
  wchar_t executable[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, executable, MAX_PATH)) return;
  const std::wstring command = L"\"" + std::wstring(executable) + L"\" \"%1\"";
  const std::wstring icon = L"\"" + std::wstring(executable) + L"\",0";
  const wchar_t* protocol = L"Software\\Classes\\CYBROWSER.Url";
  const wchar_t* html = L"Software\\Classes\\CYBROWSER.HTML";
  const wchar_t* pdf = L"Software\\Classes\\CYBROWSER.PDF";
  const wchar_t* capabilities = L"Software\\CYBROWSER\\Capabilities";
  const bool registered =
      SetRegistryString(protocol, nullptr, L"CYBROWSER URL") &&
      SetRegistryString(L"Software\\Classes\\CYBROWSER.Url\\DefaultIcon", nullptr, icon) &&
      SetRegistryString(L"Software\\Classes\\CYBROWSER.Url\\shell\\open\\command", nullptr, command) &&
      SetRegistryString(html, nullptr, L"CYBROWSER HTML Document") &&
      SetRegistryString(L"Software\\Classes\\CYBROWSER.HTML\\DefaultIcon", nullptr, icon) &&
      SetRegistryString(L"Software\\Classes\\CYBROWSER.HTML\\shell\\open\\command", nullptr, command) &&
      SetRegistryString(pdf, nullptr, L"CYBROWSER PDF Document") &&
      SetRegistryString(L"Software\\Classes\\CYBROWSER.PDF\\DefaultIcon", nullptr, icon) &&
      SetRegistryString(L"Software\\Classes\\CYBROWSER.PDF\\shell\\open\\command", nullptr, command) &&
      SetRegistryString(capabilities, L"ApplicationName", L"CYBROWSER") &&
      SetRegistryString(capabilities, L"ApplicationDescription", L"CYBROWSER 网页浏览器") &&
      SetRegistryString(L"Software\\CYBROWSER\\Capabilities\\URLAssociations", L"http", L"CYBROWSER.Url") &&
      SetRegistryString(L"Software\\CYBROWSER\\Capabilities\\URLAssociations", L"https", L"CYBROWSER.Url") &&
      SetRegistryString(L"Software\\CYBROWSER\\Capabilities\\FileAssociations", L".htm", L"CYBROWSER.HTML") &&
      SetRegistryString(L"Software\\CYBROWSER\\Capabilities\\FileAssociations", L".html", L"CYBROWSER.HTML") &&
      SetRegistryString(L"Software\\CYBROWSER\\Capabilities\\FileAssociations", L".pdf", L"CYBROWSER.PDF") &&
      SetRegistryString(L"Software\\RegisteredApplications", L"CYBROWSER", capabilities);
  if (!registered) {
    MessageBoxW(nullptr, L"无法注册 CYBROWSER。请检查当前用户的注册表权限。",
                L"默认浏览器", MB_OK | MB_ICONERROR);
    return;
  }
  ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr,
                SW_SHOWNORMAL);
}

void SimpleHandler::PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                                        const CefString& title) {
  CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
  if (hwnd) {
    SetWindowText(hwnd, std::wstring(title).c_str());
  }
}
