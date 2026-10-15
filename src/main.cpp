#include <cstdlib>
#include <iostream>
#include <limits>
#include <exception>

#include "Config.hpp"
#include "Simulation.hpp"

// cmake -S . -B build && cmake --build build
// ./build/ride_sim

namespace {

// Reads one line of input and converts it with `parse`.
// If the user just presses Enter, `defaultValue` is kept.
// If the input is invalid, the user is asked again.
template <typename T, typename ParseFn>
T promptValue(
    const std::string& label,
    T defaultValue,
    ParseFn parse
) {

    while (true) {

        std::cout
            << label
            << " [default: "
            << defaultValue
            << "]: ";

        std::string line;
        std::getline(std::cin, line);

        if (line.empty()) {
            return defaultValue;
        }

        try {
            return parse(line);
        }
        catch (...) {
            std::cout
                << "  Invalid value, please try again.\n";
        }
    }
}

// Interactively asks the user whether to run serial or
// parallel, and lets them override drivers/riders/ticks/
// threads/request-probability. Pressing Enter on any
// question keeps the value loaded from config.json.
void promptForConfig(Config& config) {

    std::cout
        << "========================================\n";
    std::cout
        << " Simulation Setup\n";
    std::cout
        << "========================================\n";

    std::cout << "Select matching mode:\n";
    std::cout << "  1) Serial\n";
    std::cout << "  2) Parallel (OpenMP)\n";
    std::cout << "  3) Parallel (MPI)\n";

    std::string modeDefault =
        (config.matchingMode == "serial")   ? "1" :
        (config.matchingMode == "parallel") ? "2" : "3";

    std::string modeChoice = promptValue<std::string>(
        "Mode (1=Serial, 2=OpenMP, 3=MPI)",
        modeDefault,
        [](const std::string& s) {
            if (s != "1" && s != "2" && s != "3") {
                throw std::runtime_error("bad mode");
            }
            return s;
        }
    );

    config.matchingMode =
        (modeChoice == "1") ? "serial" :
        (modeChoice == "2") ? "parallel" : "mpi";

    config.drivers = promptValue<int>(
        "Number of drivers",
        config.drivers,
        [](const std::string& s) {
            int v = std::stoi(s);
            if (v <= 0) throw std::runtime_error("bad");
            return v;
        }
    );

    config.riders = promptValue<int>(
        "Number of riders",
        config.riders,
        [](const std::string& s) {
            int v = std::stoi(s);
            if (v <= 0) throw std::runtime_error("bad");
            return v;
        }
    );

    config.ticks = promptValue<int>(
        "Number of simulation ticks",
        config.ticks,
        [](const std::string& s) {
            int v = std::stoi(s);
            if (v <= 0) throw std::runtime_error("bad");
            return v;
        }
    );

    config.requestProbability = promptValue<double>(
        "Ride request probability (0.0 - 1.0)",
        config.requestProbability,
        [](const std::string& s) {
            double v = std::stod(s);
            if (v < 0.0 || v > 1.0) {
                throw std::runtime_error("bad");
            }
            return v;
        }
    );

    if (config.matchingMode == "parallel") {

        config.threads = promptValue<int>(
            "Number of OpenMP threads",
            config.threads,
            [](const std::string& s) {
                int v = std::stoi(s);
                if (v <= 0) throw std::runtime_error("bad");
                return v;
            }
        );

    } else {

        // Threads are irrelevant outside OpenMP mode, but keep
        // the value at 1 so it is reported correctly.
        config.threads = 1;
    }

    if (config.matchingMode == "mpi") {

        config.processes = promptValue<int>(
            "Number of MPI processes",
            config.processes,
            [](const std::string& s) {
                int v = std::stoi(s);
                if (v <= 0) throw std::runtime_error("bad");
                return v;
            }
        );

    } else {

        // Processes are irrelevant outside MPI mode, but keep
        // the value at 1 so it is reported correctly if printed.
        config.processes = 1;
    }

    std::cout
        << "========================================\n\n";
}

// Runs the MPI build of the simulator (src/main_mpi.cpp, already built as
// the separate `mpi_ride_sim` target) with the parameters the user just
// chose interactively above.
//
// This is a deliberate design choice, not a workaround: every MPI process
// launched by `mpirun` runs its own independent copy of main(), so if the
// MPI path tried to reuse this file's interactive std::cin prompts, every
// rank would compete for the same terminal input -- that does not work
// reliably. Instead, only this ONE (non-MPI) process prompts the user,
// writes the chosen parameters to config/config.json via Config::save(),
// and then hands off to `mpirun`, which launches `mpi_ride_sim` -- a
// completely separate MPI-aware executable (src/main_mpi.cpp +
// src/MatchingMPI.cpp) that reads that same config.json on each rank via
// MPI_Bcast, exactly as documented in src/main_mpi.cpp's own comments.
//
// This also means the interactive `ride_sim` executable itself never
// needs to link against MPI, so the existing Serial and OpenMP code paths
// above (Simulation.cpp / Matching.cpp) are completely untouched by this.
int runMpiMode(const Config& config) {

    Config::save("config/config.json", config);

    std::string command =
        "mpirun --allow-run-as-root --oversubscribe -np " +
        std::to_string(config.processes) +
        " ./build/mpi_ride_sim";

    std::cout
        << "Launching MPI build with "
        << config.processes
        << " process(es)...\n";

    std::cout
        << "  "
        << command
        << "\n\n";

    int exitCode = std::system(command.c_str());

    if (exitCode != 0) {

        std::cout
            << "\nmpirun exited with a non-zero status. If this is the "
               "first MPI run, make sure the mpi_ride_sim target has been "
               "built (cmake -S . -B build && cmake --build build) and "
               "that an MPI implementation (e.g. OpenMPI: `sudo apt "
               "install libopenmpi-dev openmpi-bin`) is installed.\n";

        return 1;
    }

    return 0;
}

}

