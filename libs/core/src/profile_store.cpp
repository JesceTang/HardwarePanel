#include "hwpanel/core/profile_store.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>

#include "hwpanel/core/log.h"
#include "hwpanel/core/paths.h"

namespace hwpanel::core {

namespace {

using json = nlohmann::json;

bool ParseProfileFile(const std::filesystem::path& file, Profile& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    return false;
  }
  json doc;
  try {
    in >> doc;
  } catch (const std::exception& e) {
    HWLOG_WARN("profile store: corrupt file {}: {}", file.string(), e.what());
    return false;
  }
  if (!doc.is_object() || !doc.contains("id") || !doc["id"].is_string()) {
    HWLOG_WARN("profile store: malformed file {}", file.string());
    return false;
  }
  out.id = doc.value("id", "");
  out.name = doc.value("name", out.id);
  out.description = doc.value("description", "");
  out.settings.clear();
  if (doc.contains("settings") && doc["settings"].is_object()) {
    for (auto it = doc["settings"].begin(); it != doc["settings"].end(); ++it) {
      if (it.value().is_string()) {
        out.settings[it.key()] = it.value().get<std::string>();
      }
    }
  }
  return true;
}

}  // namespace

ProfileStore::ProfileStore(std::filesystem::path dir) : dir_(std::move(dir)) {
  paths::EnsureDir(dir_);
}

std::vector<Profile> ProfileStore::List() const {
  std::vector<Profile> out;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir_, ec)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".json") {
      continue;
    }
    if (entry.path().filename() == "active.json") {
      continue;
    }
    Profile profile;
    if (ParseProfileFile(entry.path(), profile)) {
      out.push_back(std::move(profile));
    }
  }
  std::sort(out.begin(), out.end(),
            [](const Profile& a, const Profile& b) { return a.id < b.id; });
  return out;
}

bool ProfileStore::Get(const std::string& id, Profile& out) const {
  return ParseProfileFile(dir_ / (id + ".json"), out);
}

bool ProfileStore::Save(const Profile& profile) {
  json doc = json::object();
  doc["id"] = profile.id;
  doc["name"] = profile.name;
  doc["description"] = profile.description;
  json settings = json::object();
  for (const auto& [key, value] : profile.settings) {
    settings[key] = value;
  }
  doc["settings"] = settings;

  const auto file = dir_ / (profile.id + ".json");
  const auto tmp = dir_ / (profile.id + ".json.tmp");
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    out << doc.dump(2);
  }
  std::error_code ec;
  std::filesystem::rename(tmp, file, ec);  // atomic-ish replace
  return !ec;
}

bool ProfileStore::Remove(const std::string& id) {
  std::error_code ec;
  return std::filesystem::remove(dir_ / (id + ".json"), ec) && !ec;
}

std::string ProfileStore::ActiveId() const {
  const auto file = dir_ / "active.json";
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    return "";
  }
  try {
    json doc;
    in >> doc;
    return doc.value("active", "");
  } catch (const std::exception&) {
    return "";
  }
}

void ProfileStore::SetActiveId(const std::string& id) {
  json doc = json::object();
  doc["active"] = id;
  const auto tmp = dir_ / "active.json.tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return;
    }
    out << doc.dump(2);
  }
  std::error_code ec;
  std::filesystem::rename(tmp, dir_ / "active.json", ec);
}

void ProfileStore::EnsureDefaults() {
  if (!List().empty()) {
    return;
  }
  const std::vector<Profile> defaults = {
      {"balanced", "Balanced", "Windows default balanced plan",
       {{"power.plan", "balanced"}}},
      {"performance", "Performance", "High performance plan for gaming/renders",
       {{"power.plan", "high"}}},
      {"powersaver", "Power Saver", "Battery friendly plan",
       {{"power.plan", "saver"}}},
  };
  for (const auto& profile : defaults) {
    Save(profile);
  }
  if (ActiveId().empty()) {
    SetActiveId("balanced");
  }
}

}  // namespace hwpanel::core
