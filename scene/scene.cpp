#include "scene/scene.h"
#include "core/input.h"
#include "physics/physics2d.h"
#include "renderer/renderer2d.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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

    const Entity* Scene::FindEntity(const std::string& name) const {
        for (const auto& entity : m_entities) {
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

    const Entity* Scene::FindEntityById(EntityID id) const {
        for (const auto& entity : m_entities) {
            if (entity.id == id) {
                return &entity;
            }
        }
        return nullptr;
    }

    const Entity* Scene::FindActiveCameraEntity() const {
        const Entity* fallbackCamera = nullptr;
        for (const auto& entity : m_entities) {
            if (!entity.active || !entity.camera.enabled) {
                continue;
            }
            if (entity.camera.isPrimary) {
                return &entity;
            }
            if (!fallbackCamera) {
                fallbackCamera = &entity;
            }
        }
        return fallbackCamera;
    }

    bool Scene::ResolveRuntimeCamera(float viewX,
                                     float viewY,
                                     float viewWidth,
                                     float viewHeight,
                                     Renderer::Camera2DView& outCamera) const {
        const Entity* camEntity = FindActiveCameraEntity();
        if (!camEntity) {
            return false;
        }

        const CameraComponent& cam = camEntity->camera;
        float centerX = cam.currentX;
        float centerY = cam.currentY;

        if (!cam.runtimeInitialized) {
            const Entity* target = camEntity;
            if (cam.followTarget && !cam.targetName.empty() && cam.targetName != "Self") {
                const Entity* found = FindEntity(cam.targetName);
                if (found && found->active) {
                    target = found;
                }
            }
            centerX = target->transform.x + target->transform.width * 0.5f + cam.offsetX;
            centerY = target->transform.y + target->transform.height * 0.5f + cam.offsetY;
        }

        outCamera.targetX = centerX;
        outCamera.targetY = centerY;
        outCamera.offsetX = viewX + viewWidth * 0.5f;
        outCamera.offsetY = viewY + viewHeight * 0.5f;
        outCamera.zoom    = std::max(0.20f, std::min(5.00f, cam.zoom));
        return true;
    }

    std::vector<Entity>& Scene::GetEntities() {
        return m_entities;
    }

    const std::vector<Entity>& Scene::GetEntities() const {
        return m_entities;
    }

    void Scene::UpdateAnimations(float delta) {
        using namespace Talos::Renderer;

        for (auto& entity : m_entities) {
            if (!entity.active || !entity.sprite.enabled) {
                continue;
            }

            if (entity.sprite.frameCount <= 1) {
                entity.sprite.frameCount = 1;
                entity.sprite.currentFrame = 0;
                continue;
            }

            int texW = 0, texH = 0;
            Renderer2D::GetTextureSize(entity.sprite.texturePath.c_str(), texW, texH);

            if (entity.sprite.isPlaying && entity.sprite.fps > 0.0f && delta > 0.0f) {
                float frameDuration = 1.0f / entity.sprite.fps;
                entity.sprite.frameTimer += delta;

                while (entity.sprite.frameTimer >= frameDuration) {
                    entity.sprite.frameTimer -= frameDuration;
                    ++entity.sprite.currentFrame;

                    if (entity.sprite.currentFrame >= entity.sprite.frameCount) {
                        if (entity.sprite.loop) {
                            entity.sprite.currentFrame = 0;
                        } else {
                            entity.sprite.currentFrame = entity.sprite.frameCount - 1;
                            entity.sprite.isPlaying = false;
                            entity.sprite.frameTimer = 0.0f;
                            break;
                        }
                    }
                }
            }

            entity.sprite.SyncFrameUV(texW, texH);
        }
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

        // 0. Advance 2D sprite sheet frame animations
        UpdateAnimations(delta);

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

        // 2. Semi-Implicit Euler Integration (MovementComponent: Input, Gravity & Velocity)
        constexpr float GRAVITY_2D = 580.0f;

        for (auto& entity : m_entities) {
            if (!entity.active || !entity.movement.enabled) {
                continue;
            }
            if (entity.collider.enabled && entity.collider.isStatic) {
                continue;
            }

            auto tMoveStart = Clock::now();
            ++activeMovementCount;
            ++movementOpsCount;

            if (entity.movement.playerControlled) {
                float inputVx = 0.0f;
                float inputVy = 0.0f;
                if (Input::IsActionDown("move_right") || Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) inputVx += entity.movement.speed;
                if (Input::IsActionDown("move_left")  || Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  inputVx -= entity.movement.speed;
                if (Input::IsActionDown("move_down")  || Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down))  inputVy += entity.movement.speed;
                if (Input::IsActionDown("move_up")    || Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up))    inputVy -= entity.movement.speed;

                entity.movement.velocityX = inputVx;
                if (std::fabs(entity.movement.gravityScale) > 0.001f) {
                    entity.movement.velocityY += GRAVITY_2D * entity.movement.gravityScale * delta;
                    if (std::fabs(inputVy) > 0.001f) {
                        entity.movement.velocityY = inputVy;
                    }
                } else {
                    entity.movement.velocityY = inputVy;
                }

                // Automatically mirror the sprite horizontally to face the movement direction
                if (inputVx < -0.01f) {
                    entity.sprite.flipX = true;
                } else if (inputVx > 0.01f) {
                    entity.sprite.flipX = false;
                }
            } else {
                // Autonomous physics body: integrate 2D gravity into vertical velocity,
                // or apply gentle top-down linear damping when gravityScale == 0.
                if (std::fabs(entity.movement.gravityScale) > 0.001f) {
                    entity.movement.velocityY += GRAVITY_2D * entity.movement.gravityScale * delta;
                } else if (delta > 0.0f) {
                    float damping = std::exp(-1.25f * delta);
                    entity.movement.velocityX *= damping;
                    entity.movement.velocityY *= damping;
                    if (std::fabs(entity.movement.velocityX) < 0.5f) entity.movement.velocityX = 0.0f;
                    if (std::fabs(entity.movement.velocityY) < 0.5f) entity.movement.velocityY = 0.0f;
                }
            }

            auto tMoveEnd = Clock::now();
            movementUsAccum += std::chrono::duration<double, std::micro>(tMoveEnd - tMoveStart).count();

            // Integrate world-space Transform position
            auto tTransStart = Clock::now();
            entity.transform.x += entity.movement.velocityX * delta;
            entity.transform.y += entity.movement.velocityY * delta;
            auto tTransEnd = Clock::now();
            transformUsAccum += std::chrono::duration<double, std::micro>(tTransEnd - tTransStart).count();
        }

        // =====================================================================
        // 3. Narrow-Phase 2D Collision Detection & Physical Resolution
        //    (Box vs Box, Circle vs Circle, and Mixed Circle vs Box)
        // =====================================================================
        auto tColStart = Clock::now();
        constexpr int SOLVER_ITERATIONS = 3;
        const size_t entityCount = m_entities.size();

        for (int iter = 0; iter < SOLVER_ITERATIONS; ++iter) {
            bool anyCollisionInPass = false;

            for (size_t i = 0; i < entityCount; ++i) {
                Entity& eA = m_entities[i];
                if (!eA.active || !eA.collider.enabled || !eA.collider.isSolid) {
                    continue;
                }

                for (size_t j = i + 1; j < entityCount; ++j) {
                    Entity& eB = m_entities[j];
                    if (!eB.active || !eB.collider.enabled || !eB.collider.isSolid) {
                        continue;
                    }

                    // Skip pairs where both colliders are immovable static obstacles
                    if (eA.collider.isStatic && eB.collider.isStatic) {
                        continue;
                    }

                    Shape2D shapeA = eA.collider.BuildShape2D(
                        eA.transform.x, eA.transform.y, eA.transform.width, eA.transform.height
                    );
                    Shape2D shapeB = eB.collider.BuildShape2D(
                        eB.transform.x, eB.transform.y, eB.transform.width, eB.transform.height
                    );

                    ++aabbChecksCount;
                    CollisionManifold manifold = Physics2D::ComputeManifold(shapeA, shapeB);
                    if (!manifold.colliding) {
                        continue;
                    }

                    anyCollisionInPass = true;

                    PhysicsBodyState bodyA = {
                        eA.transform.x,
                        eA.transform.y,
                        eA.movement.enabled ? eA.movement.velocityX : 0.0f,
                        eA.movement.enabled ? eA.movement.velocityY : 0.0f,
                        eA.movement.mass,
                        eA.collider.restitution,
                        eA.collider.isStatic
                    };

                    PhysicsBodyState bodyB = {
                        eB.transform.x,
                        eB.transform.y,
                        eB.movement.enabled ? eB.movement.velocityX : 0.0f,
                        eB.movement.enabled ? eB.movement.velocityY : 0.0f,
                        eB.movement.mass,
                        eB.collider.restitution,
                        eB.collider.isStatic
                    };

                    Physics2D::ResolveCollision(bodyA, bodyB, manifold);

                    eA.transform.x = bodyA.x;
                    eA.transform.y = bodyA.y;
                    if (eA.movement.enabled) {
                        eA.movement.velocityX = bodyA.velocityX;
                        eA.movement.velocityY = bodyA.velocityY;
                    }

                    eB.transform.x = bodyB.x;
                    eB.transform.y = bodyB.y;
                    if (eB.movement.enabled) {
                        eB.movement.velocityX = bodyB.velocityX;
                        eB.movement.velocityY = bodyB.velocityY;
                    }
                }
            }

            if (!anyCollisionInPass) {
                break;
            }
        }
        auto tColEnd = Clock::now();
        colliderUsAccum += std::chrono::duration<double, std::micro>(tColEnd - tColStart).count();

        // 4. Process CameraComponent (Fixed or Smooth Target Follow)
        double cameraUsAccum = 0.0;
        size_t activeCameraCount = 0;
        size_t cameraOpsCount = 0;

        for (auto& entity : m_entities) {
            if (!entity.active || !entity.camera.enabled) {
                continue;
            }

            auto tCamStart = Clock::now();
            ++activeCameraCount;
            ++cameraOpsCount;

            if (entity.camera.zoom < 0.20f) {
                entity.camera.zoom = 0.20f;
            }

            if (!entity.camera.followTarget) {
                // Fixed Camera Mode: lock onto the camera entity's initial world center + offset
                // on the first runtime frame so it remains stationary even if attached to a moving entity.
                if (!entity.camera.runtimeInitialized) {
                    entity.camera.currentX = entity.transform.x + entity.transform.width * 0.5f + entity.camera.offsetX;
                    entity.camera.currentY = entity.transform.y + entity.transform.height * 0.5f + entity.camera.offsetY;
                    entity.camera.runtimeInitialized = true;
                }
            } else {
                // Follow Target Mode: resolve target entity ("" or "Self" follows the owning entity)
                const Entity* target = &entity;
                if (!entity.camera.targetName.empty() && entity.camera.targetName != "Self") {
                    const Entity* found = FindEntity(entity.camera.targetName);
                    if (found && found->active) {
                        target = found;
                    }
                }

                float desiredX = target->transform.x + target->transform.width * 0.5f + entity.camera.offsetX;
                float desiredY = target->transform.y + target->transform.height * 0.5f + entity.camera.offsetY;

                if (!entity.camera.runtimeInitialized || !entity.camera.smoothFollow || delta <= 0.0f) {
                    entity.camera.currentX = desiredX;
                    entity.camera.currentY = desiredY;
                    entity.camera.runtimeInitialized = true;
                } else {
                    float speed = std::max(0.5f, entity.camera.followSpeed);
                    float alpha = 1.0f - std::exp(-speed * delta);
                    alpha = std::max(0.0f, std::min(1.0f, alpha));
                    entity.camera.currentX += (desiredX - entity.camera.currentX) * alpha;
                    entity.camera.currentY += (desiredY - entity.camera.currentY) * alpha;
                }
            }

            auto tCamEnd = Clock::now();
            cameraUsAccum += std::chrono::duration<double, std::micro>(tCamEnd - tCamStart).count();
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

        m_diagnostics.camera.activeCount        = activeCameraCount;
        m_diagnostics.camera.totalCount         = totalCount;
        m_diagnostics.camera.rawCpuTimeUs       = cameraUsAccum;
        m_diagnostics.camera.cpuTimeUs          = SmoothMicros(m_diagnostics.camera.cpuTimeUs, cameraUsAccum);
        m_diagnostics.camera.operationsCount    = cameraOpsCount;

        m_diagnostics.totalUpdateUs =
            m_diagnostics.transform.cpuTimeUs +
            m_diagnostics.movement.cpuTimeUs +
            m_diagnostics.collider.cpuTimeUs +
            m_diagnostics.camera.cpuTimeUs;
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
        size_t activeCameraCnt     = 0;
        size_t activeSpriteCount   = 0;
        size_t activeTextCount     = 0;
        size_t spriteDrawCalls     = 0;
        size_t textDrawCalls       = 0;

        size_t spriteHeapBytes     = 0;
        size_t activeSpriteHeap    = 0;
        size_t textHeapBytes       = 0;
        size_t activeTextHeap      = 0;
        size_t cameraHeapBytes     = 0;
        size_t activeCameraHeap    = 0;
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
            cameraHeapBytes     += entity.camera.targetName.capacity();

            if (!entity.active) {
                continue;
            }

            ++activeEntityCount;
            ++activeTransformCnt;
            if (entity.movement.enabled) ++activeMovementCnt;
            if (entity.collider.enabled) ++activeColliderCnt;
            if (entity.camera.enabled) {
                ++activeCameraCnt;
                activeCameraHeap += entity.camera.targetName.capacity();
            }
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
                        entity.sprite.color,
                        entity.sprite.sourceX,
                        entity.sprite.sourceY,
                        entity.sprite.sourceW,
                        entity.sprite.sourceH,
                        entity.sprite.flipX,
                        entity.sprite.flipY
                    );
                    activeTextures.insert(entity.sprite.texturePath);
                } else if (entity.collider.enabled && entity.collider.shape == Physics::ColliderShape::Circle) {
                    float r = entity.collider.GetEffectiveRadius(entity.transform.width, entity.transform.height);
                    Renderer2D::DrawCircle(
                        entity.transform.x + entity.transform.width * 0.5f,
                        entity.transform.y + entity.transform.height * 0.5f,
                        r,
                        entity.sprite.color
                    );
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

        m_diagnostics.camera.totalCount        = totalCount;
        m_diagnostics.camera.activeCount       = activeCameraCnt;
        m_diagnostics.camera.ramBytes          = totalCount * sizeof(CameraComponent) + cameraHeapBytes;
        m_diagnostics.camera.activeRamBytes    = activeCameraCnt * sizeof(CameraComponent) + activeCameraHeap;
        m_diagnostics.camera.vramBytes         = 0;

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
            entityNameHeapBytes + spriteHeapBytes + textHeapBytes + cameraHeapBytes;
        m_diagnostics.totalSceneVramBytes = spriteVram + textVram;
    }

}