int main() {

    try {

        Config config =
            Config::load("config/config.json");

        promptForConfig(config);

        std::cout
            << "========================================\n";

        std::cout
            << " Real-Time Ride Matching Simulator\n";

        std::cout
            << "========================================\n";

        std::cout
            << "Drivers : "
            << config.drivers
            << '\n';

        std::cout
            << "Riders  : "
            << config.riders
            << '\n';

        std::cout
            << "Ticks   : "
            << config.ticks
            << '\n';

        std::cout
            << "Mode    : "
            << (config.matchingMode == "serial" ? "SERIAL" :
                config.matchingMode == "parallel" ? "OPENMP PARALLEL" :
                "MPI PARALLEL")
            << '\n';

        if (config.matchingMode == "mpi") {

            std::cout
                << "Processes : "
                << config.processes
                << '\n';

        } else {

            std::cout
                << "Threads : "
                << config.threads
                << '\n';
        }

        std::cout
            << "Request probability : "
            << config.requestProbability
            << '\n';

        std::cout
            << "========================================\n\n";

        // MPI mode hands off to a separate MPI-aware executable (see
        // runMpiMode()'s comment above) instead of using the Simulation
        // class below, which only knows about Serial and OpenMP.
        if (config.matchingMode == "mpi") {
            return runMpiMode(config);
        }

        Simulation simulation(config);

        simulation.run();

        std::cout
            << "\n========== BENCHMARK RESULT ==========\n";

        std::cout
            << "Matched rides       : "
            << simulation.getTotalMatchedRides()
            << '\n';

        std::cout
            << "Total matching time : "
            << simulation.getTotalMatchingTimeMs()
            << " ms\n";

        std::cout
            << "Average match/tick  : "
            << simulation.getAverageMatchingTimeMs()
            << " ms\n";

        std::cout
            << "======================================\n";

    }
    catch (const std::exception& exception) {

        std::cerr
            << "ERROR: "
            << exception.what()
            << '\n';

        return 1;
    }

    return 0;
}