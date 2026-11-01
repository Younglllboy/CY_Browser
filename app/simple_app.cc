// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "simple_app.h"

#include <string>
#include <algorithm>
#include <cwctype>
#include <vector>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/views/cef_menu_button.h"
#include "include/cef_menu_model.h"
#include "include/cef_parser.h"
#include "include/cef_image.h"
#include "include/cef_task_manager.h"
#include "resource.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"
#include "simple_handler.h"
#include "search_manager.h"

namespace {

class SimpleWindowDelegate;
std::vector<SimpleWindowDelegate*> g_windows;

CefRefPtr<CefImage> LoadToolbarIcon(int id) {
  HMODULE module = GetModuleHandleW(L"CYBROWSER.dll");
  if (!module) module = GetModuleHandleW(nullptr);
  HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!resource) return nullptr;
  HGLOBAL data = LoadResource(module, resource);
  if (!data) return nullptr;
  auto image = CefImage::CreateImage();
  if (!image->AddPNG(1.0f, LockResource(data), SizeofResource(module, resource)))
    return nullptr;
  return image;
}

CefRefPtr<CefLabelButton> MakeIconButton(CefRefPtr<CefButtonDelegate> delegate,
                                        int resource_id, const char* name) {
  auto button = CefLabelButton::CreateLabelButton(delegate, "");
  button->SetImage(CEF_BUTTON_STATE_NORMAL, LoadToolbarIcon(resource_id));
  button->SetTooltipText(name);
  button->SetAccessibleName(name);
  button->SetMinimumSize(CefSize(38, 36));
  return button;
}

// Route plain words to search; accept schemes and likely hosts as addresses.
std::string ResolveInput(const CefString& input) {
  std::wstring value = input.ToWString();
  const auto first = value.find_first_not_of(L" \t\r\n");
  if (first == std::wstring::npos) return {};
  const auto last = value.find_last_not_of(L" \t\r\n");
  value = value.substr(first, last - first + 1);
  std::wstring lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
  const bool scheme = lower.find(L"://") != std::wstring::npos;
  if (scheme) {
    const auto prefix = lower.substr(0, lower.find(L"://"));
    if (prefix == L"http" || prefix == L"https" || prefix == L"file")
      return CefString(value).ToString();
  }
  const bool spaces = value.find_first_of(L" \t\r\n") != std::wstring::npos;
  const bool host = !spaces && (lower == L"localhost" ||
      lower.rfind(L"localhost:", 0) == 0 || lower.find(L'.') != std::wstring::npos);
  if (host) return "https://" + CefString(value).ToString();
  return SearchManager::Get().BuildSearchURL(value);
}

// Windows passes an opened document as a positional command-line argument.
// Convert it to a file URL before handing it to CEF.
std::string OpenArgumentURL(CefRefPtr<CefCommandLine> command_line,
                            const std::wstring& current_directory = {}) {
  CefCommandLine::ArgumentList arguments;
  command_line->GetArguments(arguments);
  for (const auto& argument : arguments) {
    const std::string value = argument.ToString();
    if (value.rfind("http://", 0) == 0 || value.rfind("https://", 0) == 0 ||
        value.rfind("file://", 0) == 0)
      return value;
    std::filesystem::path path(argument.ToWString());
    if (path.is_relative() && !current_directory.empty())
      path = std::filesystem::path(current_directory) / path;
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
    std::error_code error;
    if ((extension != L".pdf" && extension != L".htm" && extension != L".html") ||
        !std::filesystem::is_regular_file(path, error))
      continue;

    const std::wstring absolute = std::filesystem::absolute(path, error).wstring();
    if (error) continue;
    std::wstring file_url(32768, L'\0');
    DWORD length = static_cast<DWORD>(file_url.size());
    if (SUCCEEDED(UrlCreateFromPathW(absolute.c_str(), file_url.data(),
                                     &length, 0))) {
      file_url.resize(length);
      return CefString(file_url).ToString();
    }
  }
  return {};
}

