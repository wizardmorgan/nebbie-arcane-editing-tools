#include "nebbie/io.hpp"
#include "nebbie/overlay_io.hpp"

#include "nebbie/constants.hpp"
#include "nebbie/file_io.hpp"
#include "nebbie/fread.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <vector>

namespace nebbie {

namespace {

bool is_lib_data_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return false;
    }
    return is_lib_file_extension(path.extension().string());
}

std::vector<std::filesystem::path> discover_files_by_extension(
    const std::filesystem::path& dir,
    const char* extension,
    const char* canonical_name) {
    std::error_code ec;
    const std::filesystem::path canonical = dir / canonical_name;
    if (std::filesystem::exists(canonical, ec)) {
        return {canonical};
    }

    std::vector<std::filesystem::path> matches;
    if (!std::filesystem::is_directory(dir, ec)) {
        return matches;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        if (entry.path().extension().string() == extension) {
            matches.push_back(entry.path());
        }
    }

    std::sort(matches.begin(), matches.end());
    return matches;
}

bool directory_contains_lib_files(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory(ec)) {
            continue;
        }
        if (is_lib_data_file(entry.path())) {
            return true;
        }
    }
    return false;
}

std::vector<long> scan_hash_vnums(const std::filesystem::path& path) {
    std::vector<long> vnums;
    FILE* fp = open_file_read(path, "library file");
    while (true) {
        const std::string line = fread_line(fp);
        if (line.empty()) {
            if (std::feof(fp)) {
                break;
            }
            continue;
        }
        if (line[0] != '#') {
            continue;
        }
        if (line == "#$" || line == "#%" || line == "#0") {
            break;
        }
        try {
            const long vnum = std::stol(line.substr(1));
            if (vnum > 0) {
                vnums.push_back(vnum);
            }
        } catch (...) {
            continue;
        }
    }
    std::fclose(fp);
    return vnums;
}

void assign_long_sources(std::unordered_map<long, std::filesystem::path>& sources,
                         const std::filesystem::path& path,
                         const std::vector<long>& vnums) {
    for (const long vnum : vnums) {
        sources[vnum] = path;
    }
}

void assign_int_sources(std::unordered_map<int, std::filesystem::path>& sources,
                        const std::filesystem::path& path,
                        const std::vector<long>& vnums) {
    for (const long vnum : vnums) {
        sources[static_cast<int>(vnum)] = path;
    }
}

void track_zone_sources(LibContext& context,
                        const std::filesystem::path& path,
                        const std::vector<long>& vnums) {
    assign_int_sources(context.zone_sources, path, vnums);
}

void track_room_sources(LibContext& context,
                        const std::filesystem::path& path,
                        const std::vector<long>& vnums) {
    assign_long_sources(context.room_sources, path, vnums);
}

void track_mobile_sources(LibContext& context,
                          const std::filesystem::path& path,
                          const std::vector<long>& vnums) {
    assign_long_sources(context.mobile_sources, path, vnums);
}

void track_object_sources(LibContext& context,
                          const std::filesystem::path& path,
                          const std::vector<long>& vnums) {
    assign_long_sources(context.object_sources, path, vnums);
}

bool file_has_non_whitespace_content(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    char byte = '\0';
    while (input.get(byte)) {
        if (!std::isspace(static_cast<unsigned char>(byte))) {
            return true;
        }
    }
    return false;
}

template <typename Loader>
void load_discovered_files(World& world,
                           LibContext& context,
                           const std::vector<std::filesystem::path>& paths,
                           bool& has_flag,
                           std::filesystem::path& primary_path,
                           Loader loader,
                           bool optional = false) {
    if (paths.empty()) {
        return;
    }

    bool loaded_any = false;
    primary_path = paths.front().filename();
    for (std::size_t i = 0; i < paths.size(); ++i) {
        std::error_code size_ec;
        if (std::filesystem::file_size(paths[i], size_ec) == 0) {
            continue;
        }
        if (!file_has_non_whitespace_content(paths[i])) {
            continue;
        }
        try {
            loader(paths[i], !loaded_any);
            loaded_any = true;
        } catch (const std::exception& ex) {
            const std::string message =
                std::string(ex.what()) + " in " + paths[i].filename().string();
            if (optional) {
                context.load_warnings.push_back(message);
                break;
            }
            throw std::runtime_error(message);
        }
    }

    has_flag = loaded_any;
    if (!loaded_any) {
        primary_path.clear();
    }
}

World make_room_subset(const World& world, const std::vector<long>& vnums) {
    World subset;
    subset.zones = world.zones;
    for (const long vnum : vnums) {
        if (const Room* room = world.find_room(vnum)) {
            subset.rooms.emplace(vnum, *room);
        }
    }
    return subset;
}

World make_zone_subset(const World& world, const std::vector<int>& zone_nums) {
    World subset;
    for (const int zone_num : zone_nums) {
        for (const Zone& zone : world.zones) {
            if (zone.num == zone_num) {
                subset.zones.push_back(zone);
                break;
            }
        }
    }
    return subset;
}

