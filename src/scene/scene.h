#pragma once
#include <vector>
#include <string>
#include "scene/entity.h"

namespace Talos::Scene {

    // Manages all entities in the active world.
    // Accessed only through the Scene layer interface.
    class Scene {
    public:
        Entity& CreateEntity(const std::string& name, float x = 0.0f, float y = 0.0f);
        void RemoveEntity(EntityID id);

        // Path-independent lookup: reorganizing entities never breaks references
        Entity* FindEntity(const std::string& name);
        Entity* FindEntityById(EntityID id);

        std::vector<Entity>& GetEntities();
        const std::vector<Entity>& GetEntities() const;

        void Update(float delta);
        void Render() const;

    private:
        std::vector<Entity> m_entities;
        EntityID m_nextId = 1;
    };

}