class ShortcutIconCallback : public CefDownloadImageCallback {
 public:
  explicit ShortcutIconCallback(CefRefPtr<CefLabelButton> button) : button_(button) {}
  void OnDownloadImageFinished(const CefString&, int,
                               CefRefPtr<CefImage> image) override {
    if (button_ && image && !image->IsEmpty())
      button_->SetImage(CEF_BUTTON_STATE_NORMAL,image);
  }
 private:
  CefRefPtr<CefLabelButton> button_;
  IMPLEMENT_REFCOUNTING(ShortcutIconCallback);
};

class ToolbarDelegate : public CefTextfieldDelegate,
                        public CefMenuButtonDelegate,
                        public CefMenuModelDelegate {
 public:
  explicit ToolbarDelegate(CefRefPtr<CefBrowserView> view) : view_(view) {}
  void SetControls(CefRefPtr<CefLabelButton> back,
                   CefRefPtr<CefLabelButton> forward,
                   CefRefPtr<CefTextfield> address) {
    back_ = back; forward_ = forward; address_ = address;
    back_->SetEnabled(false); forward_->SetEnabled(false);
  }
  void SetAddress(const CefString& url) { if (address_) address_->SetText(url); }
  void SetNavigation(bool back, bool forward) {
    if (back_) back_->SetEnabled(back);
    if (forward_) forward_->SetEnabled(forward);
  }
  void OnButtonPressed(CefRefPtr<CefButton> button) override {
    auto browser = view_->GetBrowser();
    if (!browser) return;
    for(size_t i=0;i<shortcut_buttons_.size();++i)
      if(button->IsSame(shortcut_buttons_[i])) {
        browser->GetMainFrame()->LoadURL(shortcuts_[i].url);
        return;
      }
    if (button->IsSame(back_) && browser->CanGoBack()) browser->GoBack();
    else if (button->IsSame(forward_) && browser->CanGoForward()) browser->GoForward();
    else if (button->IsSame(refresh_)) browser->Reload();
    else if (button->IsSame(home_))
      browser->GetMainFrame()->LoadURL(SearchManager::Get().HomeURL());
  }
  bool OnKeyEvent(CefRefPtr<CefTextfield> field,
                  const CefKeyEvent& event) override {
    if (event.type != KEYEVENT_RAWKEYDOWN || event.windows_key_code != 13) return false;
    auto browser = view_->GetBrowser();
    const auto url = ResolveInput(field->GetText());
    if (browser && !url.empty()) browser->GetMainFrame()->LoadURL(url);
    return true;
  }
  void SetOtherButtons(CefRefPtr<CefLabelButton> refresh,
                       CefRefPtr<CefLabelButton> home) {
    refresh_ = refresh; home_ = home;
  }
  void SetEngineButton(CefRefPtr<CefMenuButton> button) { engine_button_ = button; }
  void RefreshEngineButton() { if (engine_button_) engine_button_->SetText(SearchManager::Get().EngineName()); }
  void SetSettingsButton(CefRefPtr<CefMenuButton> button) { settings_button_ = button; }
  void SetBookmarkButton(CefRefPtr<CefMenuButton> button) { bookmark_button_ = button; }
  void SetBookmarkState(const CefString& url) {
    if (bookmark_button_)
      bookmark_button_->SetText(SimpleHandler::GetInstance()->IsBookmarked(url.ToString()) ? "★" : "☆");
  }
  void SetTitleButton(CefRefPtr<CefLabelButton> button) { title_button_=button; }
  void SetPageTitle(const CefString& title) {
    if(title_button_) title_button_->SetText(title.empty()?"CYBROWSER":title);
  }
  void OnButtonStateChanged(CefRefPtr<CefButton> button) override {
    if(button->GetState()!=CEF_BUTTON_STATE_HOVERED ||
       !title_button_ || !button->IsSame(title_button_)) return;
    auto browser=view_->GetBrowser();
    auto manager=CefTaskManager::GetTaskManager();
    std::string memory="内存：读取中";
    if(browser && manager) {
      const int64_t id=manager->GetTaskIdForBrowserId(browser->GetIdentifier());
      CefTaskInfo info{}; info.size=sizeof(info);
      if(id>=0 && manager->GetTaskInfo(id,info)) {
        if(info.memory>=0) {
          std::ostringstream stream;
          stream<<"渲染任务内存："<<std::fixed<<std::setprecision(1)
                <<(static_cast<double>(info.memory)/1048576.0)
                <<" MB（可能由多个页面共享）";
          memory=stream.str();
        }
        cef_string_clear(&info.title);
      }
    }
    title_button_->SetTooltipText(memory);
  }
  void SetShortcutPanel(CefRefPtr<CefPanel> panel) { shortcut_panel_=panel; }
  void RefreshShortcuts(int window_width) {
    if (!shortcut_panel_) return;
    shortcut_width_=window_width;
    shortcuts_=SimpleHandler::GetInstance()->Shortcuts();
    shortcut_panel_->RemoveAllChildViews();
    shortcut_buttons_.clear();
    shortcut_more_=nullptr;
    const int capacity=std::max(1,(window_width-105)/150);
    shortcut_capacity_=capacity;
    const size_t visible=std::min(shortcuts_.size(),static_cast<size_t>(capacity));
    for(size_t i=0;i<visible;++i) {
      std::wstring name=CefString(shortcuts_[i].title.empty()?shortcuts_[i].url:shortcuts_[i].title).ToWString();
      if(name.size()>10) name=name.substr(0,9)+L"…";
      auto button=CefLabelButton::CreateLabelButton(this,CefString(name));
      button->SetMinimumSize(CefSize(134,32));
      button->SetMaximumSize(CefSize(134,32));
      button->SetTooltipText(shortcuts_[i].title+"\n"+shortcuts_[i].url);
      shortcut_panel_->AddChildView(button);
      shortcut_buttons_.push_back(button);
      if (!shortcuts_[i].icon.empty()) if (auto browser=view_->GetBrowser())
        browser->GetHost()->DownloadImage(shortcuts_[i].icon,true,20,false,
                                          new ShortcutIconCallback(button));
    }
    if(shortcuts_.size()>visible) {
      shortcut_more_=CefMenuButton::CreateMenuButton(this,"全部 ▾");
      shortcut_more_->SetTooltipText("显示全部快捷方式");
      shortcut_more_->SetMaximumSize(CefSize(76,32));
      shortcut_panel_->AddChildView(shortcut_more_);
    }
  }
  int ShortcutCapacity() const { return shortcut_capacity_; }
  void OnMenuButtonPressed(CefRefPtr<CefMenuButton> button,
                           const CefPoint& point,
                           CefRefPtr<CefMenuButtonPressedLock> lock) override {
    auto menu = CefMenuModel::CreateMenuModel(this);
    if (button->IsSame(shortcut_more_)) {
      for(size_t i=0;i<shortcuts_.size();++i)
        menu->AddItem(500+static_cast<int>(i),shortcuts_[i].title.empty()?shortcuts_[i].url:shortcuts_[i].title);
      button->ShowMenu(menu,point,CEF_MENU_ANCHOR_TOPLEFT);
      return;
    }
    if (button->IsSame(settings_button_)) {
      const char* labels[]={"历史记录","标签管理","下载管理","设置"};
      for (int i=0;i<4;++i) menu->AddItem(200+i, labels[i]);
      button->ShowMenu(menu, point, CEF_MENU_ANCHOR_TOPLEFT);
      return;
    }
    if (button->IsSame(bookmark_button_)) {
      if (auto browser=view_->GetBrowser()) {
        const std::string url=browser->GetMainFrame()->GetURL().ToString();
        if (!SimpleHandler::GetInstance()->IsBookmarked(url))
          SimpleHandler::GetInstance()->AddBookmark(browser,0);
        SetBookmarkState(url);
      }
      folders_for_menu_=SimpleHandler::GetInstance()->BookmarkFolders();
      for (size_t i=0;i<folders_for_menu_.size();++i)
        menu->AddItem(300+static_cast<int>(i), "收藏到 " + folders_for_menu_[i].second);
      menu->AddSeparator();
      menu->AddItem(399,"管理收藏标签");
      button->ShowMenu(menu, point, CEF_MENU_ANCHOR_TOPLEFT);
      return;
    }
    const char* names[] = {"Bing", "Baidu", "Google", "DuckDuckGo", "Firefox (Google)"};
    for (int i = 0; i < 5; ++i) {
      menu->AddRadioItem(100 + i, names[i], 1);
      menu->SetChecked(100 + i,
          static_cast<int>(SearchManager::Get().engine()) == i);
    }
    button->ShowMenu(menu, point, CEF_MENU_ANCHOR_TOPLEFT);
  }
  void ExecuteCommand(CefRefPtr<CefMenuModel> menu, int command_id,
                      cef_event_flags_t flags) override {
    if (command_id>=500 && static_cast<size_t>(command_id-500)<shortcuts_.size()) {
      if(auto browser=view_->GetBrowser()) browser->GetMainFrame()->LoadURL(shortcuts_[command_id-500].url);
      return;
    }
    if (command_id==399) {
      if (auto browser=view_->GetBrowser()) SimpleHandler::GetInstance()->OpenManager(browser,"tabs");
      return;
    }
    if (command_id>=300 && command_id<399) {
      size_t index=static_cast<size_t>(command_id-300);
      if (index<folders_for_menu_.size()) if (auto browser=view_->GetBrowser()) {
        SimpleHandler::GetInstance()->AddBookmark(browser,folders_for_menu_[index].first);
        SetBookmarkState(browser->GetMainFrame()->GetURL());
      }
      return;
    }
    if (command_id >= 200 && command_id <= 203) {
      const char* sections[]={"history","tabs","downloads","settings"};
      if (auto browser=view_->GetBrowser())
        SimpleHandler::GetInstance()->OpenManager(browser, sections[command_id-200]);
      return;
    }
    if (command_id < 100 || command_id > 104) return;
    SearchManager::Get().SetEngine(
        static_cast<SearchManager::Engine>(command_id - 100));
    if (engine_button_)
      engine_button_->SetText(SearchManager::Get().EngineName());
    if (auto browser = view_->GetBrowser())
      browser->GetMainFrame()->LoadURL(SearchManager::Get().HomeURL());
  }
 private:
  CefRefPtr<CefBrowserView> view_;
  CefRefPtr<CefLabelButton> back_, forward_, refresh_, home_;
  CefRefPtr<CefTextfield> address_;
  CefRefPtr<CefMenuButton> engine_button_;
  CefRefPtr<CefMenuButton> settings_button_;
  CefRefPtr<CefMenuButton> bookmark_button_;
  CefRefPtr<CefLabelButton> title_button_;
  CefRefPtr<CefPanel> shortcut_panel_;
  CefRefPtr<CefMenuButton> shortcut_more_;
  std::vector<CefRefPtr<CefLabelButton>> shortcut_buttons_;
  std::vector<SimpleHandler::Shortcut> shortcuts_;
  int shortcut_width_=800, shortcut_capacity_=0;
  std::vector<std::pair<int,std::string>> folders_for_menu_;
  IMPLEMENT_REFCOUNTING(ToolbarDelegate);
};