World make_mobile_subset(const World& world, const std::vector<long>& vnums) {
    World subset;
    for (const long vnum : vnums) {
        if (const Mobile* mobile = world.find_mobile(vnum)) {
            subset.mobiles.emplace(vnum, *mobile);
        }
    }
    return subset;
}

World make_object_subset(const World& world, const std::vector<long>& vnums) {
    World subset;
    for (const long vnum : vnums) {
        if (const GameObject* object = world.find_object(vnum)) {
            subset.objects.emplace(vnum, *object);
        }
    }
    return subset;
}

template <typename Key>
std::map<std::filesystem::path, std::vector<Key>> group_by_source_path(
    const std::unordered_map<Key, std::filesystem::path>& sources) {
    std::map<std::filesystem::path, std::vector<Key>> grouped;
    for (const auto& [key, path] : sources) {
        grouped[path].push_back(key);
    }
    return grouped;
}

void save_tracked_rooms(const World& world,
                        const LibContext& context,
                        ProgressCallback progress) {
    const std::filesystem::path primary = context.wld_path.empty()
                                              ? std::filesystem::path(WORLD_FILE)
                                              : context.wld_path;
    if (context.room_sources.empty()) {
        save_myst_wld(world, context.root / primary, progress);
        return;
    }

    auto grouped = group_by_source_path(context.room_sources);
    std::vector<long>& primary_vnums = grouped[primary];
    for (const auto& [vnum, _] : world.rooms) {
        if (context.room_sources.find(vnum) == context.room_sources.end()) {
            primary_vnums.push_back(vnum);
        }
    }
    for (const auto& [filename, vnums] : grouped) {
        save_myst_wld(make_room_subset(world, vnums), context.root / filename, progress);
    }
}

void save_tracked_zones(const World& world,
                        const LibContext& context,
                        ProgressCallback progress) {
    const std::filesystem::path primary = context.zon_path.empty() ? std::filesystem::path(ZONE_FILE)
                                                                   : context.zon_path;
    if (context.zone_sources.empty()) {
        save_myst_zon(world, context.root / primary, progress);
        return;
    }

    auto grouped = group_by_source_path(context.zone_sources);
    for (const auto& [filename, zone_nums] : grouped) {
        save_myst_zon(make_zone_subset(world, zone_nums), context.root / filename, progress);
    }
}

void save_tracked_mobiles(const World& world,
                          const LibContext& context,
                          ProgressCallback progress) {
    const std::filesystem::path primary = context.mob_path.empty() ? std::filesystem::path(MOB_FILE)
                                                                   : context.mob_path;
    if (context.mobile_sources.empty()) {
        save_myst_mob(world, context.root / primary, progress);
        return;
    }

    auto grouped = group_by_source_path(context.mobile_sources);
    std::vector<long>& primary_vnums = grouped[primary];
    for (const auto& [vnum, _] : world.mobiles) {
        if (context.mobile_sources.find(vnum) == context.mobile_sources.end()) {
            primary_vnums.push_back(vnum);
        }
    }
    for (const auto& [filename, vnums] : grouped) {
        save_myst_mob(make_mobile_subset(world, vnums), context.root / filename, progress);
    }
}

void save_tracked_objects(const World& world,
                          const LibContext& context,
                          ProgressCallback progress) {
    const std::filesystem::path primary = context.obj_path.empty() ? std::filesystem::path(OBJ_FILE)
                                                                   : context.obj_path;
    if (context.object_sources.empty()) {
        save_myst_obj(world, context.root / primary, progress);
        return;
    }

    auto grouped = group_by_source_path(context.object_sources);
    std::vector<long>& primary_vnums = grouped[primary];
    for (const auto& [vnum, _] : world.objects) {
        if (context.object_sources.find(vnum) == context.object_sources.end()) {
            primary_vnums.push_back(vnum);
        }
    }
    for (const auto& [filename, vnums] : grouped) {
        save_myst_obj(make_object_subset(world, vnums), context.root / filename, progress);
    }
}

std::filesystem::path save_path_for(const LibContext& context,
                                    const std::filesystem::path& stored_path,
                                    const char* fallback_name) {
    const std::filesystem::path filename =
        stored_path.empty() ? std::filesystem::path(fallback_name) : stored_path;
    return context.root / filename;
}

} // namespace

bool directory_has_lib_files(const std::filesystem::path& dir) {
    return directory_contains_lib_files(dir);
}

std::filesystem::path resolve_lib_directory(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path candidate = path;
    if (std::filesystem::is_regular_file(candidate, ec) && is_lib_data_file(candidate)) {
        candidate = candidate.parent_path();
    }

    if (directory_contains_lib_files(candidate)) {
        return candidate;
    }

    const auto lib_subdir = candidate / "lib";
    if (directory_contains_lib_files(lib_subdir)) {
        return lib_subdir;
    }

    return candidate;
}

void load_lib(World& world, const std::filesystem::path& lib_root, ProgressCallback progress) {
    LibContext context;
    load_lib(world, lib_root, context, progress);
}

