#pragma once

#include <filesystem>
#include <string>
#include <vector>

class AccountManager {
 public:
  struct Account { std::string id, name, salt, hash; };
  static AccountManager& Get();
  std::filesystem::path Root() const;
  std::filesystem::path DataPath() const;
  std::filesystem::path CachePath() const;
  std::string ActiveId() const { return active_id_; }
  std::string ActiveName() const;
  const std::vector<Account>& Accounts() const { return accounts_; }
  bool Register(const std::string& name, const std::string& password, std::string& error);
  bool Login(const std::string& id, const std::string& password, std::string& error);
  bool Delete(const std::string& id, const std::string& password, std::string& error);
  void Logout();
  void ProcessPendingDeletes();
 private:
  AccountManager();
  void SaveAccounts() const;
  void SaveActive() const;
  std::vector<Account> accounts_;
  std::string active_id_;
};
