// Scene container: entity storage, per-frame update/render, runtime camera and diagnostics
#pragma once
#include <vector>
#include <string>
#include <cstddef>
#include <functional>
#include "scene/entity.h"

namespace Talos::Scene {

    // =========================================================================
    // Solid Collision & Trigger / Sensor Events (Layer 3: Scene -> Scripting)
    // =========================================================================
    // Lifecycle phase of a solid physical collision between two colliders.
    enum class CollisionEventState {
        Enter = 0, // First frame of physical contact between two solid colliders
        Stay  = 1, // Subsequent frames while the two solid colliders remain in contact
        Exit  = 2  // First frame after the two solid colliders separate
    };

    // Event record generated during Scene::Update for solid physical collisions.
    // Automatically dispatched to the Scripting layer and gameplay listeners.
    struct CollisionEvent {
        EntityID entityAId = 0;
        EntityID entityBId = 0;
        std::string entityAName;
        std::string entityBName;
        std::string entityATag;
        std::string entityBTag;
        float normalX = 0.0f;       // Contact normal X pointing from Entity A toward Entity B
        float normalY = 0.0f;       // Contact normal Y pointing from Entity A toward Entity B
        float penetration = 0.0f;   // Penetration depth prior to MTV separation
        float contactX = 0.0f;      // World-space contact point X
        float contactY = 0.0f;      // World-space contact point Y
        CollisionEventState state = CollisionEventState::Enter;
    };

    // Callback signature used by the Scripting layer or gameplay listeners to receive collision events.
    using CollisionCallback = std::function<void(Entity& entityA, Entity& entityB, const CollisionEvent& event)>;

    // Lifecycle phase of a non-blocking Trigger (isTrigger == true) overlap.
    enum class TriggerEventState {
        Enter = 0, // First frame of overlap between a trigger collider and another collider
        Stay  = 1, // Subsequent frames while the two colliders remain overlapping
        Exit  = 2  // First frame after the two colliders separate
    };

    // Event record generated during Scene::Update for script and gameplay dispatch.
    struct TriggerOverlapEvent {
        EntityID triggerId = 0;
        EntityID otherId   = 0;
        std::string triggerName;
        std::string otherName;
        std::string triggerTag;
        std::string otherTag;
        TriggerEventState state = TriggerEventState::Enter;
    };

    // Callback signature used by the Scripting layer or gameplay listeners to receive trigger events.
    using TriggerCallback = std::function<void(Entity& triggerEntity, Entity& otherEntity, TriggerEventState state)>;

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
        ComponentDiagnostics camera;
        ComponentDiagnostics audio;
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
    // their per-frame movement, collision resolution, trigger detection, camera tracking,
    // and rendering submission.
    class Scene {
    public:
        // Allocates a new Entity in the scene with a unique auto-incremented ID
        // and returns a reference so the caller can configure its components immediately.
        Entity& CreateEntity(const std::string& name, float x = 0.0f, float y = 0.0f);

        // Inserts a fully constructed Entity (e.g., deserialized from a .talsc scene file)
        // and advances the internal ID counter so future entities never collide in ID.
        Entity& AddEntity(const Entity& entity);

        // Clones an existing entity by ID (all components, tag, and active state), assigns a new
        // unique EntityID and sequential name (e.g., "Hero" -> "Hero_2"), offsets its position by
        // (offsetX, offsetY), and returns a pointer to the newly created clone (or nullptr if not found).
        Entity* DuplicateEntity(EntityID sourceId, float offsetX = 16.0f, float offsetY = 16.0f);

        // Removes an entity matching the given ID from the scene container.
        void RemoveEntity(EntityID id);

        // Returns the 0-based index of the entity in the scene hierarchy list, or -1 if not found.
        int GetEntityIndex(EntityID id) const;

        // Moves the entity with 'id' one position earlier (up) in the scene hierarchy list.
        // Returns true if the entity order changed, or false if not found or already at index 0.
        bool MoveEntityUp(EntityID id);

        // Moves the entity with 'id' one position later (down) in the scene hierarchy list.
        // Returns true if the entity order changed, or false if not found or already at the end.
        bool MoveEntityDown(EntityID id);

        // Moves the entity with 'id' to 'targetIndex' (clamped to valid bounds), shifting
        // intermediate entities while preserving their relative order. Returns true if moved.
        bool MoveEntityToIndex(EntityID id, size_t targetIndex);