void load_lib(World& world,
              const std::filesystem::path& lib_root,
              LibContext& context,
              ProgressCallback progress) {
    world.clear();
    context = {};

    const std::filesystem::path resolved = resolve_lib_directory(lib_root);
    context.root = resolved;

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, ZONE_EXT, ZONE_FILE),
        context.has_zon,
        context.zon_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            const std::vector<long> vnums = scan_hash_vnums(path);
            load_myst_zon(world, path, progress, clear_existing);
            track_zone_sources(context, path.filename(), vnums);
        });

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, WORLD_EXT, WORLD_FILE),
        context.has_wld,
        context.wld_path,
        [&](const std::filesystem::path& path, bool /*clear_existing*/) {
            const std::vector<long> vnums = scan_hash_vnums(path);
            load_myst_wld(world, path, progress);
            track_room_sources(context, path.filename(), vnums);
        });

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, MOB_EXT, MOB_FILE),
        context.has_mob,
        context.mob_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            const std::vector<long> vnums = scan_hash_vnums(path);
            load_myst_mob(world, path, progress, clear_existing);
            track_mobile_sources(context, path.filename(), vnums);
        });

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, OBJ_EXT, OBJ_FILE),
        context.has_obj,
        context.obj_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            const std::vector<long> vnums = scan_hash_vnums(path);
            load_myst_obj(world, path, progress, clear_existing);
            track_object_sources(context, path.filename(), vnums);
        });

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, SHOP_EXT, SHOP_FILE),
        context.has_shp,
        context.shp_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            load_myst_shp(world, path, progress, clear_existing);
            if (context.shp_path.empty()) {
                context.shp_path = path.filename();
            }
        },
        true);

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, SPECIAL_EXT, SPECIAL_FILE),
        context.has_spe,
        context.spe_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            load_myst_spe(world, path, progress, clear_existing);
            if (context.spe_path.empty()) {
                context.spe_path = path.filename();
            }
        },
        true);

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, DAMAGE_EXT, DAMAGE_FILE),
        context.has_dam,
        context.dam_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            load_myst_dam(world, path, progress, clear_existing);
            if (context.dam_path.empty()) {
                context.dam_path = path.filename();
            }
        },
        true);

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, SOCIAL_EXT, SOCIAL_FILE),
        context.has_act,
        context.act_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            load_myst_act(world, path, progress, clear_existing);
            if (context.act_path.empty()) {
                context.act_path = path.filename();
            }
        },
        true);

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, POSE_EXT, POSE_FILE),
        context.has_pos,
        context.pos_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            load_myst_pos(world, path, progress, clear_existing);
            if (context.pos_path.empty()) {
                context.pos_path = path.filename();
            }
        },
        true);

    load_discovered_files(
        world,
        context,
        discover_files_by_extension(resolved, GUILD_EXT, GUILD_FILE),
        context.has_gui,
        context.gui_path,
        [&](const std::filesystem::path& path, bool clear_existing) {
            load_myst_gui(world, path, progress, clear_existing);
            if (context.gui_path.empty()) {
                context.gui_path = path.filename();
            }
        },
        true);

    const OverlayImportReport overlay_report = apply_overlays(world, resolved, progress);
    if (progress && (overlay_report.rooms > 0 || overlay_report.objects > 0 || overlay_report.mobiles > 0
                     || overlay_report.zone_resets > 0)) {
        progress("Applied overlays: rooms=" + std::to_string(overlay_report.rooms) + " objects="
                 + std::to_string(overlay_report.objects) + " mobiles="
                 + std::to_string(overlay_report.mobiles) + " zone_resets="
                 + std::to_string(overlay_report.zone_resets));
    }
}

void save_lib(const World& world, const LibContext& context, ProgressCallback progress) {
    if (context.has_zon) {
        save_tracked_zones(world, context, progress);
    }
    if (context.has_wld) {
        save_tracked_rooms(world, context, progress);
    }
    if (context.has_mob) {
        save_tracked_mobiles(world, context, progress);
    }
    if (context.has_obj) {
        save_tracked_objects(world, context, progress);
    }
    if (context.has_shp) {
        save_myst_shp(world, save_path_for(context, context.shp_path, SHOP_FILE), progress);
    }
    if (context.has_spe) {
        save_myst_spe(world, save_path_for(context, context.spe_path, SPECIAL_FILE), progress);
    }
    if (context.has_dam) {
        save_myst_dam(world, save_path_for(context, context.dam_path, DAMAGE_FILE), progress);
    }
    if (context.has_act) {
        save_myst_act(world, save_path_for(context, context.act_path, SOCIAL_FILE), progress);
    }
    if (context.has_pos) {
        save_myst_pos(world, save_path_for(context, context.pos_path, POSE_FILE), progress);
    }
    if (context.has_gui) {
        save_myst_gui(world, save_path_for(context, context.gui_path, GUILD_FILE), progress);
    }
}

} // namespace nebbie