// When using the Views framework this object provides the delegate
// implementation for the CefWindow that hosts the Views-based browser.
class SimpleWindowDelegate : public CefWindowDelegate {
 public:
  SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                       cef_runtime_style_t runtime_style,
                       cef_show_state_t initial_show_state)
      : browser_view_(browser_view),
        runtime_style_(runtime_style),
        initial_show_state_(initial_show_state) {}

  SimpleWindowDelegate(const SimpleWindowDelegate&) = delete;
  SimpleWindowDelegate& operator=(const SimpleWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    g_windows.push_back(this);
    window->SetWindowIcon(LoadToolbarIcon(IDR_CY_ICON_SMALL));
    window->SetWindowAppIcon(LoadToolbarIcon(IDR_CY_ICON_LARGE));
    CefBoxLayoutSettings vertical;
    vertical.horizontal = false;
    auto layout = window->SetToBoxLayout(vertical);
    content_ = CefPanel::CreatePanel(nullptr);
    CefBoxLayoutSettings content_layout; content_layout.horizontal=false;
    content_->SetToBoxLayout(content_layout);
    auto toolbar = CefPanel::CreatePanel(nullptr);
    toolbar->SetBackgroundColor(CefColorSetARGB(255, 246, 248, 251));
    CefBoxLayoutSettings horizontal;
    horizontal.horizontal = true;
    horizontal.inside_border_horizontal_spacing = 12;
    horizontal.inside_border_vertical_spacing = 8;
    horizontal.between_child_spacing = 8;
    auto row = toolbar->SetToBoxLayout(horizontal);
    controls_ = new ToolbarDelegate(browser_view_);
    auto title_bar=CefPanel::CreatePanel(nullptr);
    title_bar->SetBackgroundColor(CefColorSetARGB(255,231,237,247));
    CefBoxLayoutSettings title_layout; title_layout.horizontal=true;
    title_layout.inside_border_horizontal_spacing=12;
    title_layout.inside_border_vertical_spacing=3;
    title_bar->SetToBoxLayout(title_layout);
    auto title_button=CefLabelButton::CreateLabelButton(controls_,"CYBROWSER");
    title_button->SetImage(CEF_BUTTON_STATE_NORMAL,LoadToolbarIcon(IDR_CY_ICON_SMALL));
    title_button->SetTooltipText("内存：读取中");
    title_button->SetAccessibleName("页面名称与内存占用");
    title_bar->AddChildView(title_button);
    controls_->SetTitleButton(title_button);
    auto back = MakeIconButton(controls_, IDR_NAV_BACK, "Back");
    auto forward = MakeIconButton(controls_, IDR_NAV_FORWARD, "Forward");
    auto refresh = MakeIconButton(controls_, IDR_NAV_REFRESH, "Refresh");
    auto home = MakeIconButton(controls_, IDR_NAV_HOME, "Home");
    auto address = CefTextfield::CreateTextfield(controls_);
    auto bookmark = CefMenuButton::CreateMenuButton(controls_,"☆");
    bookmark->SetTooltipText("收藏当前页面到标签");
    bookmark->SetAccessibleName("收藏当前页面到标签");
    bookmark->SetMinimumSize(CefSize(38,36));
    auto engine = CefMenuButton::CreateMenuButton(
        controls_, SearchManager::Get().EngineName());
    auto settings = CefMenuButton::CreateMenuButton(controls_, "⚙");
    settings->SetTooltipText("管理与设置");
    settings->SetAccessibleName("管理与设置");
    settings->SetMinimumSize(CefSize(40,36));
    engine->SetTooltipText("Choose default search engine");
    engine->SetAccessibleName("Choose default search engine");
    address->SetPlaceholderText("Search or enter a web address");
    address->SetAccessibleName("Address and search");
    address->SetBackgroundColor(CefColorSetARGB(255, 255, 255, 255));
    address->SetSize(CefSize(360, 36));
    toolbar->AddChildView(back);
    toolbar->AddChildView(forward);
    toolbar->AddChildView(address);
    toolbar->AddChildView(bookmark);
    toolbar->AddChildView(engine);
    toolbar->AddChildView(refresh);
    toolbar->AddChildView(home);
    toolbar->AddChildView(settings);
    row->SetFlexForView(address, 1);
    controls_->SetControls(back, forward, address);
    controls_->SetOtherButtons(refresh, home);
    controls_->SetEngineButton(engine);
    controls_->SetSettingsButton(settings);
    controls_->SetBookmarkButton(bookmark);
    auto shortcut_bar=CefPanel::CreatePanel(nullptr);
    shortcut_bar->SetBackgroundColor(CefColorSetARGB(255,250,252,255));
    CefBoxLayoutSettings shortcut_layout; shortcut_layout.horizontal=true;
    shortcut_layout.inside_border_horizontal_spacing=12;
    shortcut_layout.inside_border_vertical_spacing=5;
    shortcut_layout.between_child_spacing=6;
    shortcut_bar->SetToBoxLayout(shortcut_layout);
    controls_->SetShortcutPanel(shortcut_bar);
    window->AddChildView(title_bar);
    window->AddChildView(toolbar);
    window->AddChildView(shortcut_bar);
    window->AddChildView(content_);
    content_->AddChildView(browser_view_);
    layout->SetFlexForView(content_, 1);
    content_->GetLayout()->AsBoxLayout()->SetFlexForView(browser_view_,1);
    controls_->RefreshShortcuts(window->GetBounds().width>0?window->GetBounds().width:800);
    if (initial_show_state_ != CEF_SHOW_STATE_HIDDEN) {
      window->Show();
    }
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    g_windows.erase(std::remove(g_windows.begin(),g_windows.end(),this),g_windows.end());
    if(auto browser=browser_view_?browser_view_->GetBrowser():nullptr)
      browser->GetHost()->CloseBrowser(true);
    controls_ = nullptr;
    browser_view_ = nullptr;
    content_ = nullptr;
  }
  void OnWindowBoundsChanged(CefRefPtr<CefWindow> window,
                             const CefRect& bounds) override {
    if(controls_ && bounds.width>0) {
      const int capacity=std::max(1,(bounds.width-105)/150);
      if(capacity!=controls_->ShortcutCapacity()) controls_->RefreshShortcuts(bounds.width);
    }
  }

  bool Owns(CefRefPtr<CefBrowser> browser) const {
    auto current = browser_view_ ? browser_view_->GetBrowser() : nullptr;
    return current && current->IsSame(browser);
  }
  void SetPageTitle(const CefString& title) { if(controls_) controls_->SetPageTitle(title); }
  void SetAddress(const CefString& url) { if (controls_) controls_->SetAddress(url); }
  void SetNavigation(bool back, bool forward) {
    if (controls_) controls_->SetNavigation(back, forward);
  }
  void SetBookmarkState(const CefString& url) { if (controls_) controls_->SetBookmarkState(url); }
  void RefreshEngineButton() { if (controls_) controls_->RefreshEngineButton(); }
  void RefreshShortcuts() {
    if(controls_ && browser_view_ && browser_view_->GetWindow()) {
      const int width=browser_view_->GetWindow()->GetBounds().width;
      controls_->RefreshShortcuts(width>0?width:800);
    }
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK.
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(800, 600);
  }

  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override {
    return initial_show_state_;
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
  CefRefPtr<CefPanel> content_;
  CefRefPtr<ToolbarDelegate> controls_;
  const cef_runtime_style_t runtime_style_;
  const cef_show_state_t initial_show_state_;

  IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
};

