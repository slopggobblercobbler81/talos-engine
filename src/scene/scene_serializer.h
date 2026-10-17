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
    };

}

