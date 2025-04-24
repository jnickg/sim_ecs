#include <jnickg/sim_ecs/sim_ecs.hpp>

#include "simulator.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char *argv[])
{
    jnickg::simulator::simulator sim;

    sim.run(20);
}
