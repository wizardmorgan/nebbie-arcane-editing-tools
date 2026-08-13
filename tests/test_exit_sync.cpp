#include "nebbie/edit.hpp"
#include "nebbie/validate.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        nebbie::World world;

        nebbie::Room destination;
        destination.vnum = 34020;
        destination.name = "Di fronte alla Roccaforte della Marca Orientale";
        destination.description = "Una piazza ampia.";
        destination.sector_type = 1;
        world.rooms.emplace(destination.vnum, destination);

        nebbie::Room source;
        source.vnum = 34021;
        source.name = "Strada verso est";
        source.description = "Una strada.";
        source.sector_type = 1;
        nebbie::Exit exit;
        exit.direction = 0;
        exit.description = "di fronte al castello";
        exit.to_room = 34020;
        source.exits.push_back(exit);
        world.rooms.emplace(source.vnum, source);

        const std::size_t updated = nebbie::refresh_inbound_exit_descriptions(world, 34020);
        if (updated != 1) {
            throw std::runtime_error("expected one inbound exit update");
        }

        const nebbie::Room* reloaded = world.find_room(34021);
        if (!reloaded || reloaded->exits.empty()) {
            throw std::runtime_error("source room missing after sync");
        }
        if (reloaded->exits.front().description != destination.name) {
            throw std::runtime_error("inbound exit description was not synchronized");
        }

        nebbie::RoomEdit edit;
        edit.name = "Nuovo titolo destinazione";
        if (!nebbie::edit_room(world, 34020, edit)) {
            throw std::runtime_error("edit_room failed");
        }
        if (world.find_room(34021)->exits.front().description != edit.name) {
            throw std::runtime_error("edit_room did not refresh inbound exits");
        }

        world.find_room(34021)->exits.front().description = "etichetta personalizzata";
        const nebbie::ValidationReport report = nebbie::validate_world(world);
        bool found_warning = false;
        for (const auto& issue : report.issues) {
            if (issue.category == "room" && issue.severity == nebbie::ValidationSeverity::warning
                && issue.message.find("has description") != std::string::npos) {
                found_warning = true;
                break;
            }
        }
        if (!found_warning) {
            throw std::runtime_error("expected validation warning for mismatched exit label");
        }

        world.find_room(34021)->exits.front().description = "nuovo titolo destinazione";
        const std::size_t realigned = nebbie::refresh_inbound_exit_descriptions(world, 34020);
        if (realigned != 0) {
            throw std::runtime_error("case-only exit label mismatch should not trigger alignment");
        }

        std::cout << "OK\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAILED: " << ex.what() << '\n';
        return 1;
    }
}
