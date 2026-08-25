#include "nebbie/system_field_config.hpp"
#include "nebbie/world.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        using nebbie::EditTarget;
        using nebbie::SystemFieldKind;
        using nebbie::default_system_field_config;
        using nebbie::edit_targets_label;
        using nebbie::mob_field_allowed;
        using nebbie::object_affect_allowed;
        using nebbie::parse_edit_targets;
        using nebbie::read_system_field_config;
        using nebbie::scan_field_migration;
        using nebbie::set_field_targets;
        using nebbie::strip_disallowed_object_affects;
        using nebbie::targets_allow;
        using nebbie::targets_for_field;
        using nebbie::write_system_field_config;

        expect(parse_edit_targets("object,mob,pg")
                   == (static_cast<unsigned>(EditTarget::Object) | static_cast<unsigned>(EditTarget::Mob)
                       | static_cast<unsigned>(EditTarget::Pg)),
               "parse all targets");
        expect(edit_targets_label(parse_edit_targets("object,mob")) == "object,mob", "label roundtrip");

        auto config = default_system_field_config();
        expect(!config.definitions.empty(), "definitions present");
        expect(targets_allow(targets_for_field(config, "resistance.fire"), EditTarget::Object),
               "default fire resistance on objects");

        set_field_targets(config, "resistance.fire", static_cast<unsigned>(EditTarget::Mob));
        expect(!targets_allow(targets_for_field(config, "resistance.fire"), EditTarget::Object),
               "fire resistance moved off objects");
        expect(targets_allow(targets_for_field(config, "resistance.fire"), EditTarget::Mob),
               "fire resistance enabled on mobs");

        for (const auto& def : config.definitions) {
            if (def.id == "resistance.fire") {
                expect(mob_field_allowed(config, def), "mob fire resistance allowed");
                break;
            }
        }

        expect(object_affect_allowed(config, 17, -10), "unrelated apply allowed");

        set_field_targets(config, "resistance.fire", static_cast<unsigned>(EditTarget::Mob));
        expect(!object_affect_allowed(config, 26, 1), "fire resistance object affect blocked");

        nebbie::World world;
        nebbie::GameObject obj;
        obj.vnum = 100;
        obj.affects.push_back({26, 1});
        world.objects.emplace(obj.vnum, obj);

        const auto previous = default_system_field_config();
        const auto report = scan_field_migration(world, previous, config);
        expect(report.object_affects_to_strip >= 1, "migration detects object affect");

        expect(strip_disallowed_object_affects(world, config) >= 1, "strip removes object affect");
        expect(world.objects.at(100).affects.empty(), "object affect cleared");

        const std::filesystem::path temp_path =
            std::filesystem::temp_directory_path() / "nebbie-system-field-config-test.conf";
        expect(write_system_field_config(temp_path, config), "write config");
        const auto loaded = read_system_field_config(temp_path);
        expect(targets_for_field(loaded, "resistance.fire")
                   == static_cast<unsigned>(EditTarget::Mob),
               "reload config");

        std::cout << "OK\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAILED: " << ex.what() << '\n';
        return 1;
    }
}
