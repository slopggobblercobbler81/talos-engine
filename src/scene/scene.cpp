#include "scene/scene.h"
#include "core/input.h"
#include "physics/physics2d.h"
#include "renderer/renderer2d.h"
#include <algorithm>

namespace Talos::Scene {

    Entity& Scene::CreateEntity(const std::string& name, float x, float y) {
        Entity entity;
        entity.id = m_nextId++;
        entity.name = name;
        entity.transform.x = x;
        entity.transform.y = y;

        // Store the entity contiguously in the vector and return a reference to it.
        m_entities.push_back(entity);
        return m_entities.back();
    }

    Entity& Scene::AddEntity(const Entity& entity) {
        Entity copy = entity;

        // Ensure the entity has a valid non-zero ID and keep m_nextId strictly ahead
        // of all loaded entity IDs to prevent duplicate IDs on subsequent creations.
        if (copy.id == 0) {
            copy.id = m_nextId++;
        } else if (copy.id >= m_nextId) {
            m_nextId = copy.id + 1;
        }

        m_entities.push_back(copy);
        return m_entities.back();
    }

    void Scene::RemoveEntity(EntityID id) {
        // Erase-remove idiom: shifts remaining entities forward and shrinks the vector
        // without leaving holes in contiguous memory.
        m_entities.erase(
            std::remove_if(m_entities.begin(), m_entities.end(),
                [id](const Entity& e) { return e.id == id; }),
            m_entities.end()
        );
    }

    void Scene::Clear() {
        m_entities.clear();
        m_nextId = 1;
    }

    Entity* Scene::FindEntity(const std::string& name) {
        for (auto& entity : m_entities) {
            if (entity.name == name) {
                return &entity;
            }
        }
        return nullptr;
    }

    Entity* Scene::FindEntityById(EntityID id) {
        for (auto& entity : m_entities) {
            if (entity.id == id) {
                return &entity;
            }
        }
        return nullptr;
    }

    std::vector<Entity>& Scene::GetEntities() {
        return m_entities;
    }

    const std::vector<Entity>& Scene::GetEntities() const {
        return m_entities;
    }

    void Scene::Update(float delta) {
        using namespace Talos::Core;
        using namespace Talos::Physics;

        for (auto& entity : m_entities) {
            // Skip inactive entities or entities without movement enabled.
            if (!entity.active || !entity.movement.enabled) {
                continue;
            }

            float moveX = entity.movement.velocityX;
            float moveY = entity.movement.velocityY;

            // If built-in player control is enabled, sample directional input from the Core Input layer.
            if (entity.movement.playerControlled) {
                moveX = 0.0f;
                moveY = 0.0f;
                if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) moveX += entity.movement.speed;
                if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  moveX -= entity.movement.speed;
                if (Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down))  moveY += entity.movement.speed;
                if (Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up))    moveY -= entity.movement.speed;
            }

            // =================================================================
            // Separated-Axis Collision Resolution (X then Y)
            // =================================================================
            // Why resolve X and Y separately instead of diagonally at once?
            // If a player moves diagonally into a vertical wall, resolving X and Y
            // independently blocks only the X movement while allowing the Y movement
            // to continue smoothly—producing natural wall sliding without getting stuck.

            // 1. Horizontal (X) Axis Step
            float nextX = entity.transform.x + moveX * delta;
            bool blockedX = false;
            if (entity.collider.enabled && entity.collider.isSolid) {
                Rect2D nextBoxX = { nextX, entity.transform.y, entity.transform.width, entity.transform.height };
                for (const auto& other : m_entities) {
                    if (other.id == entity.id || !other.active || !other.collider.enabled || !other.collider.isSolid) {
                        continue;
                    }
                    Rect2D otherBox = { other.transform.x, other.transform.y, other.transform.width, other.transform.height };
                    if (Physics2D::CheckOverlap(nextBoxX, otherBox)) {
                        blockedX = true;
                        break;
                    }
                }
            }
            if (!blockedX) {
                entity.transform.x = nextX;
            }

            // 2. Vertical (Y) Axis Step
            float nextY = entity.transform.y + moveY * delta;
            bool blockedY = false;
            if (entity.collider.enabled && entity.collider.isSolid) {
                Rect2D nextBoxY = { entity.transform.x, nextY, entity.transform.width, entity.transform.height };
                for (const auto& other : m_entities) {
                    if (other.id == entity.id || !other.active || !other.collider.enabled || !other.collider.isSolid) {
                        continue;
                    }
                    Rect2D otherBox = { other.transform.x, other.transform.y, other.transform.width, other.transform.height };
                    if (Physics2D::CheckOverlap(nextBoxY, otherBox)) {
                        blockedY = true;
                        break;
                    }
                }
            }
            if (!blockedY) {
                entity.transform.y = nextY;
            }
        }
    }

    void Scene::Render() const {
        using namespace Talos::Renderer;

        // Iterate through all active entities and submit their enabled visual
        // components to the Renderer2D subsystem in deterministic order.
        for (const auto& entity : m_entities) {
            if (!entity.active) {
                continue;
            }

            // Draw the visual body if SpriteComponent is enabled
            if (entity.sprite.enabled) {
                Renderer2D::DrawRect(
                    entity.transform.x,
                    entity.transform.y,
                    entity.transform.width,
                    entity.transform.height,
                    entity.sprite.color
                );
            }

            // Draw the text overlay/label if TextComponent is enabled
            if (entity.text.enabled && !entity.text.content.empty()) {
                Renderer2D::DrawLabel(
                    entity.text.content.c_str(),
                    entity.transform.x + entity.text.offsetX,
                    entity.transform.y + entity.text.offsetY,
                    entity.text.fontSize,
                    entity.text.color
                );
            }
        }
    }

}
