// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "simple_handler.h"
#include "simple_app.h"
#include "search_manager.h"
#include "local_data.h"

#include <sstream>
#include <string>
#include <fstream>
#include <algorithm>
#include <set>
#include <filesystem>
#include <shellapi.h>
#include <iomanip>
#include "include/cef_cookie.h"
#include "include/cef_download_item.h"

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

namespace {

SimpleHandler* g_instance = nullptr;

// Returns a data: URI with the specified contents.
std::string GetDataURI(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

void OpenDefaultBrowserSettings();

SimpleHandler::SimpleHandler(bool is_alloy_style)
    : is_alloy_style_(is_alloy_style) {
  DCHECK(!g_instance);
  g_instance = this;
  LoadHistory();
  LoadBookmarks();
  LoadDownloads();
}

SimpleHandler::~SimpleHandler() {
  g_instance = nullptr;
}

// static
SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  CEF_REQUIRE_UI_THREAD();
  if (frame->IsMain() && url.ToString().rfind("data:text/html", 0) != 0) {
    UpdateAddressBar(browser, url);
    UpdateBookmarkButton(browser, url);
    page_icons_.erase(browser->GetIdentifier());
    UpdateTabIcon(browser,"");
    UpdateTabTitle(browser,url);
    UpdateShortcutBar();
  }
}

void SimpleHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                         bool isLoading, bool canGoBack,
                                         bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();
  UpdateNavigationButtons(browser, canGoBack, canGoForward);
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  const auto url = browser->GetMainFrame()->GetURL().ToString();
  page_titles_[browser->GetIdentifier()] = title.ToString();
  UpdateTabTitle(browser,title);
  if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
    if (history_.empty() || history_.back().first != url) {
      history_.push_back({url, title.ToString()});
      if (history_.size() > 2000) history_.erase(history_.begin());
      SaveHistory();
    } else if (history_.back().second != title.ToString()) {
      history_.back().second = title.ToString(); SaveHistory();
    }
  }

  if (auto browser_view = CefBrowserView::GetForBrowser(browser)) {
    // Set the title of the window using the Views framework.
    CefRefPtr<CefWindow> window = browser_view->GetWindow();
    if (window) {
      window->SetTitle(title);
    }
  } else if (is_alloy_style_) {
    // Set the title of the window using platform APIs.
    PlatformTitleChange(browser, title);
  }
}

void SimpleHandler::OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
                                       const std::vector<CefString>& icon_urls) {
  page_icons_.erase(browser->GetIdentifier());
  if (!icon_urls.empty()) {
    const std::string icon=icon_urls.front().ToString();
    if (icon.rfind("https://",0)==0 || icon.rfind("http://",0)==0) {
      page_icons_[browser->GetIdentifier()]=icon;
      UpdateTabIcon(browser,icon);
      const std::string url=browser->GetMainFrame()->GetURL().ToString();
      bool updated=false;
      for(auto& bookmark:bookmarks_) if(bookmark.url==url && bookmark.icon!=icon) {
        bookmark.icon=icon; updated=true;
      }
      if(updated) { SaveBookmarks(); UpdateShortcutBar(); }
    }
  }
}

