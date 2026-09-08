#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace hwpanel::core {

// A persisted configuration profile: executor key -> desired value.
struct Profile {
  std::string id;
  std::string name;
  std::string description;
  std::map<std::string, std::string> settings;
};

// JSON-on-disk profile repository: <dir>/<id>.json plus <dir>/active.json.
// Corrupt files are skipped with a warning instead of breaking List()
// (plan M4: corrupt-file fallback).
class ProfileStore {
 public:
  explicit ProfileStore(std::filesystem::path dir);

  std::vector<Profile> List() const;
  bool Get(const std::string& id, Profile& out) const;
  bool Save(const Profile& profile);
  bool Remove(const std::string& id);

  std::string ActiveId() const;
  void SetActiveId(const std::string& id);

  // Seeds the built-in profiles when the directory holds none.
  void EnsureDefaults();

  const std::filesystem::path& dir() const { return dir_; }

 private:
  std::filesystem::path dir_;
};

}  // namespace hwpanel::core