class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  explicit SimpleBrowserViewDelegate(cef_runtime_style_t runtime_style)
      : runtime_style_(runtime_style) {}

  SimpleBrowserViewDelegate(const SimpleBrowserViewDelegate&) = delete;
  SimpleBrowserViewDelegate& operator=(const SimpleBrowserViewDelegate&) =
      delete;

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override {
    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        popup_browser_view, runtime_style_, CEF_SHOW_STATE_MAXIMIZED));

    // We created the Window.
    return true;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  const cef_runtime_style_t runtime_style_;

  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
};

}  // namespace

void UpdateAddressBar(CefRefPtr<CefBrowser> browser, const CefString& url) {
  for(auto* window:g_windows) if(window->Owns(browser)) { window->SetAddress(url); break; }
}

void UpdateNavigationButtons(CefRefPtr<CefBrowser> browser, bool back,
                             bool forward) {
  for(auto* window:g_windows) if(window->Owns(browser)) { window->SetNavigation(back,forward); break; }
}
void UpdateBookmarkButton(CefRefPtr<CefBrowser> browser, const CefString& url) {
  for(auto* window:g_windows) if(window->Owns(browser)) { window->SetBookmarkState(url); break; }
}
void UpdateTabTitle(CefRefPtr<CefBrowser> browser,const CefString& title) {
  for(auto* window:g_windows) if(window->Owns(browser)) { window->SetPageTitle(title); break; }
}
void UpdateTabIcon(CefRefPtr<CefBrowser>,const CefString&) {}
void NotifyTabClosed(CefRefPtr<CefBrowser>) {}
bool HasMainWindow() { return !g_windows.empty(); }
void UpdateSearchEngineButton(CefRefPtr<CefBrowser> browser) {
  for(auto* window:g_windows) if(window->Owns(browser)) { window->RefreshEngineButton(); break; }
}
void UpdateShortcutBar() {
  for(auto* window:g_windows) window->RefreshShortcuts();
}
SimpleApp::SimpleApp() = default;

