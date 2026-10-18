// Scene container: entity storage, per-frame update/render, runtime camera and diagnostics
#pragma once
#include <vector>
#include <string>
#include <cstddef>
#include <functional>
#include "scene/entity.h"

namespace Talos::Scene {

    // =========================================================================
    // Trigger / Sensor Overlap Events (Layer 3: Scene & Entity -> Scripting)
    // =========================================================================
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

        // Inserts a fully constructed Entity (e.g., deserialized from a .talos scene file)
        // and advances the internal ID counter so future entities never collide in ID.
        Entity& AddEntity(const Entity& entity);

        // Removes an entity matching the given ID from the scene container.
        void RemoveEntity(EntityID id);

        // Removes all entities from the scene and resets the ID counter to 1.
        // Used prior to loading a scene file from disk.
        void Clear();

        // Clears transient runtime trigger overlap state (called on Play/Stop transitions).
        void ResetTriggerState();

        // Registers a callback invoked whenever a Trigger (isTrigger) enters, stays, or exits overlap.
        void SetTriggerCallback(TriggerCallback callback);

        // Returns the list of Trigger events (Enter / Stay / Exit) emitted during the latest Update() step.
        const std::vector<TriggerOverlapEvent>& GetTriggerEvents() const { return m_frameTriggerEvents; }

        // Returns true if the given trigger entity is currently overlapping 'otherId'
        // (or any collider when otherId == 0).
        bool IsTriggerOverlapping(EntityID triggerId, EntityID otherId = 0) const;

        // Path-independent lookup by name. Unlike hierarchical NodePath systems,
        // finding an entity by name never breaks if the scene order is reorganized.
        // Returns nullptr if no entity with that name exists.
        Entity* FindEntity(const std::string& name);
        const Entity* FindEntity(const std::string& name) const;

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
        struct ActiveTriggerOverlap {
            EntityID triggerId = 0;
            EntityID otherId   = 0;
            std::string triggerName;
            std::string otherName;
        };

        // Contiguous storage in memory for cache-friendly iteration every frame.
        std::vector<Entity> m_entities;

        // Monotonically increasing counter guaranteeing unique EntityIDs.
        EntityID m_nextId = 1;

        // Active trigger overlaps from the previous simulation step (used to compute Enter/Stay/Exit).
        std::vector<ActiveTriggerOverlap> m_activeTriggerOverlaps;

        // Trigger events emitted during the most recent Update() call.
        std::vector<TriggerOverlapEvent> m_frameTriggerEvents;

        // Optional listener hook for Scripting / gameplay systems.
        TriggerCallback m_triggerCallback;

        // Live per-component diagnostic metrics updated during Update() and Render().
        mutable SceneDiagnostics m_diagnostics;
    };

}
