#pragma once
#include <vector>
#include <string>
#include <cstddef>
#include "scene/entity.h"

namespace Talos::Scene {

    // =========================================================================
    // Per-Component Runtime Consumption Metrics (Layer 3: Scene & Entity)
    // =========================================================================
    // Stores exact per-frame CPU execution time, RAM footprint, GPU VRAM usage,
    // active instance count, and operation count for a single component type.
    struct ComponentDiagnostics {
        size_t activeCount = 0;       // Entities with this component active in the scene
        size_t totalCount = 0;        // Total entities carrying this component struct
        double cpuTimeUs = 0.0;       // Smoothed CPU execution time in microseconds (us)
        double rawCpuTimeUs = 0.0;    // Exact single-frame CPU time in microseconds (us)
        size_t ramBytes = 0;          // Exact CPU RAM bytes (struct size + heap strings)
        size_t activeRamBytes = 0;    // CPU RAM bytes of enabled instances only
        size_t vramBytes = 0;         // Exact GPU VRAM bytes referenced by active instances
        size_t operationsCount = 0;   // Frame operations (e.g., AABB checks, draw calls)
    };

    // Aggregated diagnostic snapshot updated automatically during Scene::Update and Scene::Render.
    struct SceneDiagnostics {
        size_t totalEntities = 0;
        size_t activeEntities = 0;

        ComponentDiagnostics transform;
        ComponentDiagnostics movement;
        ComponentDiagnostics collider;
        ComponentDiagnostics sprite;
        ComponentDiagnostics text;

        double totalUpdateUs = 0.0;
        double totalRenderUs = 0.0;
        size_t totalSceneRamBytes = 0;
        size_t totalSceneVramBytes = 0;
    };

    // =========================================================================
    // Scene Container & Simulation Coordinator (Layer 3: Scene & Entity)
    // =========================================================================
    // Owns the contiguous array of Entities in the active game world and orchestrates
    // their per-frame movement, collision resolution, and rendering submission.
    class Scene {
    public:
        // Allocates a new Entity in the scene with a unique auto-incremented ID
        // and returns a reference so the caller can configure its components immediately.
        Entity& CreateEntity(const std::string& name, float x = 0.0f, float y = 0.0f);

        // Inserts a fully constructed Entity (e.g., deserialized from a .talos scene file)
        // and advances the internal ID counter so future entities never collide in ID.
        Entity& AddEntity(const Entity& entity);

        // Removes an entity matching the given ID from the scene container.
        void RemoveEntity(EntityID id);

        // Removes all entities from the scene and resets the ID counter to 1.
        // Used prior to loading a scene file from disk.
        void Clear();

        // Path-independent lookup by name. Unlike hierarchical NodePath systems,
        // finding an entity by name never breaks if the scene order is reorganized.
        // Returns nullptr if no entity with that name exists.
        Entity* FindEntity(const std::string& name);

        // Fast lookup by unique numeric identifier. Returns nullptr if not found.
        Entity* FindEntityById(EntityID id);

        // Direct access to the underlying contiguous entity storage (used by Editor/Systems).
        std::vector<Entity>& GetEntities();
        const std::vector<Entity>& GetEntities() const;

        // Advances entity movement and resolves solid collisions via the Physics2D layer.
        void Update(float delta);

        // Submits all active visual and text components to the Renderer2D layer.
        void Render() const;

        // Returns the latest per-component runtime consumption diagnostics.
        const SceneDiagnostics& GetDiagnostics() const { return m_diagnostics; }

    private:
        // Contiguous storage in memory for cache-friendly iteration every frame.
        std::vector<Entity> m_entities;

        // Monotonically increasing counter guaranteeing unique EntityIDs.
        EntityID m_nextId = 1;

        // Live per-component diagnostic metrics updated during Update() and Render().
        mutable SceneDiagnostics m_diagnostics;
    };

}
