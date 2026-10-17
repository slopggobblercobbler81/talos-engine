#pragma once
#include <vector>
#include <string>
#include "scene/entity.h"

namespace Talos::Scene {

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

    private:
        // Contiguous storage in memory for cache-friendly iteration every frame.
        std::vector<Entity> m_entities;

        // Monotonically increasing counter guaranteeing unique EntityIDs.
        EntityID m_nextId = 1;
    };

}