        // Removes all entities from the scene and resets the ID counter to 1.
        // Used prior to loading a scene file from disk.
        void Clear();

        // =====================================================================
        // Multi-Scene Management & Transitions (.talsc)
        // =====================================================================
        // Callback invoked whenever a scene transition or reload completes.
        using SceneChangeCallback = std::function<void(const std::string& newScenePath)>;

        // Returns or updates the file path of the currently loaded .talsc scene.
        const std::string& GetSceneFilePath() const { return m_sceneFilePath; }
        void SetSceneFilePath(const std::string& filePath) { m_sceneFilePath = filePath; }

        // Switches to another .talsc scene file.
        // - If 'deferred' is false and Scene::Update() is not currently executing, loads the
        //   target scene immediately (stopping active audio, resetting collision/trigger state,
        //   and invoking the SceneChangeCallback).
        // - If 'deferred' is true OR called from inside a collision/trigger callback during
        //   Scene::Update(), queues the transition so it executes safely at the end of Update()
        //   without invalidating active entity references mid-step.
        bool ChangeScene(const std::string& filePath, bool deferred = false);

        // Explicitly queues a deferred transition to 'filePath' to be executed at the end of Update().
        void RequestSceneChange(const std::string& filePath);

        // Reloads the currently active .talsc scene from disk (immediate or deferred if mid-Update).
        bool ReloadCurrentScene(bool deferred = false);

        // Queues a deferred reload of the currently active .talsc scene at the end of Update().
        void RequestSceneReload();

        // Inspects or cancels a queued deferred scene transition.
        bool HasPendingSceneChange() const { return !m_pendingScenePath.empty(); }
        const std::string& GetPendingScenePath() const { return m_pendingScenePath; }
        void CancelPendingSceneChange() { m_pendingScenePath.clear(); }

        // Executes any queued deferred scene transition immediately. Automatically called at the end of Update().
        bool ApplyPendingSceneChange();

        // Registers an optional listener notified whenever ChangeScene / ReloadCurrentScene completes.
        void SetSceneChangeCallback(SceneChangeCallback callback);

        // Clears transient runtime collision and trigger overlap state (called on Play/Stop transitions).
        void ResetTriggerState();
        void ResetCollisionState();

        // Registers a callback invoked whenever two solid colliders enter, stay in, or exit physical contact.
        void SetCollisionCallback(CollisionCallback callback);

        // Returns the list of solid CollisionEvents (Enter / Stay / Exit) emitted during the latest Update() step.
        const std::vector<CollisionEvent>& GetCollisionEvents() const { return m_frameCollisionEvents; }

        // Returns true if 'entityAId' is currently in physical contact with 'entityBId'
        // (or any solid collider when entityBId == 0).
        bool IsColliding(EntityID entityAId, EntityID entityBId = 0) const;

        // Returns true if 'entityId' is currently in physical contact with any solid collider carrying 'targetTag'.
        bool IsCollidingWithTag(EntityID entityId, const std::string& targetTag) const;

        // Registers a callback invoked whenever a Trigger (isTrigger) enters, stays, or exits overlap.
        void SetTriggerCallback(TriggerCallback callback);

        // Returns the list of Trigger events (Enter / Stay / Exit) emitted during the latest Update() step.
        const std::vector<TriggerOverlapEvent>& GetTriggerEvents() const { return m_frameTriggerEvents; }

        // Returns true if the given trigger entity is currently overlapping 'otherId'
        // (or any collider when otherId == 0).
        bool IsTriggerOverlapping(EntityID triggerId, EntityID otherId = 0) const;

        // Returns true if the given trigger entity is currently overlapping any collider carrying 'targetTag'.
        bool IsTriggerOverlappingWithTag(EntityID triggerId, const std::string& targetTag) const;

        // Path-independent lookup by name. Unlike hierarchical NodePath systems,
        // finding an entity by name never breaks if the scene order is reorganized.
        // Returns nullptr if no entity with that name exists.
        Entity* FindEntity(const std::string& name);
        const Entity* FindEntity(const std::string& name) const;

        // Fast lookup by gameplay category tag (e.g., "Player", "Enemy", "Coin").
        // FindEntityByTag returns the first active entity matching the tag (or nullptr).
        // FindEntitiesByTag returns all active entities matching the tag.
        Entity* FindEntityByTag(const std::string& tag);
        const Entity* FindEntityByTag(const std::string& tag) const;
        std::vector<Entity*> FindEntitiesByTag(const std::string& tag);
        std::vector<const Entity*> FindEntitiesByTag(const std::string& tag) const;

