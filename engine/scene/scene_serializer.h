// Human-readable .talos scene save/load (SceneSerializer)
#pragma once
#include <string>
#include "scene/scene.h"

namespace Talos::Scene {

    // =========================================================================
    // Scene Serialization Subsystem (Layer 3: Scene & Entity)
    // =========================================================================
    // Saves and loads a Scene to/from disk using the human-readable '.talos'
    // text format.
    //
    // Why a flat, deterministic section-based format?
    // Complex YAML or deeply nested scene files often produce noisy Git diffs
    // and painful merge conflicts when entities are edited. By writing each
    // flat Entity as an isolated '[Entity]' block with fixed property ordering,
    // version control diffs remain minimal, readable, and deterministic.
    class SceneSerializer {
    public:
        // Writes all entities in 'scene' to 'filePath' (creating parent folders if needed).
        // Returns true on success, or false if the file could not be opened for writing.
        static bool Save(const Scene& scene, const std::string& filePath);

        // Clears 'scene' and populates it with entities parsed from 'filePath'.
        // Returns true on success, or false if the file does not exist or cannot be read.
        static bool Load(Scene& scene, const std::string& filePath);

        // Saves a single configured 'entity' as a reusable '.prefab' template file at 'filePath'
        // (creating parent folders if needed). Returns true on success.
        static bool SavePrefab(const Entity& entity, const std::string& filePath);

        // Loads a '.prefab' template file from 'filePath' into 'outEntity' (with id reset to 0
        // and transient runtime fields cleared). Returns true on success.
        static bool LoadPrefab(const std::string& filePath, Entity& outEntity);

        // Loads a '.prefab' template from 'filePath', assigns a unique EntityID and sequential
        // conflict-free name in 'scene', optionally overrides its world position when
        // 'useOverridePosition' is true, and appends the new instance to 'scene'.
        // Returns a pointer to the newly instantiated Entity in 'scene', or nullptr on failure.
        static Entity* InstantiatePrefab(Scene& scene,
                                         const std::string& filePath,
                                         float overrideX = 0.0f,
                                         float overrideY = 0.0f,
                                         bool useOverridePosition = false);

        // Builds a normalized file path (e.g., "examples/prefabs/gold_coin.prefab") from an entity name.
        static std::string BuildPrefabPath(const std::string& entityName,
                                           const std::string& directory = "examples/prefabs");
    };

}

