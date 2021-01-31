//
// Created by fs on 1/30/21.
//

#pragma once

#include <functional>
#include <map>
#include <string>

class CmdProcessor {
 public:
  std::string DumpCmd(const std::string &delimiter = " ") {
    std::string res;
    for (auto &i : cmd_list_) {
      res += i.first + delimiter;
    }
    res.erase(res.length() - delimiter.length());
    return res;
  }
  std::string DumpHelp(const std::string &delimiter = "\n") {
    std::string res;
    for (auto &i : cmd_list_) {
      res += i.first + " - ";
      res += i.second.first + delimiter;
    }
    return res;
  }
  auto Add(const std::string &cmd, const std::function<void()> &action,
           const std::string &comment = "") -> decltype(*this) {
    cmd_list_[cmd] = std::make_pair(comment, action);
    return *this;
  }

  bool ProcessCmd(const std::string &cmd) {
    if (cmd_list_.count(cmd)) {
      cmd_list_[cmd].second();
      return true;
    }
    return false;
  }

 private:
  std::map<std::string, std::pair<std::string, std::function<void()>>>
      cmd_list_;
};
