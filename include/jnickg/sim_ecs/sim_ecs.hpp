#pragma once

#include <stdlib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace jnickg::sim_ecs
{
//
// Entities
//

template <typename T> using handle = std::shared_ptr<T>;
using entity_t                     = std::size_t;
using system_t                     = std::size_t;
using frameidx_t                   = std::size_t;
using clock_t                      = std::chrono::high_resolution_clock;
using time_t                       = clock_t::time_point;
using duration_t                   = clock_t::duration;
template <typename T> using maybe  = std::optional<T>;
constexpr inline auto nothing      = std::nullopt;

constexpr inline entity_t NO_ENTITY = 0; //< The null entity, used to indicate that an entity does not exist
inline static entity_t new_entity()
{
    static std::atomic<entity_t> id = 1;
    return id++;
}

constexpr inline system_t NO_SYSTEM = 0; //< The null system, used to indicate that a system does not exist
inline static system_t new_system()
{
    static std::atomic<system_t> id = 1;
    return id++;
}

//
// Components
//

struct ComponentBase
{
    maybe<entity_t> owner = nothing;         // The entity that owns this component
    time_t created_at     = clock_t::now();  //< The real-world time when the
                                             // component was created, for debugging
    time_t last_updated_at = clock_t::now(); //< The real-world time when the component was last
                                             // updated, for debugging
    system_t last_updated_by = NO_SYSTEM;    //< The system that last updated this component, for debugging

    ComponentBase()                                 = default;
    ComponentBase(const ComponentBase &)            = delete;
    ComponentBase(ComponentBase &&)                 = delete;
    ComponentBase &operator=(const ComponentBase &) = delete;
    ComponentBase &operator=(ComponentBase &&)      = delete;
    virtual ~ComponentBase()                        = default;

    inline void mark_update_at(time_t time) { last_updated_at = time; }

    inline void mark_updated() { mark_update_at(clock_t::now()); }

    virtual std::ostream &print(std::ostream &os) const
    {
        std::stringstream ss;

        ss << "ComponentBase(owner=" << owner.value_or(NO_ENTITY)
           << ", created_at=" << created_at.time_since_epoch().count()
           << ", last_updated_at=" << last_updated_at.time_since_epoch().count()
           << ", last_updated_by=" << last_updated_by << ")";

        os << ss.str();

        return os;
    }
};

inline std::ostream &operator<<(std::ostream &os, const ComponentBase &component) { return component.print(os); }

inline std::ostream &operator<<(std::ostream &os, const handle<ComponentBase> &component)
{
    if (component)
    {
        return component->print(os);
    }
    else
    {
        os << "nullptr";
        return os;
    }
}

inline std::ostream &operator<<(std::ostream &os, const maybe<handle<ComponentBase>> &component)
{
    if (!component.has_value())
    {
        os << "nullopt";
    }
    else if (component.has_value() && !component.value())
    {
        os << "nullptr";
    }
    else
    {
        component.value()->print(os);
    }
    return os;
}

struct OwnedComponentBase : public ComponentBase
{
    OwnedComponentBase(entity_t owner) { this->owner = owner; }
    OwnedComponentBase()          = delete;
    virtual ~OwnedComponentBase() = default;
};

/**
 * @brief A component that stores a time value for the world
 * @details This component is used to store a time value for the world. It is
 * used to synchronize the world time with the entity time. This is useful for
 * entities that need
 */
struct WorldTimeComponent : public ComponentBase
{
    bool running      = false; //< If the world is advancing through time
    double total_time = 0.0;   //< The total world time that has passed
    double delta_time = 0.0;   //< The world time that has passed since the last update
    double step       = 1.0;   //< The world time step that is used to update the world
    double time_scale = 1.0;   //< The time scale of the world, used to speed up or
                               // slow down the world time

    virtual ~WorldTimeComponent() = default;

    virtual std::ostream &print(std::ostream &os) const override
    {
        std::stringstream ss;
        std::stringstream ss_base;
        ComponentBase::print(ss_base);

        ss << "WorldTimeComponent(base=" << ss_base.str() << ", " << "running=" << running
           << ", total_time=" << total_time << ", delta_time=" << delta_time << ", step=" << step
           << ", time_scale=" << time_scale << ")";

        os << ss.str();

        return os;
    }
};

/**
 * @brief A component that marks an entity as time-based
 * @details This component is used to mark an entity as time-based, meaning that
 * it can be updated based on the world time. This is useful for entities that
 * need to be updated
 */
struct TimedEntityComponent : public OwnedComponentBase
{
    bool running      = false; //< If the entity is running
    double time_scale = 1.0;   //< The time scale of the entity

    TimedEntityComponent(entity_t world) : OwnedComponentBase{world} {}
    TimedEntityComponent()          = delete;
    virtual ~TimedEntityComponent() = default;

    virtual std::ostream &print(std::ostream &os) const override
    {
        std::stringstream ss;
        std::stringstream ss_base;
        ComponentBase::print(ss_base);

        ss << "TimedEntityComponent(base=" << ss_base.str() << ", " << "running=" << running
           << ", time_scale=" << time_scale << ")";

        os << ss.str();

        return os;
    }
};

//
// Systems
//
using component_set_t = std::unordered_set<handle<ComponentBase>>;

enum class system_state
{
    enabled,
    disabled,
};

struct SystemBase
{
    std::string name   = "";
    system_state state = system_state::enabled;

    SystemBase()                              = default;
    SystemBase(const SystemBase &)            = delete;
    SystemBase(SystemBase &&)                 = delete;
    SystemBase &operator=(const SystemBase &) = delete;
    SystemBase &operator=(SystemBase &&)      = delete;

    virtual ~SystemBase() = default;

  protected:
    /**
     * @brief Update the system for the given entities, and return what was
     * updated
     *
     * @return A set of components that were actually updated by this call
     */
    virtual component_set_t update_impl(const std::vector<entity_t> &entities) = 0;

  public:
    inline bool is_enabled() const { return state == system_state::enabled; }
    inline void enable() { state = system_state::enabled; }
    inline void disable() { state = system_state::disabled; }

    virtual void update(const std::vector<entity_t> &entities) final
    {
        if (state != system_state::enabled)
        {
            std::cerr << "System " << name << " is not enabled" << std::endl;
            return;
        }
        if (entities.empty())
        {
            return;
        }
        auto updated     = update_impl(entities);
        auto update_time = clock_t::now();
        for (const auto &component : updated)
        {
            if (!component)
            {
                continue;
            }
            component->mark_update_at(update_time);
        }
    }

    virtual void update_if(const std::vector<entity_t> &entities, std::function<bool(entity_t)> predicate) final
    {
        if (state != system_state::enabled)
        {
            return;
        }
        if (entities.empty())
        {
            return;
        }
        if (!predicate)
        {
            return;
        }
        std::vector<entity_t> filtered_entities;
        std::copy_if(entities.begin(), entities.end(), std::back_inserter(filtered_entities), predicate);
        update(filtered_entities);
    }
};

/**
 * @brief A system that takes a user-provided component retriever and a
 * user-provided update function
 * @details This system is used to update entities that have a specific
 * component. The component retriever is used to get the component for the given
 * entities to be updated, and the update function is used to update the
 * component. This is useful for systems that need to update entities that have
 * a specific component.
 */
template <typename... Cs> struct GenericSystem : public SystemBase
{
    using component_tuple_t = std::tuple<handle<Cs>...>;

    using initialize_component_t = std::function<void(entity_t)>;
    using can_update_t           = std::function<bool(entity_t)>;
    using component_retriever_t  = std::function<component_tuple_t(entity_t)>;
    using update_function_t      = std::function<component_set_t(entity_t, component_tuple_t)>;

    can_update_t can_update_f;
    component_retriever_t get_components_f;
    update_function_t update_components_f;

    GenericSystem(can_update_t can_update, component_retriever_t get_components, update_function_t update_components)
        : can_update_f{can_update}, get_components_f{get_components}, update_components_f{update_components}
    {
    }

  private:
    template <typename... Ts> inline bool all_non_null(std::tuple<handle<Ts>...> const &t)
    {
        return (... && (std::get<handle<Ts>>(t) != nullptr));
    }

    template <typename... Ts> inline bool any_null(std::tuple<handle<Ts>...> const &t)
    {
        return (... || (std::get<handle<Ts>>(t) == nullptr));
    }

  protected:
    component_set_t update_impl(const std::vector<entity_t> &entities) override
    {
        // Build a list of entities that have the component
        std::unordered_map<entity_t, component_tuple_t> components;
        for (const auto &entity : entities)
        {
            if (!can_update_f(entity))
            {
                continue;
            }
            auto component = get_components_f(entity);
            if (any_null(component))
            {
                continue;
            }
            // Iff all components are present, add them to the map
            components[entity] = component;
        }
        // Update the components
        component_set_t updated_components;
        for (const auto &[entity, component] : components)
        {
            auto updated = update_components_f(entity, component);
            updated_components.insert(updated.begin(), updated.end());
        }
        return updated_components;
    }
};

//
// Managers
//

class ComponentManager
{
    inline static std::atomic<std::size_t> manager_id_counter;

    std::size_t id = manager_id_counter++;
    std::unordered_set<entity_t> entities;

  public:
    template <typename T> void add(entity_t entity, std::shared_ptr<T> component)
    {
        auto &map   = get_map<T>();
        map[entity] = component;
        entities.insert(entity);
    }

    template <typename T> std::shared_ptr<T> get(entity_t entity)
    {
        auto &map = get_map<T>();
        auto it   = map.find(entity);
        if (it != map.end())
        {
            return it->second;
        }
        return nullptr;
    }

    std::vector<entity_t> get_all_entities() const { return std::vector<entity_t>(entities.begin(), entities.end()); }

    template <typename T> void remove(entity_t entity)
    {
        auto &map = get_map<T>();
        map.erase(entity);
    }

    bool entity_exists(entity_t entity) const { return entities.find(entity) != entities.end(); }

    template <typename T> bool has(entity_t entity) const
    {
        auto &map = get_map<T>();
        return map.find(entity) != map.end();
    }

    template <typename T> std::unordered_map<entity_t, std::shared_ptr<T>> &get_all() { return get_map<T>(); }

  private:
    template <typename T>
    static std::unordered_map<entity_t, std::shared_ptr<T>> &get_static_map(std::size_t manager_id)
    {
        static std::unordered_map<std::size_t, std::unordered_map<entity_t, std::shared_ptr<T>>> map;
        return map[manager_id];
    }
    template <typename T> std::unordered_map<entity_t, std::shared_ptr<T>> &get_map() { return get_static_map<T>(id); }

    template <typename T> const std::unordered_map<entity_t, std::shared_ptr<T>> &get_map() const
    {
        return get_static_map<T>(id);
    }
};

struct SystemManager
{
    struct SystemDependencyNode
    {
        system_t id;
        std::vector<system_t> dependencies;
    };
    struct ExecutionStage
    {
        std::vector<system_t> systems;
    };
    using ExecutionGraph = std::vector<ExecutionStage>;
    std::unordered_map<system_t, SystemDependencyNode> system_nodes;
    std::unordered_map<system_t, handle<SystemBase>> systems;

    SystemManager() = default;

    system_t register_new(handle<SystemBase> system)
    {
        auto system_id     = new_system();
        systems[system_id] = system;
        return system_id;
    }

    void add_dependency(system_t system, system_t dependency)
    {
        if (system_nodes.find(system) == system_nodes.end())
        {
            system_nodes[system] = SystemDependencyNode{system, {}};
        }
        system_nodes[system].dependencies.push_back(dependency);
    }

    ExecutionGraph build_execution_graph()
    {
        ExecutionGraph graph;

        // Count incoming edges for each node
        std::unordered_map<system_t, int> in_degree;
        for (const auto &[id, node] : system_nodes)
        {
            if (in_degree.find(id) == in_degree.end())
            {
                in_degree[id] = 0;
            }
            for (auto dep : node.dependencies)
            {
                in_degree[dep]; // ensure it's in the map
                in_degree[id]++;
            }
        }

        // Systems with no dependencies
        std::queue<system_t> ready;
        for (const auto &[id, degree] : in_degree)
        {
            if (degree == 0)
            {
                ready.push(id);
            }
        }

        std::unordered_map<system_t, std::vector<system_t>> reverse_graph;
        for (const auto &[id, node] : system_nodes)
        {
            for (auto dep : node.dependencies)
            {
                reverse_graph[dep].push_back(id);
            }
        }

        // Build stages
        while (!ready.empty())
        {
            ExecutionStage stage;
            std::queue<system_t> next_ready;

            // process current stage
            size_t count = ready.size();
            for (size_t i = 0; i < count; ++i)
            {
                system_t id = ready.front();
                ready.pop();
                stage.systems.push_back(id);

                // reduce the dependency count of dependents
                for (auto dependent : reverse_graph[id])
                {
                    if (--in_degree[dependent] == 0)
                    {
                        next_ready.push(dependent);
                    }
                }
            }

            graph.push_back(std::move(stage));
            std::swap(ready, next_ready);
        }

        return graph;
    }

    void update(const std::vector<entity_t> &entities)
    {
        auto stages          = build_execution_graph();
        size_t current_stage = 0;
        for (const auto &stage : stages)
        {
            for (const auto &system_id : stage.systems)
            {
                if (systems.find(system_id) == systems.end())
                {
                    continue;
                }

                auto system = systems[system_id];
                if (!system || !system->is_enabled())
                {
                    continue;
                }

                systems[system_id]->update(entities);
            }
        }
    }
};
} // namespace jnickg::sim_ecs
