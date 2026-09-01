#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nebbie {

struct LibContext {
    std::filesystem::path root;
    bool has_zon = false;
    bool has_wld = false;
    bool has_mob = false;
    bool has_obj = false;
    bool has_shp = false;
    bool has_spe = false;
    bool has_dam = false;
    bool has_act = false;
    bool has_pos = false;
    bool has_gui = false;

    std::filesystem::path zon_path;
    std::filesystem::path wld_path;
    std::filesystem::path mob_path;
    std::filesystem::path obj_path;
    std::filesystem::path shp_path;
    std::filesystem::path spe_path;
    std::filesystem::path dam_path;
    std::filesystem::path act_path;
    std::filesystem::path pos_path;
    std::filesystem::path gui_path;

    std::unordered_map<int, std::filesystem::path> zone_sources;
    std::unordered_map<long, std::filesystem::path> room_sources;
    std::unordered_map<long, std::filesystem::path> mobile_sources;
    std::unordered_map<long, std::filesystem::path> object_sources;

    std::vector<std::string> load_warnings;

    bool has_any() const;
};

} // namespace nebbie
