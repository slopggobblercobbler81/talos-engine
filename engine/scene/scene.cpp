#include "scene/scene.h"
#include "audio/audio_system.h"
#include "core/input.h"
#include "core/logger.h"
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
        for (const auto& existing : m_entities) {
            if (existing.id >= m_nextId) {
                m_nextId = existing.id + 1;
            }
        }

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
        for (const auto& existing : m_entities) {
            if (existing.id >= m_nextId) {
                m_nextId = existing.id + 1;
            }
        }

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

    Entity* Scene::DuplicateEntity(EntityID sourceId, float offsetX, float offsetY) {
        if (sourceId == 0) return nullptr;
        const Entity* src = FindEntityById(sourceId);
        if (!src) return nullptr;

        // Copy before mutating m_entities in case vector reallocation invalidates src.
        Entity clone = *src;

        for (const auto& existing : m_entities) {
            if (existing.id >= m_nextId) {
                m_nextId = existing.id + 1;
            }
        }
        clone.id = m_nextId++;

        // Build a unique sequential name (e.g., "Hero" -> "Hero_2", "Object_1" -> "Object_2").
        std::string baseName = clone.name.empty() ? "Entity" : clone.name;
        std::string stem = baseName;
        int nextSuffix = 2;

        size_t underscorePos = baseName.rfind('_');
        if (underscorePos != std::string::npos && underscorePos > 0 && underscorePos + 1 < baseName.size()) {
            bool allDigits = true;
            int parsedNum = 0;
            for (size_t i = underscorePos + 1; i < baseName.size(); ++i) {
                char c = baseName[i];
                if (c < '0' || c > '9') {
                    allDigits = false;
                    break;
                }
                if (parsedNum < 100000) {
                    parsedNum = parsedNum * 10 + (c - '0');
                }
            }
            if (allDigits) {
                stem = baseName.substr(0, underscorePos);
                nextSuffix = std::max(2, parsedNum + 1);
            }
        }

        while (true) {
            std::string candidate = stem + "_" + std::to_string(nextSuffix);
            if (!FindEntity(candidate)) {
                clone.name = candidate;
                break;
            }
            ++nextSuffix;
        }

        clone.transform.x += offsetX;
        clone.transform.y += offsetY;

        // Reset transient runtime flags so the cloned entity starts cleanly in Play Mode.
        clone.sprite.frameTimer = 0.0f;
        clone.camera.currentX = 0.0f;
        clone.camera.currentY = 0.0f;
        clone.camera.runtimeInitialized = false;
        clone.audio.runtimeStarted = false;

        m_entities.push_back(clone);
        return &m_entities.back();
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

    int Scene::GetEntityIndex(EntityID id) const {
        if (id == 0) return -1;
        for (size_t i = 0; i < m_entities.size(); ++i) {
            if (m_entities[i].id == id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool Scene::MoveEntityUp(EntityID id) {
        int idx = GetEntityIndex(id);
        if (idx <= 0) {
            return false;
        }
        std::swap(m_entities[static_cast<size_t>(idx)], m_entities[static_cast<size_t>(idx - 1)]);
        return true;
    }

    bool Scene::MoveEntityDown(EntityID id) {
        int idx = GetEntityIndex(id);
        if (idx < 0 || static_cast<size_t>(idx + 1) >= m_entities.size()) {
            return false;
        }
        std::swap(m_entities[static_cast<size_t>(idx)], m_entities[static_cast<size_t>(idx + 1)]);
        return true;
    }

    bool Scene::MoveEntityToIndex(EntityID id, size_t targetIndex) {
        if (m_entities.size() <= 1) {
            return false;
        }
        int curIdx = GetEntityIndex(id);
        if (curIdx < 0) {
            return false;
        }
        size_t clampedTarget = std::min(targetIndex, m_entities.size() - 1);
        size_t fromIdx = static_cast<size_t>(curIdx);
        if (fromIdx == clampedTarget) {
            return false;
        }

        Entity moving = std::move(m_entities[fromIdx]);
        if (fromIdx < clampedTarget) {
            for (size_t i = fromIdx; i < clampedTarget; ++i) {
                m_entities[i] = std::move(m_entities[i + 1]);
            }
        } else {
            for (size_t i = fromIdx; i > clampedTarget; --i) {
                m_entities[i] = std::move(m_entities[i - 1]);
            }
        }
        m_entities[clampedTarget] = std::move(moving);
        return true;
    }

    void Scene::Clear() {
        m_entities.clear();
        m_nextId = 1;
        ResetTriggerState();
        ResetCollisionState();
    }

    void Scene::ResetTriggerState() {
        m_activeTriggerOverlaps.clear();
        m_frameTriggerEvents.clear();
        ResetCollisionState();
        for (auto& entity : m_entities) {
            entity.camera.runtimeInitialized = false;
            entity.audio.runtimeStarted = false;
        }
    }

    void Scene::ResetCollisionState() {
        m_activeCollisions.clear();
        m_frameCollisionEvents.clear();
    }

    void Scene::SetCollisionCallback(CollisionCallback callback) {
        m_collisionCallback = std::move(callback);
    }

    bool Scene::IsColliding(EntityID entityAId, EntityID entityBId) const {
        for (const auto& active : m_activeCollisions) {
            bool matchesA = (active.entityAId == entityAId);
            bool matchesB = (active.entityBId == entityAId);
            if (!matchesA && !matchesB) {
                continue;
            }
            if (entityBId == 0) {
                return true;
            }
            EntityID partnerId = matchesA ? active.entityBId : active.entityAId;
            if (partnerId == entityBId) {
                return true;
            }
        }
        return false;
    }

    bool Scene::IsCollidingWithTag(EntityID entityId, const std::string& targetTag) const {
        if (targetTag.empty()) return false;
        for (const auto& active : m_activeCollisions) {
            if (active.entityAId == entityId && active.entityBTag == targetTag) {
                return true;
            }
            if (active.entityBId == entityId && active.entityATag == targetTag) {
                return true;
            }
        }
        return false;
    }

    void Scene::SetTriggerCallback(TriggerCallback callback) {
        m_triggerCallback = std::move(callback);
    }

    bool Scene::IsTriggerOverlapping(EntityID triggerId, EntityID otherId) const {
        for (const auto& active : m_activeTriggerOverlaps) {
            if (active.triggerId == triggerId && (otherId == 0 || active.otherId == otherId)) {
                return true;
            }
        }
        return false;
    }

    bool Scene::IsTriggerOverlappingWithTag(EntityID triggerId, const std::string& targetTag) const {
        if (targetTag.empty()) return false;
        for (const auto& active : m_activeTriggerOverlaps) {
            if (active.triggerId == triggerId && active.otherTag == targetTag) {
                return true;
            }
            if (active.otherId == triggerId && active.triggerTag == targetTag) {
                return true;
            }
        }
        return false;
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

    Entity* Scene::FindEntityByTag(const std::string& tag) {
        if (tag.empty()) return nullptr;
        for (auto& entity : m_entities) {
            if (entity.active && entity.tag == tag) {
                return &entity;
            }
        }
        return nullptr;
    }

    const Entity* Scene::FindEntityByTag(const std::string& tag) const {
        if (tag.empty()) return nullptr;
        for (const auto& entity : m_entities) {
            if (entity.active && entity.tag == tag) {
                return &entity;
            }
        }
        return nullptr;
    }

    std::vector<Entity*> Scene::FindEntitiesByTag(const std::string& tag) {
        std::vector<Entity*> matches;
        if (tag.empty()) return matches;
        for (auto& entity : m_entities) {
            if (entity.active && entity.tag == tag) {
                matches.push_back(&entity);
            }
        }
        return matches;
    }

    std::vector<const Entity*> Scene::FindEntitiesByTag(const std::string& tag) const {
        std::vector<const Entity*> matches;
        if (tag.empty()) return matches;
        for (const auto& entity : m_entities) {
            if (entity.active && entity.tag == tag) {
                matches.push_back(&entity);
            }
        }
        return matches;
    }

    namespace {

        std::string ToLowerAscii(const std::string& str) {
            std::string out = str;
            for (char& c : out) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
            }
            return out;
        }

        std::string TrimAscii(const std::string& str) {
            size_t start = 0;
            while (start < str.size() && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r' || str[start] == '\n')) {
                ++start;
            }
            size_t end = str.size();
            while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r' || str[end - 1] == '\n')) {
                --end;
            }
            return str.substr(start, end - start);
        }

    } // anonymous namespace

    bool Scene::MatchesEntityFilter(const Entity& entity,
                                    const std::string& searchQuery,
                                    const std::string& tagFilter) {
        // 1. Optional exact category tag filter
        std::string trimmedTagFilter = TrimAscii(tagFilter);
        if (!trimmedTagFilter.empty()) {
            std::string lowerFilter = ToLowerAscii(trimmedTagFilter);
            if (lowerFilter == "untagged" || lowerFilter == "none") {
                if (!entity.tag.empty()) {
                    return false;
                }
            } else if (lowerFilter != "all") {
                if (ToLowerAscii(entity.tag) != lowerFilter) {
                    return false;
                }
            }
        }

        // 2. Optional text search query (matches name or tag; supports tag:, #, name: prefixes)
        std::string trimmedQuery = TrimAscii(searchQuery);
        if (trimmedQuery.empty()) {
            return true;
        }

        std::string lowerQuery = ToLowerAscii(trimmedQuery);
        std::string lowerName  = ToLowerAscii(entity.name);
        std::string lowerTag   = ToLowerAscii(entity.tag);

        if (lowerQuery.rfind("tag:", 0) == 0) {
            std::string tagTerm = TrimAscii(lowerQuery.substr(4));
            if (tagTerm.empty()) return true;
            return lowerTag.find(tagTerm) != std::string::npos;
        }
        if (!lowerQuery.empty() && lowerQuery[0] == '#') {
            std::string tagTerm = TrimAscii(lowerQuery.substr(1));
            if (tagTerm.empty()) return true;
            return lowerTag.find(tagTerm) != std::string::npos;
        }
        if (lowerQuery.rfind("name:", 0) == 0) {
            std::string nameTerm = TrimAscii(lowerQuery.substr(5));
            if (nameTerm.empty()) return true;
            return lowerName.find(nameTerm) != std::string::npos;
        }

        return (lowerName.find(lowerQuery) != std::string::npos) ||
               (lowerTag.find(lowerQuery) != std::string::npos);
    }

    std::vector<Entity*> Scene::FilterEntities(const std::string& searchQuery,
                                               const std::string& tagFilter) {
        std::vector<Entity*> result;
        for (auto& entity : m_entities) {
            if (MatchesEntityFilter(entity, searchQuery, tagFilter)) {
                result.push_back(&entity);
            }
        }
        return result;
    }

    std::vector<const Entity*> Scene::FilterEntities(const std::string& searchQuery,
                                                     const std::string& tagFilter) const {
        std::vector<const Entity*> result;
        for (const auto& entity : m_entities) {
            if (MatchesEntityFilter(entity, searchQuery, tagFilter)) {
                result.push_back(&entity);
            }
        }
        return result;
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
        m_frameCollisionEvents.clear();
        std::vector<ActiveCollisionPair> currentCollisions;

        for (int iter = 0; iter < SOLVER_ITERATIONS; ++iter) {
            bool anyCollisionInPass = false;

            for (size_t i = 0; i < entityCount; ++i) {
                Entity& eA = m_entities[i];
                if (!eA.active || !eA.collider.enabled || !eA.collider.isSolid || eA.collider.isTrigger) {
                    continue;
                }

                for (size_t j = i + 1; j < entityCount; ++j) {
                    Entity& eB = m_entities[j];
                    if (!eB.active || !eB.collider.enabled || !eB.collider.isSolid || eB.collider.isTrigger) {
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

                    // Record unique colliding pair for this frame (using the initial contact manifold)
                    bool alreadyRecordedInFrame = false;
                    for (const auto& rec : currentCollisions) {
                        if (rec.entityAId == eA.id && rec.entityBId == eB.id) {
                            alreadyRecordedInFrame = true;
                            break;
                        }
                    }
                    if (!alreadyRecordedInFrame) {
                        currentCollisions.push_back({
                            eA.id,
                            eB.id,
                            eA.name,
                            eB.name,
                            eA.tag,
                            eB.tag,
                            manifold.normalX,
                            manifold.normalY,
                            manifold.penetration,
                            manifold.contactX,
                            manifold.contactY
                        });
                    }

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

        auto formatNameAndTag = [](const std::string& name, const std::string& tag) -> std::string {
            if (tag.empty()) {
                return "'" + name + "'";
            }
            return "'" + name + "' [" + tag + "]";
        };

        // Emit Enter & Stay events for solid collision contacts in this frame
        for (const auto& cur : currentCollisions) {
            bool wasActive = false;
            for (const auto& prev : m_activeCollisions) {
                if (prev.entityAId == cur.entityAId && prev.entityBId == cur.entityBId) {
                    wasActive = true;
                    break;
                }
            }

            CollisionEventState state = wasActive ? CollisionEventState::Stay : CollisionEventState::Enter;
            CollisionEvent ev = {
                cur.entityAId,
                cur.entityBId,
                cur.entityAName,
                cur.entityBName,
                cur.entityATag,
                cur.entityBTag,
                cur.normalX,
                cur.normalY,
                cur.penetration,
                cur.contactX,
                cur.contactY,
                state
            };
            m_frameCollisionEvents.push_back(ev);

            if (state == CollisionEventState::Enter) {
                TALOS_LOG_INFO("[Collision] " + formatNameAndTag(cur.entityAName, cur.entityATag) +
                               " collided with " + formatNameAndTag(cur.entityBName, cur.entityBTag));
            }

            if (m_collisionCallback) {
                Entity* entA = FindEntityById(cur.entityAId);
                Entity* entB = FindEntityById(cur.entityBId);
                if (entA && entB) {
                    m_collisionCallback(*entA, *entB, m_frameCollisionEvents.back());
                }
            }
        }

        // Emit Exit events for solid collision pairs that separated on this frame
        for (const auto& prev : m_activeCollisions) {
            bool stillActive = false;
            for (const auto& cur : currentCollisions) {
                if (cur.entityAId == prev.entityAId && cur.entityBId == prev.entityBId) {
                    stillActive = true;
                    break;
                }
            }

            if (!stillActive) {
                CollisionEvent ev = {
                    prev.entityAId,
                    prev.entityBId,
                    prev.entityAName,
                    prev.entityBName,
                    prev.entityATag,
                    prev.entityBTag,
                    prev.normalX,
                    prev.normalY,
                    0.0f,
                    prev.contactX,
                    prev.contactY,
                    CollisionEventState::Exit
                };
                m_frameCollisionEvents.push_back(ev);

                if (m_collisionCallback) {
                    Entity* entA = FindEntityById(prev.entityAId);
                    Entity* entB = FindEntityById(prev.entityBId);
                    if (entA && entB) {
                        m_collisionCallback(*entA, *entB, m_frameCollisionEvents.back());
                    }
                }
            }
        }

        m_activeCollisions = std::move(currentCollisions);

        // =====================================================================
        // 3b. Sensor / Trigger Overlap Detection & Event Dispatch (isTrigger)
        //     Detects Box/Circle overlaps without blocking physical movement
        //     and emits Enter / Stay / Exit events toward scripts & listeners.
        // =====================================================================
        m_frameTriggerEvents.clear();
        std::vector<ActiveTriggerOverlap> currentOverlaps;

        for (size_t i = 0; i < entityCount; ++i) {
            Entity& eA = m_entities[i];
            if (!eA.active || !eA.collider.enabled) {
                continue;
            }

            for (size_t j = i + 1; j < entityCount; ++j) {
                Entity& eB = m_entities[j];
                if (!eB.active || !eB.collider.enabled) {
                    continue;
                }

                // Only evaluate pairs where at least one collider is a Sensor / Trigger
                if (!eA.collider.isTrigger && !eB.collider.isTrigger) {
                    continue;
                }

                Shape2D shapeA = eA.collider.BuildShape2D(
                    eA.transform.x, eA.transform.y, eA.transform.width, eA.transform.height
                );
                Shape2D shapeB = eB.collider.BuildShape2D(
                    eB.transform.x, eB.transform.y, eB.transform.width, eB.transform.height
                );

                ++aabbChecksCount;
                if (!Physics2D::CheckOverlap(shapeA, shapeB)) {
                    continue;
                }

                if (eA.collider.isTrigger) {
                    currentOverlaps.push_back({ eA.id, eB.id, eA.name, eB.name, eA.tag, eB.tag });
                }
                if (eB.collider.isTrigger) {
                    currentOverlaps.push_back({ eB.id, eA.id, eB.name, eA.name, eB.tag, eA.tag });
                }
            }
        }

        // Emit Enter & Stay events for currently overlapping trigger pairs
        for (const auto& cur : currentOverlaps) {
            bool wasActive = false;
            for (const auto& prev : m_activeTriggerOverlaps) {
                if (prev.triggerId == cur.triggerId && prev.otherId == cur.otherId) {
                    wasActive = true;
                    break;
                }
            }

            TriggerEventState state = wasActive ? TriggerEventState::Stay : TriggerEventState::Enter;
            m_frameTriggerEvents.push_back({
                cur.triggerId, cur.otherId, cur.triggerName, cur.otherName, cur.triggerTag, cur.otherTag, state
            });

            if (state == TriggerEventState::Enter) {
                TALOS_LOG_INFO("[Trigger] " + formatNameAndTag(cur.otherName, cur.otherTag) +
                               " entered trigger " + formatNameAndTag(cur.triggerName, cur.triggerTag));
            }

            if (m_triggerCallback) {
                Entity* trigEnt  = FindEntityById(cur.triggerId);
                Entity* otherEnt = FindEntityById(cur.otherId);
                if (trigEnt && otherEnt) {
                    m_triggerCallback(*trigEnt, *otherEnt, state);
                }
            }
        }

        // Emit Exit events for trigger pairs that separated on this frame
        for (const auto& prev : m_activeTriggerOverlaps) {
            bool stillActive = false;
            for (const auto& cur : currentOverlaps) {
                if (cur.triggerId == prev.triggerId && cur.otherId == prev.otherId) {
                    stillActive = true;
                    break;
                }
            }

            if (!stillActive) {
                m_frameTriggerEvents.push_back({
                    prev.triggerId, prev.otherId, prev.triggerName, prev.otherName,
                    prev.triggerTag, prev.otherTag, TriggerEventState::Exit
                });
                TALOS_LOG_INFO("[Trigger] " + formatNameAndTag(prev.otherName, prev.otherTag) +
                               " exited trigger " + formatNameAndTag(prev.triggerName, prev.triggerTag));

                if (m_triggerCallback) {
                    Entity* trigEnt  = FindEntityById(prev.triggerId);
                    Entity* otherEnt = FindEntityById(prev.otherId);
                    if (trigEnt && otherEnt) {
                        m_triggerCallback(*trigEnt, *otherEnt, TriggerEventState::Exit);
                    }
                }
            }
        }

        m_activeTriggerOverlaps = std::move(currentOverlaps);

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

        // 5. Process AudioComponent (Sound Effects & Music Streams)
        double audioUsAccum = 0.0;
        size_t activeAudioCount = 0;
        size_t audioOpsCount = 0;

        for (auto& entity : m_entities) {
            if (!entity.active || !entity.audio.enabled) {
                if (entity.audio.runtimeStarted && !entity.audio.audioPath.empty()) {
                    if (entity.audio.isMusic) {
                        Audio::AudioSystem::StopMusic(entity.audio.audioPath);
                    } else {
                        Audio::AudioSystem::StopSound(entity.audio.audioPath);
                    }
                    entity.audio.runtimeStarted = false;
                }
                continue;
            }

            auto tAudioStart = Clock::now();
            ++activeAudioCount;

            if (!entity.audio.audioPath.empty()) {
                bool triggeredByCollision = false;
                if (entity.audio.playOnCollision) {
                    for (const auto& ev : m_frameCollisionEvents) {
                        if (ev.state == CollisionEventState::Enter &&
                            (ev.entityAId == entity.id || ev.entityBId == entity.id)) {
                            triggeredByCollision = true;
                            break;
                        }
                    }
                    if (!triggeredByCollision) {
                        for (const auto& trig : m_frameTriggerEvents) {
                            if (trig.state == TriggerEventState::Enter &&
                                (trig.triggerId == entity.id || trig.otherId == entity.id)) {
                                triggeredByCollision = true;
                                break;
                            }
                        }
                    }
                }

                if (entity.audio.isMusic) {
                    if (!entity.audio.runtimeStarted) {
                        entity.audio.runtimeStarted = true;
                        if (entity.audio.playOnStart) {
                            Audio::AudioSystem::PlayMusic(entity.audio.audioPath,
                                                          entity.audio.loop,
                                                          entity.audio.volume,
                                                          entity.audio.pitch,
                                                          entity.audio.pan);
                            ++audioOpsCount;
                        }
                    }
                    if (triggeredByCollision) {
                        Audio::AudioSystem::PlayMusic(entity.audio.audioPath,
                                                      entity.audio.loop,
                                                      entity.audio.volume,
                                                      entity.audio.pitch,
                                                      entity.audio.pan);
                        ++audioOpsCount;
                    }
                    if (Audio::AudioSystem::IsMusicPlaying(entity.audio.audioPath)) {
                        Audio::AudioSystem::SetMusicLooping(entity.audio.audioPath, entity.audio.loop);
                        Audio::AudioSystem::SetMusicVolume(entity.audio.audioPath, entity.audio.volume);
                        Audio::AudioSystem::SetMusicPitch(entity.audio.audioPath, entity.audio.pitch);
                        Audio::AudioSystem::SetMusicPan(entity.audio.audioPath, entity.audio.pan);
                        ++audioOpsCount;
                    }
                } else {
                    if (!entity.audio.runtimeStarted) {
                        entity.audio.runtimeStarted = true;
                        if (entity.audio.playOnStart) {
                            Audio::AudioSystem::PlaySound(entity.audio.audioPath,
                                                          entity.audio.volume,
                                                          entity.audio.pitch,
                                                          entity.audio.pan);
                            ++audioOpsCount;
                        }
                    } else if (entity.audio.loop && !Audio::AudioSystem::IsSoundPlaying(entity.audio.audioPath)) {
                        Audio::AudioSystem::PlaySound(entity.audio.audioPath,
                                                      entity.audio.volume,
                                                      entity.audio.pitch,
                                                      entity.audio.pan);
                        ++audioOpsCount;
                    }
                    if (triggeredByCollision) {
                        Audio::AudioSystem::PlaySound(entity.audio.audioPath,
                                                      entity.audio.volume,
                                                      entity.audio.pitch,
                                                      entity.audio.pan);
                        ++audioOpsCount;
                    }
                    if (Audio::AudioSystem::IsSoundPlaying(entity.audio.audioPath)) {
                        Audio::AudioSystem::SetSoundVolume(entity.audio.audioPath, entity.audio.volume);
                        Audio::AudioSystem::SetSoundPitch(entity.audio.audioPath, entity.audio.pitch);
                        Audio::AudioSystem::SetSoundPan(entity.audio.audioPath, entity.audio.pan);
                    }
                }
            }

            auto tAudioEnd = Clock::now();
            audioUsAccum += std::chrono::duration<double, std::micro>(tAudioEnd - tAudioStart).count();
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

        m_diagnostics.audio.activeCount         = activeAudioCount;
        m_diagnostics.audio.totalCount          = totalCount;
        m_diagnostics.audio.rawCpuTimeUs        = audioUsAccum;
        m_diagnostics.audio.cpuTimeUs           = SmoothMicros(m_diagnostics.audio.cpuTimeUs, audioUsAccum);
        m_diagnostics.audio.operationsCount     = audioOpsCount;

        m_diagnostics.totalUpdateUs =
            m_diagnostics.transform.cpuTimeUs +
            m_diagnostics.movement.cpuTimeUs +
            m_diagnostics.collider.cpuTimeUs +
            m_diagnostics.camera.cpuTimeUs +
            m_diagnostics.audio.cpuTimeUs;
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
        size_t activeAudioCnt      = 0;
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
        size_t audioHeapBytes      = 0;
        size_t activeAudioHeap     = 0;
        size_t entityNameHeapBytes = 0;

        std::unordered_set<std::string> activeTextures;
        std::unordered_set<int> activeFontSizes;

        // 1. Gather memory footprint across all entities and collect active indices for depth sorting
        std::vector<size_t> renderOrder;
        renderOrder.reserve(m_entities.size());

        for (size_t i = 0; i < m_entities.size(); ++i) {
            const Entity& entity = m_entities[i];
            entityNameHeapBytes += entity.name.capacity() + entity.tag.capacity();
            spriteHeapBytes     += entity.sprite.texturePath.capacity();
            textHeapBytes       += entity.text.content.capacity();
            cameraHeapBytes     += entity.camera.targetName.capacity();
            audioHeapBytes      += entity.audio.audioPath.capacity();

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
            if (entity.audio.enabled) {
                ++activeAudioCnt;
                activeAudioHeap += entity.audio.audioPath.capacity();
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

        m_diagnostics.audio.totalCount         = totalCount;
        m_diagnostics.audio.activeCount        = activeAudioCnt;
        m_diagnostics.audio.ramBytes           = totalCount * sizeof(AudioComponent) + audioHeapBytes +
                                                 Audio::AudioSystem::GetTotalSoundMemoryBytes() +
                                                 Audio::AudioSystem::GetTotalMusicMemoryBytes();
        m_diagnostics.audio.activeRamBytes     = activeAudioCnt * sizeof(AudioComponent) + activeAudioHeap +
                                                 Audio::AudioSystem::GetTotalSoundMemoryBytes() +
                                                 Audio::AudioSystem::GetTotalMusicMemoryBytes();
        m_diagnostics.audio.vramBytes          = 0;

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
            entityNameHeapBytes + spriteHeapBytes + textHeapBytes + cameraHeapBytes + audioHeapBytes +
            Audio::AudioSystem::GetTotalSoundMemoryBytes() + Audio::AudioSystem::GetTotalMusicMemoryBytes();
        m_diagnostics.totalSceneVramBytes = spriteVram + textVram;
    }

}