namespace {
std::filesystem::path HistoryPath() {
  auto dir = LocalDataPath();
  std::filesystem::create_directories(dir);
  return dir / L"history.tsv";
}
std::filesystem::path BookmarkPath() { return HistoryPath().parent_path()/L"bookmarks.txt"; }
std::filesystem::path DownloadsPath() { return HistoryPath().parent_path()/L"downloads.txt"; }
std::string DecodeQuery(std::string value) {
  std::replace(value.begin(),value.end(),'+',' ');
  return CefURIDecode(value,true,static_cast<cef_uri_unescape_rule_t>(UU_NORMAL | UU_SPACES | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS)).ToString();
}
std::string Escape(const std::string& value) {
  std::string out;
  for (char c : value) {
    switch (c) { case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break; case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; }
  }
  return out;
}
std::string Link(const std::string& section, const std::string& action = "") {
  return "https://cybrowser.invalid/" + section + (action.empty() ? "" : "/" + action);
}
std::string PageStart(const std::string& section) {
  std::string s = R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>CYBROWSER 管理中心</title><style>
  :root{font-family:'Segoe UI','Microsoft YaHei',sans-serif;color:#18243a;background:#f4f7fb}*{box-sizing:border-box}body{margin:0}.shell{max-width:1150px;margin:48px auto;padding:0 26px}header{display:flex;align-items:center;justify-content:space-between;margin-bottom:26px}h1{font-size:30px;letter-spacing:-.5px;margin:0}header p{color:#66758c;margin:7px 0}nav{display:flex;gap:9px;flex-wrap:wrap;margin-bottom:20px}nav a,.btn{display:inline-block;padding:10px 16px;border-radius:11px;text-decoration:none;background:white;color:#28405e;border:1px solid #dce4ee;font-size:14px}nav a.active,.btn.primary{background:#3468e8;color:white;border-color:#3468e8}.card{background:white;border:1px solid #e1e8f1;border-radius:18px;box-shadow:0 12px 30px #24406a0a;padding:24px;margin-bottom:18px}.row{display:flex;gap:14px;align-items:center;padding:13px 0;border-bottom:1px solid #edf1f6}.row:last-child{border:0}.grow{flex:1;min-width:0}.title{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.sub{font-size:13px;color:#6f8094;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:4px}.actions{display:flex;gap:8px;flex-wrap:wrap}h2{font-size:18px;margin:0 0 16px}.empty{padding:30px;color:#758398;text-align:center}small{color:#718096} .danger{color:#bb3948}.pill{font-size:12px;color:#365ec0;background:#ebf1ff;padding:5px 9px;border-radius:9px}</style><div class="shell"><header><div><h1>浏览器管理中心</h1><p>管理浏览数据、下载与偏好设置</p></div><span class="pill">CYBROWSER</span></header><nav>)HTML";
  const std::pair<const char*,const char*> sections[]={{"history","历史记录"},{"tabs","收藏标签"},{"downloads","下载"},{"settings","设置"}};
  for (auto& entry : sections) s += "<a class='" + std::string(section==entry.first?"active":"") + "' href='" + Link(entry.first) + "'>" + entry.second + "</a>";
  return s + "</nav>";
}
void ShowPage(CefRefPtr<CefBrowser> browser, const std::string& html) {
  browser->GetMainFrame()->LoadURL(GetDataURI(html + "</div></html>", "text/html"));
}
std::string SiteHost(const std::string& url) {
  CefURLParts parts;
  if (!CefParseURL(url,parts)) return {};
  return CefString(&parts.host).ToString();
}
bool CookieForSite(const CefCookie& cookie,const std::string& host) {
  if(host.empty()) return false;
  std::string domain=CefString(&cookie.domain).ToString();
  const bool domain_cookie=!domain.empty() && domain[0]=='.';
  if(domain_cookie) domain.erase(0,1);
  if(domain.empty()) return false;
  return host==domain || (domain_cookie && host.size()>domain.size() &&
    host.compare(host.size()-domain.size(),domain.size(),domain)==0 &&
    host[host.size()-domain.size()-1]=='.');
}
class SiteCookieVisitor : public CefCookieVisitor {
 public:
  SiteCookieVisitor(CefRefPtr<CefBrowser> browser,std::string host,size_t history_index,
                    int delete_index=-1,bool delete_all=false)
      :browser_(browser),host_(std::move(host)),history_index_(history_index),
       delete_index_(delete_index),delete_all_(delete_all) {}
  bool Visit(const CefCookie& cookie, int count, int total, bool& delete_cookie) override {
    if(CookieForSite(cookie,host_)) {
      if(delete_all_ || matched_==delete_index_) delete_cookie=true;
      if(delete_index_<0 && !delete_all_) {
        html_ += "<div class='row'><div class='grow'><div class='title'>"+
          Escape(CefString(&cookie.name).ToString())+"</div><div class='sub'>"+
          Escape(CefString(&cookie.domain).ToString())+" · "+
          Escape(CefString(&cookie.path).ToString())+
          "</div></div><a class='btn danger' href='"+
          Link("history","cookie-delete/"+std::to_string(history_index_)+"/"+
          std::to_string(matched_))+"'>删除 Cookie</a></div>";
      }
      ++matched_;
    }
    if(count==total-1) {
      if(delete_index_>=0) {
        CefPostTask(TID_UI,base::BindOnce([](CefRefPtr<CefBrowser> browser,size_t index){
          if(auto handler=SimpleHandler::GetInstance())
            handler->OpenManager(browser,"history/site/"+std::to_string(index));
        },browser_,history_index_));
      } else if(!delete_all_) {
        std::string page=PageStart("history")+"<div class='card'><h2>"+
          Escape(host_)+" · 高级设置</h2><p>此网站当前有效的 Cookie："+
          std::to_string(matched_)+" 个</p>";
        page+=matched_?html_:"<div class='empty'>此网站暂无有效 Cookie</div>";
        ShowPage(browser_,page+"</div><a class='btn' href='"+Link("history")+"'>返回历史记录</a>");
      }
    }
    return true;
  }
 private:
  CefRefPtr<CefBrowser> browser_;
  std::string host_,html_;
  size_t history_index_;
  int delete_index_,matched_=0;
  bool delete_all_;
  IMPLEMENT_REFCOUNTING(SiteCookieVisitor);
};
class HistoryCookieCleanup : public CefCookieVisitor {
 public:
  explicit HistoryCookieCleanup(std::set<std::string> hosts):hosts_(std::move(hosts)) {}
  bool Visit(const CefCookie& cookie,int,int,bool& delete_cookie) override {
    for(const auto& host:hosts_) if(CookieForSite(cookie,host)) {
      delete_cookie=true; break;
    }
    return true;
  }
 private:
  std::set<std::string> hosts_;
  IMPLEMENT_REFCOUNTING(HistoryCookieCleanup);
};
}

void SimpleHandler::LoadHistory() {
  std::ifstream file(HistoryPath(), std::ios::binary);
  std::string url, title;
  while (std::getline(file, url, '\t') && std::getline(file, title))
    if (!url.empty()) history_.push_back({url,title});
}
void SimpleHandler::SaveHistory() {
  std::ofstream file(HistoryPath(), std::ios::binary | std::ios::trunc);
  for (const auto& item : history_) file << item.first << '\t' << item.second << '\n';
}
void SimpleHandler::LoadDownloads() {
  std::ifstream file(DownloadsPath(),std::ios::binary);
  Download item{};
  while(file >> item.id >> std::quoted(item.name) >> std::quoted(item.path)
             >> std::quoted(item.url) >> item.percent >> item.complete >> item.canceled) {
    item.cef_id=0; item.callback=nullptr;
    if (item.id>=next_download_id_) next_download_id_=item.id+1;
    if (!item.complete && !item.canceled) item.canceled=true;
    downloads_.push_back(item);
  }
}
void SimpleHandler::SaveDownloads() {
  std::ofstream file(DownloadsPath(),std::ios::binary|std::ios::trunc);
  for(const auto& item:downloads_) file << item.id << ' ' << std::quoted(item.name) << ' '
    << std::quoted(item.path) << ' ' << std::quoted(item.url) << ' '
    << item.percent << ' ' << item.complete << ' ' << item.canceled << '\n';
}
void SimpleHandler::LoadBookmarks() {
  folders_.push_back({0,"默认标签"});
  folders_.push_back({-1,"快捷方式"});
  std::ifstream file(BookmarkPath(),std::ios::binary);
  char kind;
  while (file >> kind) {
    if (kind=='F') {
      Folder folder{};
      if (!(file >> folder.id >> std::quoted(folder.name))) break;
      if (folder.id>0) { next_folder_id_=std::max(next_folder_id_,folder.id+1); folders_.push_back(folder); }
    } else if (kind=='B') {
      Bookmark bookmark{};
      if (!(file >> bookmark.id >> bookmark.folder_id >> std::quoted(bookmark.url)
                 >> std::quoted(bookmark.title) >> std::quoted(bookmark.icon))) break;
      if (bookmark.url.rfind("http://",0)==0 || bookmark.url.rfind("https://",0)==0) {
        next_bookmark_id_=std::max(next_bookmark_id_,bookmark.id+1);
        bookmarks_.push_back(bookmark);
      }
    } else break;
  }
}
void SimpleHandler::SaveBookmarks() {
  std::ofstream file(BookmarkPath(),std::ios::binary|std::ios::trunc);
  for (const auto& folder:folders_) if (folder.id>0)
    file << "F " << folder.id << ' ' << std::quoted(folder.name) << '\n';
  for (const auto& bookmark:bookmarks_)
    file << "B " << bookmark.id << ' ' << bookmark.folder_id << ' '
         << std::quoted(bookmark.url) << ' ' << std::quoted(bookmark.title) << ' '
         << std::quoted(bookmark.icon) << '\n';
}
std::vector<std::pair<int,std::string>> SimpleHandler::BookmarkFolders() const {
  std::vector<std::pair<int,std::string>> result;
  for (const auto& folder:folders_) result.push_back({folder.id,folder.name});
  return result;
}
std::vector<SimpleHandler::Shortcut> SimpleHandler::Shortcuts() const {
  std::vector<Shortcut> result;
  for (const auto& bookmark:bookmarks_) if (bookmark.folder_id==-1)
    result.push_back({bookmark.url,bookmark.title,bookmark.icon});
  return result;
}
bool SimpleHandler::IsBookmarked(const std::string& url) const {
  return std::any_of(bookmarks_.begin(),bookmarks_.end(),[&](const Bookmark& b){return b.url==url;});
}
void SimpleHandler::AddBookmark(CefRefPtr<CefBrowser> browser, int folder_id) {
  const std::string url=browser->GetMainFrame()->GetURL().ToString();
  if (url.rfind("http://",0)!=0 && url.rfind("https://",0)!=0) return;
  if (std::none_of(folders_.begin(),folders_.end(),[&](const Folder& f){return f.id==folder_id;})) folder_id=0;
  auto it=std::find_if(bookmarks_.begin(),bookmarks_.end(),[&](const Bookmark& b){return b.url==url && b.folder_id==folder_id;});
  std::string title=page_titles_[browser->GetIdentifier()];
  if (title.empty()) title=url;
  const std::string icon=page_icons_[browser->GetIdentifier()];
  if (it==bookmarks_.end()) bookmarks_.push_back({next_bookmark_id_++,folder_id,url,title,icon});
  else { it->title=title; it->icon=icon; }
  if (folder_id!=-1) bookmarks_.erase(std::remove_if(bookmarks_.begin(),bookmarks_.end(),[&](const Bookmark& b){return b.url==url && b.folder_id!=folder_id && b.folder_id!=-1;}),bookmarks_.end());
  SaveBookmarks();
  UpdateBookmarkButton(browser,url);
  UpdateShortcutBar();
}
void SimpleHandler::OpenManager(CefRefPtr<CefBrowser> browser, const std::string& section) {
  manager_section_ = section; RenderManager(browser);
}

void SimpleHandler::RenderManager(CefRefPtr<CefBrowser> browser) {
  const auto& section = manager_section_;
  if (section.rfind("history/site/",0)==0) {
    const size_t index=static_cast<size_t>(std::strtoull(section.c_str()+13,nullptr,10));
    if(index>=history_.size()) { OpenManager(browser,"history"); return; }
    const std::string host=SiteHost(history_[index].first);
    ShowPage(browser,PageStart("history")+"<div class='card'><h2>"+Escape(host)+
      " · 高级设置</h2><p>此网站暂无有效 Cookie，或正在读取…</p></div>");
    auto manager = CefCookieManager::GetGlobalManager(nullptr);
    if (manager) manager->VisitAllCookies(new SiteCookieVisitor(browser,host,index));
    return;
  }
  std::string html = PageStart(section);
  if (section == "history") {
    html += "<div class='card'><h2>历史记录</h2>";
    if (history_.empty()) html += "<div class='empty'>尚无浏览记录</div>";
    for (size_t i=history_.size(); i>0; --i) {
      const auto& entry=history_[i-1];
      html += "<div class='row'><div class='grow'><div class='title'>" + Escape(entry.second.empty()?entry.first:entry.second) + "</div><div class='sub'>" + Escape(entry.first) + "</div></div><a class='btn' href='" + Escape(entry.first) + "'>打开</a><a class='btn' href='"+Link("history","site/"+std::to_string(i-1))+"'>高级设置</a><a class='btn danger' href='" + Link("history","delete/"+std::to_string(i-1)) + "'>删除网站记录及 Cookie</a></div>";
    }
    html += "</div><a class='btn danger' href='" + Link("history","clear") + "'>清除历史记录及相关 Cookie</a>";
  } else if (section == "downloads") {
    html += "<div class='card'><h2>下载任务</h2>";
    if (downloads_.empty()) html += "<div class='empty'>暂无下载任务</div>";
    for (const auto& item : downloads_) {
      html += "<div class='row'><div class='grow'><div class='title'>" + Escape(item.name) + "</div><div class='sub'>" + Escape(item.path) + " · " + (item.complete?"已完成":item.canceled?"已取消":std::to_string(item.percent)+"%") + "</div></div>";
      if (!item.complete && !item.canceled) html += "<a class='btn danger' href='" + Link("downloads","cancel/"+std::to_string(item.id)) + "'>取消</a>";
      if (item.complete && !item.path.empty()) html += "<a class='btn' href='" + Link("downloads","show/"+std::to_string(item.id)) + "'>所在文件夹</a>";
      html += "</div>";
    }
    html += "</div><a class='btn danger' href='" + Link("downloads","clear") + "'>清除任务记录</a>";
  } else if (section == "settings") {
    html += "<div class='card'><h2>默认搜索引擎</h2><p>选择地址栏搜索使用的引擎。</p><div class='actions'>";
    const char* names[]={"Bing","Baidu","Google","DuckDuckGo","Firefox (Google)"};
    for (int i=0;i<5;++i) html += "<a class='btn " + std::string(static_cast<int>(SearchManager::Get().engine())==i?"primary":"") + "' href='" + Link("settings","engine/"+std::to_string(i)) + "'>" + names[i] + "</a>";
    html += "</div></div><div class='card'><h2>默认浏览器</h2><p>在 Windows 默认应用设置中选择 CYBROWSER。</p><a class='btn primary' href='" + Link("settings","default-browser") + "'>设置为默认浏览器</a></div><div class='card'><h2>浏览数据</h2><a class='btn' href='" + Link("history") + "'>按网站管理历史与 Cookie</a></div>";
  } else if (section == "tabs") {
    html += "<div class='card'><h2>新建收藏标签</h2><form action='" + Link("tabs","create") + "' method='get'><input name='name' maxlength='60' required placeholder='输入标签名称' style='padding:11px 13px;border:1px solid #dce4ee;border-radius:10px;font:inherit;width:min(320px,70%)'> <button class='btn primary' type='submit'>创建标签</button></form></div>";
    for (const auto& folder:folders_) {
      html += "<div class='card'><div class='row'><div class='grow'><h2>" + Escape(folder.name) + "</h2><small>" + std::to_string(std::count_if(bookmarks_.begin(),bookmarks_.end(),[&](const Bookmark& b){return b.folder_id==folder.id;})) + " 个网址</small></div>";
      if (folder.id>0) html += "<a class='btn danger' href='" + Link("tabs","delete-folder/"+std::to_string(folder.id)) + "'>删除标签及网址</a>";
      html += "</div>";
      for (const auto& bookmark:bookmarks_) if (bookmark.folder_id==folder.id) {
        html += "<div class='row'>";
        if (!bookmark.icon.empty()) html += "<img src='" + Escape(bookmark.icon) + "' width='20' height='20' style='object-fit:contain'>";
        html += "<div class='grow'><div class='title'>" + Escape(bookmark.title) + "</div><div class='sub'>" + Escape(bookmark.url) + "</div></div><a class='btn' href='" + Escape(bookmark.url) + "'>打开</a><a class='btn danger' href='" + Link("tabs","delete-bookmark/"+std::to_string(bookmark.id)) + "'>删除</a></div>";
      }
      html += "</div>";
    }
  } else {
    html += "<div class='card'><h2>未知页面</h2></div>";
  }
  ShowPage(browser, html);
}

bool SimpleHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request, bool, bool) {
  if (!frame->IsMain()) return false;
  const std::string url=request->GetURL().ToString(), prefix="https://cybrowser.invalid/";
  if (url.rfind(prefix,0)!=0) return false;
  std::string path=url.substr(prefix.size());
  const auto slash=path.find('/');
  const std::string section=path.substr(0,slash);
  const std::string action=slash==std::string::npos?"":path.substr(slash+1);
  if (section=="settings" && action=="default-browser") {
    OpenDefaultBrowserSettings();
    return true;
  }
  if(section=="cookies") {
    OpenManager(browser,"history");
    return true;
  }
  if (section=="accounts") {
    OpenManager(browser,"settings");
    return true;
  }
  if (section=="tabs" && action.rfind("create?name=",0)==0) {
    std::string raw=action.substr(12);
    const auto amp=raw.find('&'); if (amp!=std::string::npos) raw.resize(amp);
    std::string name=DecodeQuery(raw);
    if (!name.empty() && name.size()<=180 &&
        std::none_of(folders_.begin(),folders_.end(),[&](const Folder& folder){return folder.name==name;})) {
      folders_.push_back({next_folder_id_++,name}); SaveBookmarks();
    }
  }
  if (section=="tabs" && action.rfind("delete-folder/",0)==0) {
    int id=atoi(action.c_str()+14);
    if (id>0) {
      folders_.erase(std::remove_if(folders_.begin(),folders_.end(),[&](const Folder& f){return f.id==id;}),folders_.end());
      bookmarks_.erase(std::remove_if(bookmarks_.begin(),bookmarks_.end(),[&](const Bookmark& b){return b.folder_id==id;}),bookmarks_.end());
      SaveBookmarks(); UpdateBookmarkButton(browser,browser->GetMainFrame()->GetURL());
      UpdateShortcutBar();
    }
  }
  if (section=="tabs" && action.rfind("delete-bookmark/",0)==0) {
    int id=atoi(action.c_str()+16);
    bookmarks_.erase(std::remove_if(bookmarks_.begin(),bookmarks_.end(),[&](const Bookmark& b){return b.id==id;}),bookmarks_.end());
    SaveBookmarks(); UpdateBookmarkButton(browser,browser->GetMainFrame()->GetURL());
    UpdateShortcutBar();
  }
  if (section=="history" && action=="clear") {
    std::set<std::string> hosts;
    for(const auto& entry:history_) {
      auto host=SiteHost(entry.first); if(!host.empty()) hosts.insert(std::move(host));
    }
    if(auto manager=CefCookieManager::GetGlobalManager(nullptr))
      if(!hosts.empty()) manager->VisitAllCookies(new HistoryCookieCleanup(std::move(hosts)));
    history_.clear(); SaveHistory();
  }
  if (section=="history" && action.rfind("delete/",0)==0) {
    size_t index=static_cast<size_t>(atoi(action.c_str()+7));
    if (index<history_.size()) {
      const auto host=SiteHost(history_[index].first);
      if(auto manager=CefCookieManager::GetGlobalManager(nullptr))
        if(!host.empty()) manager->VisitAllCookies(new SiteCookieVisitor(browser,host,index,-1,true));
      history_.erase(history_.begin()+index); SaveHistory();
    }
  }
  if(section=="history" && action.rfind("site/",0)==0) {
    manager_section_="history/"+action;
    CefPostTask(TID_UI,base::BindOnce(&SimpleHandler::RenderManager,this,browser));
    return true;
  }
  if(section=="history" && action.rfind("cookie-delete/",0)==0) {
    const auto args=action.substr(14);
    const auto split=args.find('/');
    if(split!=std::string::npos) {
      const size_t index=static_cast<size_t>(std::strtoull(args.c_str(),nullptr,10));
      const int cookie_index=atoi(args.c_str()+split+1);
      if(index<history_.size() && cookie_index>=0)
        if(auto manager=CefCookieManager::GetGlobalManager(nullptr))
          manager->VisitAllCookies(new SiteCookieVisitor(browser,SiteHost(history_[index].first),index,cookie_index));
    }
    return true;
  }
  if (section=="settings" && action.rfind("engine/",0)==0) {
    int i=action.back()-'0';
    if (i>=0 && i<5) {
      SearchManager::Get().SetEngine(static_cast<SearchManager::Engine>(i));
      UpdateSearchEngineButton(browser);
    }
  }
  if (section=="downloads" && action.rfind("cancel/",0)==0) {
    uint64_t id=std::strtoull(action.c_str()+7,nullptr,10);
    for (auto& item:downloads_) if (item.id==id && item.callback) item.callback->Cancel();
  }
  if (section=="downloads" && action.rfind("show/",0)==0) {
    uint64_t id=std::strtoull(action.c_str()+5,nullptr,10);
    for (const auto& item:downloads_) if (item.id==id && item.complete && !item.path.empty()) {
      std::filesystem::path downloaded_path=std::filesystem::u8path(item.path);
      ShellExecuteW(nullptr,L"open",downloaded_path.parent_path().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    }
  }
  if (section=="downloads" && action=="clear")
    downloads_.erase(std::remove_if(downloads_.begin(),downloads_.end(),[](const Download& d){return d.complete || d.canceled;}),downloads_.end());
  if (section=="downloads" && action=="clear") SaveDownloads();
  manager_section_=section;
  CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::RenderManager, this, browser));
  return true;
}
bool SimpleHandler::OnBeforeDownload(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem>,
                                     const CefString& suggested_name, CefRefPtr<CefBeforeDownloadCallback> callback) {
  auto directory=LocalDataPath()/L"downloads";
  std::filesystem::create_directories(directory);
  auto name=std::filesystem::u8path(suggested_name.ToString()).filename();
  if(name.empty()) name=L"download";
  auto path=directory/name;
  int suffix=1;
  while(std::filesystem::exists(path)) {
    path=directory/(name.stem().wstring()+L" ("+std::to_wstring(suffix++)+L")"+name.extension().wstring());
  }
  callback->Continue(path.wstring(),false);
  return true;
}
void SimpleHandler::OnDownloadUpdated(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem> item,
                                      CefRefPtr<CefDownloadItemCallback> callback) {
  auto it=std::find_if(downloads_.begin(),downloads_.end(),[&](const Download& d){return d.cef_id==item->GetId() && d.cef_id!=0;});
  if (it==downloads_.end()) { downloads_.push_back({next_download_id_++,item->GetId(),item->GetSuggestedFileName().ToString(),item->GetFullPath().ToString(),item->GetURL().ToString(),item->GetPercentComplete(),item->IsComplete(),item->IsCanceled(),callback}); }
  else { it->path=item->GetFullPath().ToString(); it->percent=item->GetPercentComplete(); it->complete=item->IsComplete(); it->canceled=item->IsCanceled(); it->callback=callback; }
  SaveDownloads();
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Sanity-check the configured runtime style.
  CHECK_EQ(is_alloy_style_ ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_CHROME,
           browser->GetHost()->GetRuntimeStyle());

  // Add to the list of existing browsers.
  browser_list_.push_back(browser);
}

