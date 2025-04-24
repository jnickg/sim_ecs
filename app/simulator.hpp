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

struct WandererComponent : public OwnedComponentBase
{
    double x         = 0.0;
    double y         = 0.0;
    double speed     = 0.0;
    double direction = 0.0;

    WandererComponent(entity_t world) : OwnedComponentBase{world} {}
    WandererComponent()          = delete;
    virtual ~WandererComponent() = default;

    virtual std::ostream &print(std::ostream &os) const override
    {
        std::stringstream ss;
        std::stringstream ss_base;
        ComponentBase::print(ss_base);

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

    // std::unordered_map<entity_t, handle<WorldTimeComponent>> world_time;
    // std::unordered_map<entity_t, handle<TimedEntityComponent>> timed_entities;
    // std::unordered_map<entity_t, handle<WorldSpace2DComponent>> world_space_2d;
    // std::unordered_map<entity_t, handle<WandererComponent>> wanderers;

    using world_time_system_t                     = GenericSystem<WorldTimeComponent>;
    handle<world_time_system_t> world_time_system = std::make_shared<world_time_system_t>(
        [this](entity_t entity) -> bool {
            auto should_run = this->component_manager->has<WorldTimeComponent>(entity);
            return should_run;
        },
        [this](entity_t entity) -> world_time_system_t::component_tuple_t {
            return {this->component_manager->get<WorldTimeComponent>(entity)};
        },
        [](entity_t entity, world_time_system_t::component_tuple_t components) -> component_set_t {
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

    using timed_entity_system_t                       = GenericSystem<TimedEntityComponent, WorldTimeComponent>;
    handle<timed_entity_system_t> timed_entity_system = std::make_shared<timed_entity_system_t>(
        [this](entity_t entity) -> bool {
            return false;
            // return this->timed_entities.find(entity) != this->timed_entities.end() &&
            //        this->world_time.find(entity) != this->world_time.end();
        },
        [this](entity_t entity) -> timed_entity_system_t::component_tuple_t {
            return {this->component_manager->get<TimedEntityComponent>(entity),
                this->component_manager->get<WorldTimeComponent>(entity)};
        },
        [](entity_t entity, timed_entity_system_t::component_tuple_t components) -> component_set_t {
            return component_set_t{};
            // component_set_t updated_components;
            // auto timed_c     = std::get<handle<TimedEntityComponent>>(components);
            // auto world_time_c = std::get<handle<WorldTimeComponent>>(components);
            // if (!timed_c || !world_time_c)
            // {
            //     // No time for this entity, so nothing to do
            //     return updated_components;
            // }

            // if (!timed_c->running || !world_time_c->running)
            // {
            //     // Time is not running, so it can't update
            //     return updated_components;
            // }

            // // TODO
            // return updated_components;
        });

    using wandering_system_t
        = GenericSystem<WandererComponent, TimedEntityComponent, WorldTimeComponent, WorldSpace2DComponent>;
    handle<wandering_system_t> wandering_system = std::make_shared<wandering_system_t>(
        [this](entity_t entity) -> bool {
            auto can_run = this->component_manager->has<WandererComponent>(entity)
                && this->component_manager->has<TimedEntityComponent>(entity);
            return can_run;
        },
        [this](entity_t entity) -> wandering_system_t::component_tuple_t {
            auto wanderer_c = this->component_manager->get<WandererComponent>(entity);
            auto timed_c    = this->component_manager->get<TimedEntityComponent>(entity);
            if (!wanderer_c || !timed_c)
            {
                // No wanderer component, so nothing to do
                return {};
            }
            auto world_time_c = this->component_manager->get<WorldTimeComponent>(timed_c->owner.value_or(NO_ENTITY));
            auto world_space_c
                = this->component_manager->get<WorldSpace2DComponent>(wanderer_c->owner.value_or(NO_ENTITY));
            if (!world_time_c || !world_space_c)
            {
                // No world time or world space component, so nothing to do
                return {};
            }
            return {wanderer_c, timed_c, world_time_c, world_space_c};
        },
        [](entity_t entity, wandering_system_t::component_tuple_t component) -> component_set_t {
            component_set_t updated_components;
            auto wanderer_c    = std::get<handle<WandererComponent>>(component);
            auto timed_c       = std::get<handle<TimedEntityComponent>>(component);
            auto world_time_c  = std::get<handle<WorldTimeComponent>>(component);
            auto world_space_c = std::get<handle<WorldSpace2DComponent>>(component);
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
        });

    using diagnostic_system_t                     = GenericSystem<WandererComponent>;
    handle<diagnostic_system_t> diagnostic_system = std::make_shared<diagnostic_system_t>(
        [this](entity_t entity) -> bool {
            auto can_run = this->component_manager->has<WandererComponent>(entity);
            return can_run;
        },
        [this](entity_t entity) -> diagnostic_system_t::component_tuple_t {
            return {this->component_manager->get<WandererComponent>(entity)};
        },
        [](entity_t entity, diagnostic_system_t::component_tuple_t component) -> component_set_t {
            component_set_t updated_components;
            auto wanderer_c = std::get<handle<WandererComponent>>(component);
            if (!wanderer_c)
            {
                // No wanderer component, so nothing to do
                return updated_components;
            }

            std::stringstream ss;
            ss << "Wanderer: " << *wanderer_c << std::endl;
            std::cout << ss.str();

            return updated_components;
        });

    simulator()
    {
        // Add the systems to the system manager
        auto world_time_s   = this->system_manager->register_new(this->world_time_system);
        auto timed_entity_s = this->system_manager->register_new(this->timed_entity_system);
        auto wandering_s    = this->system_manager->register_new(this->wandering_system);
        auto diagnostic_s   = this->system_manager->register_new(this->diagnostic_system);

        // Add dependencies
        this->system_manager->add_dependency(world_time_s, diagnostic_s);
        this->system_manager->add_dependency(timed_entity_s, world_time_s);
        this->system_manager->add_dependency(wandering_s, timed_entity_s);
        this->system_manager->add_dependency(wandering_s, world_time_s);

        // Enable the systems
        this->world_time_system->enable();
        this->timed_entity_system->enable();
        this->wandering_system->enable();
        this->diagnostic_system->enable();

        //
        // Create the world
        //
        auto world_e = new_entity();

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
        auto wanderer_e = new_entity();

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
