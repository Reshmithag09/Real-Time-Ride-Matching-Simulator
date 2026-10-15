#pragma once

#include <string>

struct Config {
    int drivers;
    int riders;
    int ticks;

    double requestProbability;

    std::string matchingMode;

    int threads;

    // Number of MPI processes to launch when matchingMode == "mpi".
    // Unused by the serial and OpenMP paths (left untouched by this field).
    int processes;

    static Config load(const std::string& filename);

    // Writes the current config back to `filename` as JSON, in the same
    // shape `load()` expects. Used by the interactive MPI mode (main.cpp)
    // to hand the chosen parameters to the separate mpi_ride_sim binary,
    // which reads config/config.json on its own at startup.
    static void save(const std::string& filename, const Config& config);
};