bool SimpleHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Closing the main window requires special handling. See the DoClose()
  // documentation in the CEF header for a detailed destription of this
  // process.
  if (browser_list_.size() == 1) {
    // Set a flag to indicate that the window close should be allowed.
    is_closing_ = true;
  }

  // Allow the close. For windowed browsers this will result in the OS close
  // event being sent.
  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  NotifyTabClosed(browser);

  // Remove from the list of existing browsers.
  BrowserList::iterator bit = browser_list_.begin();
  for (; bit != browser_list_.end(); ++bit) {
    if ((*bit)->IsSame(browser)) {
      browser_list_.erase(bit);
      break;
    }
  }

  if (browser_list_.empty()) {
    // All browser windows have closed. Quit the application message loop.
    CefQuitMessageLoop();
  }
}

void SimpleHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  // Allow Chrome to show the error page.
  if (!is_alloy_style_) {
    return;
  }

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED) {
    return;
  }

  // Display a load error message using a data: URI.
  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";

  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}

void SimpleHandler::ShowMainWindow() {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::ShowMainWindow, this));
    return;
  }

  if (browser_list_.empty()) {
    return;
  }

  auto main_browser = browser_list_.front();

  if (auto browser_view = CefBrowserView::GetForBrowser(main_browser)) {
    // Show the window using the Views framework.
    if (auto window = browser_view->GetWindow()) {
      window->Show();
    }
  } else if (is_alloy_style_) {
    PlatformShowWindow(main_browser);
  }
}

void SimpleHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this,
                                       force_close));
    return;
  }

  if (browser_list_.empty()) {
    return;
  }

  for (const auto& browser : browser_list_) {
    browser->GetHost()->CloseBrowser(force_close);
  }
}

#if !defined(OS_MAC)
void SimpleHandler::PlatformShowWindow(CefRefPtr<CefBrowser> browser) {
  NOTIMPLEMENTED();
}
#endif
