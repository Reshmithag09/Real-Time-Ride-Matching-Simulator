
#include "Matching.hpp"

#include <iostream>
#include <limits>
#include <cmath>
#include <omp.h>

#include "Geo.hpp"


int MatchingEngine::findNearestDriver(
    const std::vector<Driver>& drivers,
    const Rider& rider
) const {

    int nearestDriverIndex = -1;

    // compared squared distances, so sqrt() is avoided
    // during the driver search.
    double minimumDistanceSquared =
        std::numeric_limits<double>::max();

    const Position riderPickup = rider.getPickup();

    for (int i = 0; i < static_cast<int>(drivers.size()); ++i) {

        if (drivers[i].getStatus() != DriverStatus::IDLE) {
            continue;
        }

        const Position driverPosition =
            drivers[i].getPosition();

        const double dx =
            driverPosition.x - riderPickup.x;

        const double dy =
            driverPosition.y - riderPickup.y;

        const double distanceSquared =
            dx * dx + dy * dy;

        if (distanceSquared < minimumDistanceSquared) {
            minimumDistanceSquared = distanceSquared;
            nearestDriverIndex = i;
        }
    }

    return nearestDriverIndex;
}


int MatchingEngine::matchRidesSerial(
    std::vector<Driver>& drivers,
    std::vector<Rider>& riders,
    std::vector<Ride>& rides
) {

    int matchedRides = 0;

    for (Ride& ride : rides) {

        if (ride.getStatus() != RideStatus::REQUESTED) {
            continue;
        }

        /*
         * Rider IDs are assigned as i + 1 and riders are stored
         * in the same order, so Rider ID N corresponds to
         * riders[N - 1].
         *
         * This replaces the previous O(riders) linear search.
         */
        const int riderIndex =
            ride.getRiderId() - 1;

        if (riderIndex < 0 ||
            riderIndex >= static_cast<int>(riders.size())) {
            continue;
        }

        Rider* rider = &riders[riderIndex];

        int driverIndex = findNearestDriver(
            drivers,
            *rider
        );

        if (driverIndex == -1) {
            continue;
        }

        Driver& driver = drivers[driverIndex];

        double distance = euclideanDistance(
            driver.getPosition(),
            rider->getPickup()
        );

        driver.setStatus(DriverStatus::BUSY);
        rider->setStatus(RiderStatus::MATCHED);

        ride.setDriverId(driver.getId());
        ride.setStatus(RideStatus::MATCHED);

        ++matchedRides;

        std::cout
            << "  [SERIAL] Ride "
            << ride.getId()
            << " matched: Driver "
            << driver.getId()
            << " -> Rider "
            << rider->getId()
            << " (distance: "
            << distance
            << ")\n";
    }

    return matchedRides;
}


int MatchingEngine::matchRidesParallel(
    std::vector<Driver>& drivers,
    std::vector<Rider>& riders,
    std::vector<Ride>& rides
) {

    const int rideCount =
        static_cast<int>(rides.size());

    const int driverCount =
        static_cast<int>(drivers.size());

    std::vector<int> candidateDriver(
        rideCount,
        -1
    );

    /*
     * Store squared distance during Phase 1.
     *
     * We do not need the actual distance to determine
     * which driver is nearest.
     */
    std::vector<double> candidateDistanceSquared(
        rideCount,
        std::numeric_limits<double>::max()
    );

    /*
     * PHASE 1:
     *
     * Each requested ride independently searches for its
     * nearest currently-idle driver.
     *
     * No driver/rider/ride state is modified here.
     *
     * Optimization:
     * - Rider lookup is O(1)
     * - Distance comparison avoids sqrt()
     */
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < rideCount; ++i) {

        const Ride& ride = rides[i];

        if (ride.getStatus() != RideStatus::REQUESTED) {
            continue;
        }

        /*
         * Rider IDs are 1-based and correspond directly
         * to vector indices.
         */
        const int riderIndex =
            ride.getRiderId() - 1;

        if (riderIndex < 0 ||
            riderIndex >= static_cast<int>(riders.size())) {
            continue;
        }

        const Rider& rider = riders[riderIndex];

        const Position riderPickup =
            rider.getPickup();

        int nearestDriver = -1;

        double minimumDistanceSquared =
            std::numeric_limits<double>::max();

        for (int d = 0; d < driverCount; ++d) {

            if (drivers[d].getStatus() != DriverStatus::IDLE) {
                continue;
            }

            const Position driverPosition =
                drivers[d].getPosition();

            const double dx =
                driverPosition.x - riderPickup.x;

            const double dy =
                driverPosition.y - riderPickup.y;

            const double distanceSquared =
                dx * dx + dy * dy;

            if (distanceSquared < minimumDistanceSquared) {

                minimumDistanceSquared =
                    distanceSquared;

                nearestDriver = d;
            }
        }

        candidateDriver[i] =
            nearestDriver;

        candidateDistanceSquared[i] =
            minimumDistanceSquared;
    }


    /*
     * PHASE 2:
     *
     * Resolve conflicts sequentially.
     *
     * Multiple rides may have selected the same driver
     * during Phase 1. Only the first valid ride gets
     * that driver.
     *
     * This phase remains serial to preserve the original
     * conflict-resolution behavior and avoid races.
     */
    int matchedRides = 0;

    for (int i = 0; i < rideCount; ++i) {

        Ride& ride = rides[i];

        if (ride.getStatus() != RideStatus::REQUESTED) {
            continue;
        }

        const int driverIndex =
            candidateDriver[i];

        if (driverIndex == -1) {
            continue;
        }

        /*
         * The driver may have already been assigned to an
         * earlier ride during conflict resolution.
         */
        if (drivers[driverIndex].getStatus()
            != DriverStatus::IDLE) {
            continue;
        }

        /*
         * O(1) rider lookup.
         */
        const int riderIndex =
            ride.getRiderId() - 1;

        if (riderIndex < 0 ||
            riderIndex >= static_cast<int>(riders.size())) {
            continue;
        }

        Rider* rider =
            &riders[riderIndex];

        Driver& driver =
            drivers[driverIndex];

        /*
         * Calculate the real distance only once for the
         * winning driver, because the existing output
         * prints the actual distance.
         */
        const double distance =
            std::sqrt(candidateDistanceSquared[i]);

        driver.setStatus(
            DriverStatus::BUSY
        );

        rider->setStatus(
            RiderStatus::MATCHED
        );

        ride.setDriverId(
            driver.getId()
        );

        ride.setStatus(
            RideStatus::MATCHED
        );

        ++matchedRides;

        std::cout
            << "  [OPENMP] Ride "
            << ride.getId()
            << " matched: Driver "
            << driver.getId()
            << " -> Rider "
            << rider->getId()
            << " (distance: "
            << distance
            << ")\n";
    }

    return matchedRides;
}