        // Returns true if 'entity' matches both 'tagFilter' (exact case-insensitive tag when non-empty,
        // or untagged when "Untagged"/"None") and 'searchQuery' (case-insensitive substring match against
        // entity name or tag; also supports 'tag:<query>', '#<query>', and 'name:<query>' prefixes).
        static bool MatchesEntityFilter(const Entity& entity,
                                        const std::string& searchQuery,
                                        const std::string& tagFilter = "");

        // Returns all entities in hierarchy order that match 'searchQuery' and optional 'tagFilter'.
        std::vector<Entity*> FilterEntities(const std::string& searchQuery,
                                            const std::string& tagFilter = "");
        std::vector<const Entity*> FilterEntities(const std::string& searchQuery,
                                                  const std::string& tagFilter = "") const;

        // Fast lookup by unique numeric identifier. Returns nullptr if not found.
        Entity* FindEntityById(EntityID id);
        const Entity* FindEntityById(EntityID id) const;

        // Finds the active runtime CameraComponent entity (prioritizing isPrimary == true).
        // Returns nullptr if no active entity has camera.enabled == true.
        const Entity* FindActiveCameraEntity() const;

        // Resolves the active runtime Camera2DView for Play Mode or standalone game window rendering.
        // Centers the camera within the given screen/viewport rectangle (viewX, viewY, viewWidth, viewHeight).
        // Returns false if no active CameraComponent exists in the scene.
        bool ResolveRuntimeCamera(float viewX,
                                  float viewY,
                                  float viewWidth,
                                  float viewHeight,
                                  Renderer::Camera2DView& outCamera) const;

        // Direct access to the underlying contiguous entity storage (used by Editor/Systems).
        std::vector<Entity>& GetEntities();
        const std::vector<Entity>& GetEntities() const;

        // Advances 2D sprite sheet frame animations (currentFrame, frameTimer, UV sub-rect)
        // for all active entities with sprite.enabled && sprite.frameCount > 1.
        void UpdateAnimations(float delta);

        // Advances entity movement, sprite animations, solid collisions, trigger overlaps, and runtime camera tracking.
        void Update(float delta);

        // Submits all active visual and text components to the Renderer2D layer.
        void Render() const;

        // Returns the latest per-component runtime consumption diagnostics.
        const SceneDiagnostics& GetDiagnostics() const { return m_diagnostics; }

    private:
        struct ActiveCollisionPair {
            EntityID entityAId = 0;
            EntityID entityBId = 0;
            std::string entityAName;
            std::string entityBName;
            std::string entityATag;
            std::string entityBTag;
            float normalX = 0.0f;
            float normalY = 0.0f;
            float penetration = 0.0f;
            float contactX = 0.0f;
            float contactY = 0.0f;
        };

        struct ActiveTriggerOverlap {
            EntityID triggerId = 0;
            EntityID otherId   = 0;
            std::string triggerName;
            std::string otherName;
            std::string triggerTag;
            std::string otherTag;
        };

        // Contiguous storage in memory for cache-friendly iteration every frame.
        std::vector<Entity> m_entities;

        // Monotonically increasing counter guaranteeing unique EntityIDs.
        EntityID m_nextId = 1;

        // Active solid collision contacts from the previous simulation step (used to compute Enter/Stay/Exit).
        std::vector<ActiveCollisionPair> m_activeCollisions;

        // Solid collision events emitted during the most recent Update() call.
        std::vector<CollisionEvent> m_frameCollisionEvents;

        // Optional listener hook for Scripting / gameplay systems on solid collisions.
        CollisionCallback m_collisionCallback;

        // Active trigger overlaps from the previous simulation step (used to compute Enter/Stay/Exit).
        std::vector<ActiveTriggerOverlap> m_activeTriggerOverlaps;

        // Trigger events emitted during the most recent Update() call.
        std::vector<TriggerOverlapEvent> m_frameTriggerEvents;

        // Optional listener hook for Scripting / gameplay systems on trigger overlaps.
        TriggerCallback m_triggerCallback;

        // File path of the currently loaded .talsc scene (and any queued deferred scene transition).
        std::string m_sceneFilePath;
        std::string m_pendingScenePath;
        bool m_isUpdating = false;
        SceneChangeCallback m_sceneChangeCallback;

        // Live per-component diagnostic metrics updated during Update() and Render().
        mutable SceneDiagnostics m_diagnostics;
    };

}

