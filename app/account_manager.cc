#include "account_manager.h"

#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

#pragma comment(lib,"bcrypt.lib")

namespace {
std::string Hex(const unsigned char* bytes, size_t size) {
  constexpr char digits[]="0123456789abcdef";
  std::string out; out.reserve(size*2);
  for(size_t i=0;i<size;++i) { out+=digits[bytes[i]>>4]; out+=digits[bytes[i]&15]; }
  return out;
}
bool Unhex(const std::string& input, std::array<unsigned char,16>& result) {
  if(input.size()!=32) return false;
  for(size_t i=0;i<16;++i) {
    auto digit=[](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; return -1; };
    int a=digit(input[i*2]),b=digit(input[i*2+1]);
    if(a<0||b<0) return false;
    result[i]=static_cast<unsigned char>((a<<4)|b);
  }
  return true;
}
std::string HashPassword(const std::string& password, const std::string& salt) {
  std::array<unsigned char,16> salt_bytes{};
  if(!Unhex(salt,salt_bytes)) return {};
  BCRYPT_ALG_HANDLE algorithm=nullptr;
  if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG)<0) return {};
  std::array<unsigned char,32> digest{};
  NTSTATUS status=BCryptDeriveKeyPBKDF2(algorithm,
      reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),static_cast<ULONG>(password.size()),
      salt_bytes.data(),static_cast<ULONG>(salt_bytes.size()),200000,
      digest.data(),static_cast<ULONG>(digest.size()),0);
  BCryptCloseAlgorithmProvider(algorithm,0);
  return status<0 ? std::string() : Hex(digest.data(),digest.size());
}
bool Matches(const std::string& password,const AccountManager::Account& account) {
  const std::string actual=HashPassword(password,account.salt);
  if(actual.size()!=account.hash.size() || actual.empty()) return false;
  unsigned char difference=0;
  for(size_t i=0;i<actual.size();++i) difference|=static_cast<unsigned char>(actual[i]^account.hash[i]);
  return difference==0;
}
bool ValidId(const std::string& id) {
  return id.size()==32 && std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
}

AccountManager& AccountManager::Get() { static AccountManager instance; return instance; }
std::filesystem::path AccountManager::Root() const {
  wchar_t value[MAX_PATH]={};
  if(!GetEnvironmentVariableW(L"LOCALAPPDATA",value,MAX_PATH)) return {};
  return std::filesystem::path(value)/L"CYBROWSER";
}
std::filesystem::path AccountManager::DataPath() const {
  return active_id_.empty()?Root():Root()/L"accounts"/active_id_;
}
std::filesystem::path AccountManager::CachePath() const {
  return active_id_.empty()?Root():DataPath()/L"cache";
}
AccountManager::AccountManager() {
  std::filesystem::create_directories(Root()/L"accounts");
  std::ifstream file(Root()/L"accounts.txt",std::ios::binary);
  Account account;
  while(file >> std::quoted(account.id) >> std::quoted(account.name)
             >> std::quoted(account.salt) >> std::quoted(account.hash)) {
    if(ValidId(account.id)) accounts_.push_back(account);
  }
  std::ifstream active(Root()/L"active.txt",std::ios::binary);
  std::getline(active,active_id_);
  if(std::none_of(accounts_.begin(),accounts_.end(),[&](const Account& a){return a.id==active_id_;})) active_id_.clear();
}
std::string AccountManager::ActiveName() const {
  for(const auto& account:accounts_) if(account.id==active_id_) return account.name;
  return "访客";
}
void AccountManager::SaveAccounts() const {
  std::ofstream file(Root()/L"accounts.txt",std::ios::binary|std::ios::trunc);
  for(const auto& account:accounts_) file << std::quoted(account.id) << ' ' << std::quoted(account.name) << ' '
    << std::quoted(account.salt) << ' ' << std::quoted(account.hash) << '\n';
}
void AccountManager::SaveActive() const { std::ofstream(Root()/L"active.txt",std::ios::binary|std::ios::trunc) << active_id_; }
bool AccountManager::Register(const std::string& name,const std::string& password,std::string& error) {
  if(name.empty()||name.size()>80||password.size()<8||password.size()>256) { error="名称须为 1–80 字节，密码须为 8–256 字节。"; return false; }
  if(std::any_of(accounts_.begin(),accounts_.end(),[&](const Account& a){return a.name==name;})) { error="账户名称已存在。"; return false; }
  std::array<unsigned char,16> id{},salt{};
  if(BCryptGenRandom(nullptr,id.data(),16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0 ||
     BCryptGenRandom(nullptr,salt.data(),16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) { error="无法生成安全随机数。"; return false; }
  Account account{Hex(id.data(),id.size()),name,Hex(salt.data(),salt.size()),{}};
  account.hash=HashPassword(password,account.salt);
  if(account.hash.empty()) { error="密码处理失败。"; return false; }
  std::filesystem::create_directories(Root()/L"accounts"/account.id);
  accounts_.push_back(account); SaveAccounts();
  active_id_=account.id; SaveActive();
  return true;
}
bool AccountManager::Login(const std::string& id,const std::string& password,std::string& error) {
  auto it=std::find_if(accounts_.begin(),accounts_.end(),[&](const Account& a){return a.id==id;});
  if(it==accounts_.end()||!Matches(password,*it)) { error="账户或密码错误。"; return false; }
  active_id_=id; SaveActive(); return true;
}
bool AccountManager::Delete(const std::string& id,const std::string& password,std::string& error) {
  auto it=std::find_if(accounts_.begin(),accounts_.end(),[&](const Account& a){return a.id==id;});
  if(it==accounts_.end()||!Matches(password,*it)) { error="密码错误，未删除账户。"; return false; }
  const bool active=id==active_id_;
  accounts_.erase(it); SaveAccounts();
  if(active) { active_id_.clear(); SaveActive(); }
  std::ofstream(Root()/L"pending_delete.txt",std::ios::binary|std::ios::app) << id << '\n';
  if(!active) ProcessPendingDeletes();
  return true;
}
void AccountManager::Logout() { active_id_.clear(); SaveActive(); }
void AccountManager::ProcessPendingDeletes() {
  const auto pending=Root()/L"pending_delete.txt";
  std::ifstream file(pending,std::ios::binary);
  std::vector<std::string> remaining;
  std::string id;
  while(std::getline(file,id)) if(ValidId(id)) {
    bool removed=false;
    for(int attempt=0;attempt<20 && !removed;++attempt) {
      std::error_code error;
      std::filesystem::remove_all(Root()/L"accounts"/id,error);
      removed=!error;
      if(!removed) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    if(!removed) remaining.push_back(id);
  }
  file.close();
  std::ofstream output(pending,std::ios::binary|std::ios::trunc);
  for(const auto& entry:remaining) output << entry << '\n';
}
