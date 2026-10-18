#include "scene/scene.h"
#include "core/input.h"
#include "physics/physics2d.h"
#include "renderer/renderer2d.h"
#include <algorithm>
#include <chrono>
#include <unordered_set>

namespace Talos::Scene {

    static double SmoothMicros(double prevSmoothed, double rawSample) {
        if (prevSmoothed <= 0.0001) {
            return rawSample;
        }
        return prevSmoothed * 0.85 + rawSample * 0.15;
    }

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
        using Clock = std::chrono::steady_clock;

        double transformUsAccum = 0.0;
        double movementUsAccum  = 0.0;
        double colliderUsAccum  = 0.0;

        size_t activeTransformCount = 0;
        size_t activeMovementCount  = 0;
        size_t activeColliderCount  = 0;
        size_t movementOpsCount     = 0;
        size_t aabbChecksCount      = 0;

        // 1. Evaluate active Transform & Collider component counts
        auto tTransformStart = Clock::now();
        for (const auto& entity : m_entities) {
            if (!entity.active) continue;
            ++activeTransformCount;
            if (entity.collider.enabled) {
                ++activeColliderCount;
            }
        }
        auto tTransformEnd = Clock::now();
        transformUsAccum += std::chrono::duration<double, std::micro>(tTransformEnd - tTransformStart).count();

        // 2. Process MovementComponent & ColliderComponent per active entity
        for (auto& entity : m_entities) {
            if (!entity.active || !entity.movement.enabled) {
                continue;
            }

            // --- MovementComponent execution timing ---
            auto tMoveStart = Clock::now();
            ++activeMovementCount;
            ++movementOpsCount;

            float moveX = entity.movement.velocityX;
            float moveY = entity.movement.velocityY;

            // If built-in player control is enabled, sample directional input from Core::Input / InputMap.
            if (entity.movement.playerControlled) {
                moveX = 0.0f;
                moveY = 0.0f;
                if (Input::IsActionDown("move_right") || Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) moveX += entity.movement.speed;
                if (Input::IsActionDown("move_left")  || Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  moveX -= entity.movement.speed;
                if (Input::IsActionDown("move_down")  || Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down))  moveY += entity.movement.speed;
                if (Input::IsActionDown("move_up")    || Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up))    moveY -= entity.movement.speed;
            }

            float nextX = entity.transform.x + moveX * delta;
            auto tMoveEnd = Clock::now();
            movementUsAccum += std::chrono::duration<double, std::micro>(tMoveEnd - tMoveStart).count();

            // =================================================================
            // Separated-Axis Collision Resolution (X then Y)
            // =================================================================
            // 1. Horizontal (X) Axis Step
            auto tColXStart = Clock::now();
            bool blockedX = false;
            if (entity.collider.enabled && entity.collider.isSolid) {
                Rect2D nextBoxX = { nextX, entity.transform.y, entity.transform.width, entity.transform.height };
                for (const auto& other : m_entities) {
                    if (other.id == entity.id || !other.active || !other.collider.enabled || !other.collider.isSolid) {
                        continue;
                    }
                    Rect2D otherBox = { other.transform.x, other.transform.y, other.transform.width, other.transform.height };
                    ++aabbChecksCount;
                    if (Physics2D::CheckOverlap(nextBoxX, otherBox)) {
                        blockedX = true;
                        break;
                    }
                }
            }
            auto tColXEnd = Clock::now();
            colliderUsAccum += std::chrono::duration<double, std::micro>(tColXEnd - tColXStart).count();

            // --- TransformComponent X Commit + Next Y Calculation ---
            auto tTransXStart = Clock::now();
            if (!blockedX) {
                entity.transform.x = nextX;
            }
            float nextY = entity.transform.y + moveY * delta;
            auto tTransXEnd = Clock::now();
            transformUsAccum += std::chrono::duration<double, std::micro>(tTransXEnd - tTransXStart).count();

            // 2. Vertical (Y) Axis Step
            auto tColYStart = Clock::now();
            bool blockedY = false;
            if (entity.collider.enabled && entity.collider.isSolid) {
                Rect2D nextBoxY = { entity.transform.x, nextY, entity.transform.width, entity.transform.height };
                for (const auto& other : m_entities) {
                    if (other.id == entity.id || !other.active || !other.collider.enabled || !other.collider.isSolid) {
                        continue;
                    }
                    Rect2D otherBox = { other.transform.x, other.transform.y, other.transform.width, other.transform.height };
                    ++aabbChecksCount;
                    if (Physics2D::CheckOverlap(nextBoxY, otherBox)) {
                        blockedY = true;
                        break;
                    }
                }
            }
            auto tColYEnd = Clock::now();
            colliderUsAccum += std::chrono::duration<double, std::micro>(tColYEnd - tColYStart).count();

