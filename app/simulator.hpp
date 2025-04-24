#pragma once
#include <jnickg/sim_ecs/sim_ecs.hpp>

#include <stdlib.h>

#include <cmath>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace jnickg::simulator
{
using namespace jnickg::sim_ecs;

struct WorldSpace2DComponent : public ComponentBase
{
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;

    virtual std::ostream &print(std::ostream &os) const override
    {
        std::stringstream ss;
        std::stringstream ss_base;
        ComponentBase::print(ss_base);

        ss << "WorldSpace2DComponent(base=" << ss_base.str() << ", " << "min_x=" << min_x << ", max_x=" << max_x
           << ", min_y=" << min_y << ", max_y=" << max_y << ")";

        os << ss.str();

        return os;
    }
};

struct WandererComponent : public OwnedComponent
{
    double x         = 0.0;
    double y         = 0.0;
    double speed     = 0.0;
    double direction = 0.0;

    WandererComponent(entity_t world) : OwnedComponent{world} {}
    WandererComponent()          = delete;
    virtual ~WandererComponent() = default;

    virtual std::ostream &print(std::ostream &os) const override
    {
        std::stringstream ss;
        std::stringstream ss_base;
        OwnedComponent::print(ss_base);

        ss << "WandererComponent(base=" << ss_base.str() << ", " << "x=" << x << ", y=" << y << ", speed=" << speed
           << ", direction=" << direction << ")";

        os << ss.str();

        return os;
    }
};

struct simulator
{
    using ticks_t = std::size_t;

    handle<ComponentManager> component_manager = std::make_shared<ComponentManager>();
    handle<SystemManager> system_manager       = std::make_shared<SystemManager>();

//     using diagnostic_system_t                     = GenericSystem<WandererComponent>;
//     handle<diagnostic_system_t> diagnostic_system = std::make_shared<diagnostic_system_t>(
// );

    simulator()
    {
        // Add the systems to the system manager
        // auto world_time_s   = this->system_manager->register_new(this->world_time_system);
        auto world_time_s = this->system_manager->new_system<WorldTimeComponent>(
            "World Time System", system_state::enabled,
            [this](entity_t entity) {
                auto should_run = this->component_manager->has<WorldTimeComponent>(entity);
                return should_run;
            },
            [this](entity_t entity) {
                return this->component_manager->get_view<WorldTimeComponent>(entity);
            },
            [](entity_t entity, auto components) {
                component_set_t updated_components;
                auto world_time_c = std::get<0>(components);
                if (!world_time_c || !world_time_c->running)
                {
                    // No time for this entity, so nothing to do
                    return updated_components;
                }
    
                // Increment the time
                world_time_c->delta_time = world_time_c->step * world_time_c->time_scale;
                world_time_c->total_time += world_time_c->delta_time;
                std::cout << "World time updated: " << world_time_c->total_time << std::endl;
    
                return updated_components;
            });


        auto movement_s    = this->system_manager->new_system<WandererComponent, TimedEntityComponent>(
            "Movement System", system_state::enabled,
            [this](entity_t entity) -> bool {
                if (!this->component_manager->has_view<WandererComponent, TimedEntityComponent>(entity))
                {
                    // No wanderer component, so nothing to do
                    return false;
                }
                auto [wanderer_c, timed_c] = this->component_manager->get_view<WandererComponent, TimedEntityComponent>(entity);
                auto world_e = wanderer_c->owner;
                return this->component_manager->has_view<WandererComponent, TimedEntityComponent>(entity)
                    && this->component_manager->has_view<WorldTimeComponent, WorldSpace2DComponent>(world_e);
            },
            [this](entity_t entity) -> auto {
                return this->component_manager->get_view<WandererComponent, TimedEntityComponent>(entity);
            },
            [this](entity_t entity, auto components) {
                component_set_t updated_components;
                auto& [wanderer_c, timed_c] = components;
                if (!wanderer_c || !timed_c)
                {
                    // No wanderer component, so nothing to do
                    return updated_components;
                }
                auto world_e       = wanderer_c->owner;
                auto [world_time_c, world_space_c] = this->component_manager->get_view<WorldTimeComponent, WorldSpace2DComponent>(world_e);
                if (!wanderer_c || !timed_c || !world_time_c || !world_space_c)
                {
                    // No wanderer component, so nothing to do
                    return updated_components;
                }
    
                if (!timed_c->running || !world_time_c->running)
                {
                    // Time is not running, so it can't wander
                    return updated_components;
                }
    
                // How much time has passed
                auto time_passed = world_time_c->delta_time;
                if (time_passed == 0.0)
                {
                    // No time has passed, so nothing to do
                    return updated_components;
                }
    
                // How much time has passed in the entity's time
                auto entity_time_passed = time_passed * timed_c->time_scale;
                if (entity_time_passed == 0.0)
                {
                    // No time has passed, so nothing to do
                    return updated_components;
                }
    
                // Actually move the wanderer
                wanderer_c->x += wanderer_c->speed * entity_time_passed * std::cos(wanderer_c->direction);
                wanderer_c->y += wanderer_c->speed * entity_time_passed * std::sin(wanderer_c->direction);
                // Check if the wanderer is out of bounds
                if (wanderer_c->x < world_space_c->min_x || wanderer_c->x > world_space_c->max_x
                    || wanderer_c->y < world_space_c->min_y || wanderer_c->y > world_space_c->max_y)
                {
                    // Wanderer is out of bounds, so wrap around world bounds
                    if (wanderer_c->x < world_space_c->min_x)
                    {
                        wanderer_c->x = world_space_c->max_x;
                    }
                    else if (wanderer_c->x > world_space_c->max_x)
                    {
                        wanderer_c->x = world_space_c->min_x;
                    }
                    if (wanderer_c->y < world_space_c->min_y)
                    {
                        wanderer_c->y = world_space_c->max_y;
                    }
                    else if (wanderer_c->y > world_space_c->max_y)
                    {
                        wanderer_c->y = world_space_c->min_y;
                    }
                    // Update the wanderer component
                    updated_components.emplace(wanderer_c);
                }
    
                return updated_components;
            }
        );
        auto diagnostic_s   = this->system_manager->new_system<WandererComponent>(
            "Diagnostic System", system_state::enabled,
            [this](entity_t entity) -> bool {
                auto can_run = this->component_manager->has_view<WandererComponent>(entity);
                return can_run;
            },
            [this](entity_t entity) -> auto {
                return this->component_manager->get_view<WandererComponent>(entity);
            },
            [](entity_t entity, auto components) -> component_set_t {
                component_set_t updated_components;
                auto& [wanderer_c] = components;
                if (!wanderer_c)
                {
                    // No wanderer component, so nothing to do
                    return updated_components;
                }
    
                std::stringstream ss;
                ss << *wanderer_c << std::endl;
                std::cout << ss.str();
    
                return updated_components;
            });

        // Add dependencies
        this->system_manager->add_dependency(world_time_s, diagnostic_s);
        this->system_manager->add_dependency(movement_s, world_time_s);

        //
        // Create the world
        //
        auto world_e = create_entity();

        auto world_time_c = std::make_shared<WorldTimeComponent>();
        this->component_manager->add(world_e, world_time_c);
        world_time_c->running    = true;
        world_time_c->step       = 1.0;
        world_time_c->time_scale = 1.0;
        world_time_c->total_time = 0.0;
        world_time_c->delta_time = 0.0;

        auto world_space_2d_c = std::make_shared<WorldSpace2DComponent>();
        this->component_manager->add(world_e, world_space_2d_c);
        world_space_2d_c->min_x = -10.0;
        world_space_2d_c->max_x = 10.0;
        world_space_2d_c->min_y = -10.0;
        world_space_2d_c->max_y = 10.0;

        //
        // Create the wanderer
        //
        auto wanderer_e = create_entity();

        auto timed_entity_c = std::make_shared<TimedEntityComponent>(world_e);
        this->component_manager->add(wanderer_e, timed_entity_c);
        timed_entity_c->running    = true;
        timed_entity_c->time_scale = 1.0;

        auto wanderer_c = std::make_shared<WandererComponent>(world_e);
        this->component_manager->add(wanderer_e, wanderer_c);
        wanderer_c->x         = 0.0;
        wanderer_c->y         = 0.0;
        wanderer_c->speed     = 1.0;
        wanderer_c->direction = 0.0;
    }
    simulator(const simulator &)            = delete;
    simulator(simulator &&)                 = delete;
    simulator &operator=(const simulator &) = delete;
    simulator &operator=(simulator &&)      = delete;
    virtual ~simulator()                    = default;

    void run(ticks_t t)
    {
        for (ticks_t i = 0; i < t; ++i)
        {
            auto all_entities = this->component_manager->get_all_entities();
            this->system_manager->update(all_entities);
        }
    }
};
} // namespace jnickg::simulator
