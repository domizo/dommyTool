#include "Solver/DBulletStep.h"
#include <cassert>
#include <limits>
#include <cstdio>

int main() {
    using namespace dommy::bullet;
    SimulationStepPlan p = {7, 8, 9};
    assert(!prepareSimulationStep(1, 0, 60, p) && p.seconds == 7);
    assert(!prepareSimulationStep(1, 1025, 60, p));
    assert(!prepareSimulationStep(1, 1, 0, p));
    assert(!prepareSimulationStep(1, 1, 1000001, p));
    for (double dt : {0., -1., 1e300, std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()})
        assert(!prepareSimulationStep(dt, 1, 60, p));
    assert(prepareSimulationStep(1.0/24, 4, 240, p));
    assert(p.substeps == 4 && p.seconds > 0 && p.fixedSeconds > 0);
    assert(simulationStepBudget(0.f, p.seconds, p.fixedSeconds, p.substeps));
    assert(!simulationStepBudget(0.f, 3600.f, 1.f/1000000, 1));
    assert(!simulationStepBudget(0.f, 1.f/24, 1.f/1000000, 1));
    assert(simulationStepBudget(0.f, 5000.f, 1.f, 2));
    assert(!simulationStepBudget(0.f, 5000.f, 1.f, 3));
    assert(simulationStepBudget(.9f, 9999.f, 1.f, 1));
    assert(!simulationStepBudget(.9f, 10000.5f, 1.f, 1));
    assert(!simulationStepBudget(1.f, 0.f, 1e-6f, 1));
    assert(simulationStepBudget(-1e-9f, 1.f/60, 1.f/60, 1));
    assert(!simulationStepBudget(0.f, 1.f, 0.f, 1));
    assert(!simulationStepBudget(0.f, 1.f, 1.f, 0));
    assert(!simulationStepBudget(0.f, 1.f, std::numeric_limits<float>::denorm_min(), 1));
    assert(!simulationStepBudget(std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::max(), 1.f, 1));
    assert(simulationStepBudget(0., 5000., 1., 2));
    std::puts("Bullet step planning: numeric boundaries, residual time and whole-update budget passed");
}
