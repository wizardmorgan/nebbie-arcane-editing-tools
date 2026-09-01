#include "nebbie/io.hpp"

#include "nebbie/constants.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void copy_fixture_file(const std::filesystem::path& from,
                       const std::filesystem::path& to,
                       const char* target_name) {
    std::filesystem::copy_file(from, to / target_name, std::filesystem::copy_options::overwrite_existing);
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: nebbie-lib-extensions-tests <fixtures-directory>\n";
            return 1;
        }

        const std::filesystem::path fixtures = argv[1];
        const auto out = std::filesystem::temp_directory_path() / "nebbie-lib-extensions-test";
        std::filesystem::remove_all(out);
        std::filesystem::create_directories(out);

        copy_fixture_file(fixtures / nebbie::ZONE_FILE, out, "castelli.zon");
        copy_fixture_file(fixtures / nebbie::WORLD_FILE, out, "castelli.wld");
        copy_fixture_file(fixtures / nebbie::MOB_FILE, out, "castelli.mob");
        copy_fixture_file(fixtures / nebbie::OBJ_FILE, out, "castelli.obj");
        copy_fixture_file(fixtures / nebbie::SHOP_FILE, out, "castelli.shp");
        copy_fixture_file(fixtures / nebbie::SPECIAL_FILE, out, "castelli.spe");

        if (!nebbie::directory_has_lib_files(out)) {
            throw std::runtime_error("directory_has_lib_files failed for castelli.* fixtures");
        }

        nebbie::World world;
        nebbie::LibContext context;
        nebbie::load_lib(world, out, context);

        if (!context.has_any()) {
            throw std::runtime_error("expected LibContext to detect loaded files");
        }
        if (context.zon_path.filename() != "castelli.zon") {
            throw std::runtime_error("expected castelli.zon path in LibContext");
        }
        if (context.wld_path.filename() != "castelli.wld") {
            throw std::runtime_error("expected castelli.wld path in LibContext");
        }
        if (world.zones.empty() || world.rooms.empty() || world.mobiles.empty() || world.objects.empty()) {
            throw std::runtime_error("expected zone/room/mobile/object data from castelli.* files");
        }

        nebbie::save_lib(world, context);
        if (!std::filesystem::exists(out / "castelli.zon")
            || !std::filesystem::exists(out / "castelli.wld")) {
            throw std::runtime_error("save_lib did not preserve castelli.* filenames");
        }

        const auto blank_shop_dir = std::filesystem::temp_directory_path() / "nebbie-lib-blank-shop-test";
        std::filesystem::remove_all(blank_shop_dir);
        std::filesystem::create_directories(blank_shop_dir);
        copy_fixture_file(fixtures / nebbie::ZONE_FILE, blank_shop_dir, "castelli.zon");
        copy_fixture_file(fixtures / nebbie::WORLD_FILE, blank_shop_dir, "castelli.wld");
        copy_fixture_file(fixtures / nebbie::MOB_FILE, blank_shop_dir, "castelli.mob");
        copy_fixture_file(fixtures / nebbie::OBJ_FILE, blank_shop_dir, "castelli.obj");
        {
            std::ofstream blank_shop(blank_shop_dir / nebbie::SHOP_FILE);
            blank_shop << "   \n\n";
        }

        nebbie::World blank_shop_world;
        nebbie::LibContext blank_shop_context;
        nebbie::load_lib(blank_shop_world, blank_shop_dir, blank_shop_context);
        if (blank_shop_context.has_shp) {
            throw std::runtime_error("blank myst.shp should be ignored");
        }
        if (!blank_shop_world.shops.empty()) {
            throw std::runtime_error("blank myst.shp should not load shops");
        }
        if (blank_shop_world.zones.empty() || blank_shop_world.rooms.empty()) {
            throw std::runtime_error("expected core library data despite blank myst.shp");
        }

        const auto broken_shop_dir = std::filesystem::temp_directory_path() / "nebbie-lib-broken-shop-test";
        std::filesystem::remove_all(broken_shop_dir);
        std::filesystem::create_directories(broken_shop_dir);
        copy_fixture_file(fixtures / nebbie::ZONE_FILE, broken_shop_dir, "castelli.zon");
        copy_fixture_file(fixtures / nebbie::WORLD_FILE, broken_shop_dir, "castelli.wld");
        copy_fixture_file(fixtures / nebbie::MOB_FILE, broken_shop_dir, "castelli.mob");
        copy_fixture_file(fixtures / nebbie::OBJ_FILE, broken_shop_dir, "castelli.obj");
        {
            std::ofstream truncated_shop(broken_shop_dir / nebbie::SHOP_FILE);
            truncated_shop << "#1~\n";
        }

        nebbie::World broken_shop_world;
        nebbie::LibContext broken_shop_context;
        nebbie::load_lib(broken_shop_world, broken_shop_dir, broken_shop_context);
        if (broken_shop_context.has_shp) {
            throw std::runtime_error("truncated myst.shp should not be marked loaded");
        }
        if (!broken_shop_context.load_warnings.empty()) {
            if (broken_shop_context.load_warnings.front().find("myst.shp") == std::string::npos) {
                throw std::runtime_error("expected myst.shp warning for truncated shop file");
            }
        } else {
            throw std::runtime_error("expected warning for truncated myst.shp");
        }

        std::cout << "OK\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAILED: " << ex.what() << '\n';
        return 1;
    }
}