            // --- TransformComponent Y Commit ---
            auto tTransYStart = Clock::now();
            if (!blockedY) {
                entity.transform.y = nextY;
            }
            auto tTransYEnd = Clock::now();
            transformUsAccum += std::chrono::duration<double, std::micro>(tTransYEnd - tTransYStart).count();
        }

        // Commit simulation component metrics to m_diagnostics
        const size_t totalCount = m_entities.size();
        m_diagnostics.totalEntities  = totalCount;
        m_diagnostics.activeEntities = activeTransformCount;

        m_diagnostics.transform.activeCount     = activeTransformCount;
        m_diagnostics.transform.totalCount      = totalCount;
        m_diagnostics.transform.rawCpuTimeUs    = transformUsAccum;
        m_diagnostics.transform.cpuTimeUs       = SmoothMicros(m_diagnostics.transform.cpuTimeUs, transformUsAccum);
        m_diagnostics.transform.operationsCount = activeTransformCount;

        m_diagnostics.movement.activeCount      = activeMovementCount;
        m_diagnostics.movement.totalCount       = totalCount;
        m_diagnostics.movement.rawCpuTimeUs     = movementUsAccum;
        m_diagnostics.movement.cpuTimeUs        = SmoothMicros(m_diagnostics.movement.cpuTimeUs, movementUsAccum);
        m_diagnostics.movement.operationsCount  = movementOpsCount;

        m_diagnostics.collider.activeCount      = activeColliderCount;
        m_diagnostics.collider.totalCount       = totalCount;
        m_diagnostics.collider.rawCpuTimeUs     = colliderUsAccum;
        m_diagnostics.collider.cpuTimeUs        = SmoothMicros(m_diagnostics.collider.cpuTimeUs, colliderUsAccum);
        m_diagnostics.collider.operationsCount  = aabbChecksCount;

        m_diagnostics.totalUpdateUs =
            m_diagnostics.transform.cpuTimeUs +
            m_diagnostics.movement.cpuTimeUs +
            m_diagnostics.collider.cpuTimeUs;
    }

    void Scene::Render() const {
        using namespace Talos::Renderer;
        using Clock = std::chrono::steady_clock;

        double spriteUsAccum = 0.0;
        double textUsAccum   = 0.0;

        size_t activeEntityCount   = 0;
        size_t activeTransformCnt  = 0;
        size_t activeMovementCnt   = 0;
        size_t activeColliderCnt   = 0;
        size_t activeSpriteCount   = 0;
        size_t activeTextCount     = 0;
        size_t spriteDrawCalls     = 0;
        size_t textDrawCalls       = 0;

        size_t spriteHeapBytes     = 0;
        size_t activeSpriteHeap    = 0;
        size_t textHeapBytes       = 0;
        size_t activeTextHeap      = 0;
        size_t entityNameHeapBytes = 0;

        std::unordered_set<std::string> activeTextures;
        std::unordered_set<int> activeFontSizes;

        // 1. Gather memory footprint across all entities and collect active indices for depth sorting
        std::vector<size_t> renderOrder;
        renderOrder.reserve(m_entities.size());

        for (size_t i = 0; i < m_entities.size(); ++i) {
            const Entity& entity = m_entities[i];
            entityNameHeapBytes += entity.name.capacity();
            spriteHeapBytes     += entity.sprite.texturePath.capacity();
            textHeapBytes       += entity.text.content.capacity();

            if (!entity.active) {
                continue;
            }

            ++activeEntityCount;
            ++activeTransformCnt;
            if (entity.movement.enabled) ++activeMovementCnt;
            if (entity.collider.enabled) ++activeColliderCnt;
            renderOrder.push_back(i);
        }

        // 2. Sort active entities deterministically by (sortingLayer ASC, zOrder ASC, originalIndex ASC)
        auto tSortStart = Clock::now();
        std::sort(renderOrder.begin(), renderOrder.end(),
            [this](size_t idxA, size_t idxB) {
                const Entity& a = m_entities[idxA];
                const Entity& b = m_entities[idxB];
                if (a.sprite.sortingLayer != b.sprite.sortingLayer) {
                    return a.sprite.sortingLayer < b.sprite.sortingLayer;
                }
                if (a.sprite.zOrder != b.sprite.zOrder) {
                    return a.sprite.zOrder < b.sprite.zOrder;
                }
                return idxA < idxB;
            });
        auto tSortEnd = Clock::now();
        spriteUsAccum += std::chrono::duration<double, std::micro>(tSortEnd - tSortStart).count();

        // 3. Submit sorted entities' visual and text components from back to front
        for (size_t idx : renderOrder) {
            const Entity& entity = m_entities[idx];

            // SpriteComponent Rendering & Timing
            if (entity.sprite.enabled) {
                ++activeSpriteCount;
                activeSpriteHeap += entity.sprite.texturePath.capacity();

                auto tSpriteStart = Clock::now();
                if (!entity.sprite.texturePath.empty()) {
                    Renderer2D::DrawSprite(
                        entity.sprite.texturePath.c_str(),
                        entity.transform.x,
                        entity.transform.y,
                        entity.transform.width,
                        entity.transform.height,
                        entity.sprite.color
                    );
                    activeTextures.insert(entity.sprite.texturePath);
                } else {
                    Renderer2D::DrawRect(
                        entity.transform.x,
                        entity.transform.y,
                        entity.transform.width,
                        entity.transform.height,
                        entity.sprite.color
                    );
                }
                auto tSpriteEnd = Clock::now();
                spriteUsAccum += std::chrono::duration<double, std::micro>(tSpriteEnd - tSpriteStart).count();
                ++spriteDrawCalls;
            }

            // TextComponent Rendering & Timing
            if (entity.text.enabled) {
                ++activeTextCount;
                activeTextHeap += entity.text.content.capacity();

                if (!entity.text.content.empty()) {
                    auto tTextStart = Clock::now();
                    Renderer2D::DrawLabel(
                        entity.text.content.c_str(),
                        entity.transform.x + entity.text.offsetX,
                        entity.transform.y + entity.text.offsetY,
                        entity.text.fontSize,
                        entity.text.color
                    );
                    auto tTextEnd = Clock::now();
                    textUsAccum += std::chrono::duration<double, std::micro>(tTextEnd - tTextStart).count();
                    ++textDrawCalls;
                    activeFontSizes.insert(entity.text.fontSize);
                }
            }
        }

        const size_t totalCount = m_entities.size();
        m_diagnostics.totalEntities  = totalCount;
        m_diagnostics.activeEntities = activeEntityCount;

        // Compute exact RAM footprint per component across the scene
        m_diagnostics.transform.totalCount     = totalCount;
        m_diagnostics.transform.activeCount    = activeTransformCnt;
        m_diagnostics.transform.ramBytes       = totalCount * sizeof(TransformComponent);
        m_diagnostics.transform.activeRamBytes = activeTransformCnt * sizeof(TransformComponent);
        m_diagnostics.transform.vramBytes      = 0;

        m_diagnostics.movement.totalCount      = totalCount;
        m_diagnostics.movement.activeCount     = activeMovementCnt;
        m_diagnostics.movement.ramBytes        = totalCount * sizeof(MovementComponent);
        m_diagnostics.movement.activeRamBytes  = activeMovementCnt * sizeof(MovementComponent);
        m_diagnostics.movement.vramBytes       = 0;

        m_diagnostics.collider.totalCount      = totalCount;
        m_diagnostics.collider.activeCount     = activeColliderCnt;
        m_diagnostics.collider.ramBytes        = totalCount * sizeof(ColliderComponent);
        m_diagnostics.collider.activeRamBytes  = activeColliderCnt * sizeof(ColliderComponent);
        m_diagnostics.collider.vramBytes       = 0;

        // Compute exact GPU VRAM for unique textures referenced by active SpriteComponents
        size_t spriteVram = 0;
        for (const auto& texPath : activeTextures) {
            spriteVram += Renderer2D::GetTextureVRAMBytes(texPath.c_str());
        }

        m_diagnostics.sprite.totalCount      = totalCount;
        m_diagnostics.sprite.activeCount     = activeSpriteCount;
        m_diagnostics.sprite.rawCpuTimeUs    = spriteUsAccum;
        m_diagnostics.sprite.cpuTimeUs       = SmoothMicros(m_diagnostics.sprite.cpuTimeUs, spriteUsAccum);
        m_diagnostics.sprite.ramBytes        = totalCount * sizeof(SpriteComponent) + spriteHeapBytes;
        m_diagnostics.sprite.activeRamBytes  = activeSpriteCount * sizeof(SpriteComponent) + activeSpriteHeap;
        m_diagnostics.sprite.vramBytes       = spriteVram;
        m_diagnostics.sprite.operationsCount = spriteDrawCalls;

        // Compute exact GPU VRAM for unique TrueType font atlases referenced by active TextComponents
        size_t textVram = 0;
        for (int fontSize : activeFontSizes) {
            textVram += Renderer2D::GetFontAtlasVRAMBytes(fontSize);
        }

        m_diagnostics.text.totalCount      = totalCount;
        m_diagnostics.text.activeCount     = activeTextCount;
        m_diagnostics.text.rawCpuTimeUs    = textUsAccum;
        m_diagnostics.text.cpuTimeUs       = SmoothMicros(m_diagnostics.text.cpuTimeUs, textUsAccum);
        m_diagnostics.text.ramBytes        = totalCount * sizeof(TextComponent) + textHeapBytes;
        m_diagnostics.text.activeRamBytes  = activeTextCount * sizeof(TextComponent) + activeTextHeap;
        m_diagnostics.text.vramBytes       = textVram;
        m_diagnostics.text.operationsCount = textDrawCalls;

        m_diagnostics.totalRenderUs = m_diagnostics.sprite.cpuTimeUs + m_diagnostics.text.cpuTimeUs;
        m_diagnostics.totalSceneRamBytes =
            (m_entities.capacity() * sizeof(Entity)) +
            entityNameHeapBytes + spriteHeapBytes + textHeapBytes;
        m_diagnostics.totalSceneVramBytes = spriteVram + textVram;
    }

}