bool SimpleApp::OnAlreadyRunningAppRelaunch(
    CefRefPtr<CefCommandLine> command_line,
    const CefString& current_directory) {
  CEF_REQUIRE_UI_THREAD();
  auto* handler = SimpleHandler::GetInstance();
  if (!handler) return false;
  std::string url = command_line->GetSwitchValue("url");
  if (url.empty())
    url = OpenArgumentURL(command_line, current_directory.ToWString());
  if (!url.empty() && !handler->OpenURL(url)) return false;
  handler->ShowMainWindow();
  return true;
}

void SimpleApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // Check if Alloy style will be used.
  cef_runtime_style_t runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
  bool use_alloy_style = command_line->HasSwitch("use-alloy-style");
  if (use_alloy_style) {
    runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }

  // SimpleHandler implements browser-level callbacks.
  CefRefPtr<SimpleHandler> handler(new SimpleHandler(use_alloy_style));

  // Specify CEF browser settings here.
  CefBrowserSettings browser_settings;

  std::string url;

  // Check if a "--url=" value was provided via the command-line. If so, use
  // that instead of the default URL.
  url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = OpenArgumentURL(command_line);
  }
  if (url.empty()) {
    url = SearchManager::Get().HomeURL();
  }

  // Views is enabled by default (add `--use-native` to disable).
  const bool use_views = !command_line->HasSwitch("use-native");

  // If using Views create the browser using the Views framework, otherwise
  // create the browser using the native platform framework.
  if (use_views) {
    // Create the BrowserView.
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        handler, url, browser_settings, nullptr, nullptr,
        new SimpleBrowserViewDelegate(runtime_style));

    // Optionally configure the initial show state.
    cef_show_state_t initial_show_state = CEF_SHOW_STATE_MAXIMIZED;
    const std::string& show_state_value =
        command_line->GetSwitchValue("initial-show-state");
    if (show_state_value == "minimized") {
      initial_show_state = CEF_SHOW_STATE_MINIMIZED;
    } else if (show_state_value == "normal") {
      initial_show_state = CEF_SHOW_STATE_NORMAL;
    } else if (show_state_value == "maximized") {
      initial_show_state = CEF_SHOW_STATE_MAXIMIZED;
    }
#if defined(OS_MAC)
    // Hidden show state is only supported on MacOS.
    else if (show_state_value == "hidden") {
      initial_show_state = CEF_SHOW_STATE_HIDDEN;
    }
#endif

    // Create the Window. It will show itself after creation.
    CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(
        browser_view, runtime_style, initial_show_state));
  } else {
    // Information used when creating the native window.
    CefWindowInfo window_info;

#if defined(OS_WIN)
    // On Windows we need to specify certain flags that will be passed to
    // CreateWindowEx().
    window_info.SetAsPopup(nullptr, "cefsimple");
#endif

    // Alloy style will create a basic native window. Chrome style will create a
    // fully styled Chrome UI window.
    window_info.runtime_style = runtime_style;

    // Create the first browser window.
    CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings,
                                  nullptr, nullptr);
  }
}

CefRefPtr<CefClient> SimpleApp::GetDefaultClient() {
  // Called when a new browser window is created via Chrome style UI.
  return SimpleHandler::GetInstance();
}
