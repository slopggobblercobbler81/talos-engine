#include "scene/scene_serializer.h"
#include "core/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <cstdlib>
#include <algorithm>

namespace Talos::Scene {

    // Internal helper: Strips leading and trailing whitespace/newlines from a string.
    static std::string Trim(const std::string& str) {
        const char* whitespace = " \t\r\n";
        size_t first = str.find_first_not_of(whitespace);
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(whitespace);
        return str.substr(first, (last - first + 1));
    }

    // Internal helper: Parses an "R,G,B,A" string into a Renderer::ColorRGBA struct.
    static Renderer::ColorRGBA ParseColor(const std::string& value) {
        Renderer::ColorRGBA color = { 255, 255, 255, 255 };
        std::stringstream ss(value);
        std::string token;
        int channels[4] = { 255, 255, 255, 255 };
        int index = 0;

        while (std::getline(ss, token, ',') && index < 4) {
            int parsed = std::atoi(Trim(token).c_str());
            if (parsed < 0) parsed = 0;
            if (parsed > 255) parsed = 255;
            channels[index++] = parsed;
        }

        color.r = static_cast<uint8_t>(channels[0]);
        color.g = static_cast<uint8_t>(channels[1]);
        color.b = static_cast<uint8_t>(channels[2]);
        color.a = static_cast<uint8_t>(channels[3]);
        return color;
    }

    bool SceneSerializer::Save(const Scene& scene, const std::string& filePath) {
        // Automatically create parent directories (e.g., "scenes/") if they do not exist yet.
        std::filesystem::path pathObj(filePath);
        if (pathObj.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(pathObj.parent_path(), ec);
        }

        std::ofstream out(filePath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            TALOS_LOG_ERROR("Failed to open scene file for writing: '" + filePath + "'");
            return false;
        }

        out << "# Talos Engine Scene File (v1)\n";
        out << "# Human-readable, deterministic flat entity serialization\n\n";
        out << std::fixed << std::setprecision(2);

        for (const auto& e : scene.GetEntities()) {
            out << "[Entity]\n";
            out << "id = " << e.id << "\n";
            out << "name = " << e.name << "\n";
            out << "active = " << (e.active ? 1 : 0) << "\n";

            // TransformComponent
            out << "transform.x = " << e.transform.x << "\n";
            out << "transform.y = " << e.transform.y << "\n";
            out << "transform.width = " << e.transform.width << "\n";
            out << "transform.height = " << e.transform.height << "\n";

            // SpriteComponent
            out << "sprite.enabled = " << (e.sprite.enabled ? 1 : 0) << "\n";
            out << "sprite.texturePath = " << e.sprite.texturePath << "\n";
            out << "sprite.color = "
                << static_cast<int>(e.sprite.color.r) << ","
                << static_cast<int>(e.sprite.color.g) << ","
                << static_cast<int>(e.sprite.color.b) << ","
                << static_cast<int>(e.sprite.color.a) << "\n";
            out << "sprite.sortingLayer = " << e.sprite.sortingLayer << "\n";
            out << "sprite.zOrder = " << e.sprite.zOrder << "\n";
            out << "sprite.sourceX = " << e.sprite.sourceX << "\n";
            out << "sprite.sourceY = " << e.sprite.sourceY << "\n";
            out << "sprite.sourceW = " << e.sprite.sourceW << "\n";
            out << "sprite.sourceH = " << e.sprite.sourceH << "\n";
            out << "sprite.flipX = " << (e.sprite.flipX ? 1 : 0) << "\n";
            out << "sprite.flipY = " << (e.sprite.flipY ? 1 : 0) << "\n";
            out << "sprite.frameCount = " << e.sprite.frameCount << "\n";
            out << "sprite.currentFrame = " << e.sprite.currentFrame << "\n";
            out << "sprite.fps = " << e.sprite.fps << "\n";
            out << "sprite.loop = " << (e.sprite.loop ? 1 : 0) << "\n";
            out << "sprite.isPlaying = " << (e.sprite.isPlaying ? 1 : 0) << "\n";

            // TextComponent
            out << "text.enabled = " << (e.text.enabled ? 1 : 0) << "\n";
            out << "text.content = " << e.text.content << "\n";
            out << "text.fontSize = " << e.text.fontSize << "\n";
            out << "text.color = "
                << static_cast<int>(e.text.color.r) << ","
                << static_cast<int>(e.text.color.g) << ","
                << static_cast<int>(e.text.color.b) << ","
                << static_cast<int>(e.text.color.a) << "\n";
            out << "text.offsetX = " << e.text.offsetX << "\n";
            out << "text.offsetY = " << e.text.offsetY << "\n";

            // ColliderComponent
            out << "collider.enabled = " << (e.collider.enabled ? 1 : 0) << "\n";
            out << "collider.shape = " << static_cast<int>(e.collider.shape) << "\n";
            out << "collider.radius = " << e.collider.radius << "\n";
            out << "collider.restitution = " << e.collider.restitution << "\n";
            out << "collider.isSolid = " << (e.collider.isSolid ? 1 : 0) << "\n";
            out << "collider.isStatic = " << (e.collider.isStatic ? 1 : 0) << "\n";

            // MovementComponent
            out << "movement.enabled = " << (e.movement.enabled ? 1 : 0) << "\n";
            out << "movement.playerControlled = " << (e.movement.playerControlled ? 1 : 0) << "\n";
            out << "movement.speed = " << e.movement.speed << "\n";
            out << "movement.mass = " << e.movement.mass << "\n";
            out << "movement.gravityScale = " << e.movement.gravityScale << "\n";
            out << "movement.velocityX = " << e.movement.velocityX << "\n";
            out << "movement.velocityY = " << e.movement.velocityY << "\n";

            // CameraComponent
            out << "camera.enabled = " << (e.camera.enabled ? 1 : 0) << "\n";
            out << "camera.isPrimary = " << (e.camera.isPrimary ? 1 : 0) << "\n";
            out << "camera.followTarget = " << (e.camera.followTarget ? 1 : 0) << "\n";
            out << "camera.smoothFollow = " << (e.camera.smoothFollow ? 1 : 0) << "\n";
            out << "camera.targetName = " << e.camera.targetName << "\n";
            out << "camera.followSpeed = " << e.camera.followSpeed << "\n";
            out << "camera.zoom = " << e.camera.zoom << "\n";
            out << "camera.offsetX = " << e.camera.offsetX << "\n";
            out << "camera.offsetY = " << e.camera.offsetY << "\n\n";
        }

        TALOS_LOG_INFO("Scene saved to '" + filePath + "' (" +
                       std::to_string(scene.GetEntities().size()) + " entities)");
        return true;
    }

    bool SceneSerializer::Load(Scene& scene, const std::string& filePath) {
        std::ifstream in(filePath, std::ios::in);
        if (!in.is_open()) {
            TALOS_LOG_WARN("Scene file not found or could not be opened: '" + filePath + "'");
            return false;
        }

        scene.Clear();

        Entity currentEntity;
        bool hasActiveEntity = false;
        std::string line;
        int fileLineNumber = 0;

        while (std::getline(in, line)) {
            ++fileLineNumber;
            std::string trimmed = Trim(line);

            // Skip blank lines and comment lines starting with '#'
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            // Start of a new [Entity] block
            if (trimmed == "[Entity]") {
                if (hasActiveEntity) {
                    scene.AddEntity(currentEntity);
                }
                currentEntity = Entity{};
                hasActiveEntity = true;
                continue;
            }

            if (!hasActiveEntity) {
                Core::Logger::Write(Core::LogLevel::Warning,
                                    "Ignored statement outside [Entity] block: \"" + trimmed + "\"",
                                    filePath.c_str(), fileLineNumber);
                continue;
            }

            // Parse "key = value" pair
            size_t eqPos = trimmed.find('=');
            if (eqPos == std::string::npos) {
                Core::Logger::Write(Core::LogLevel::Error,
                                    "Malformed scene entry (missing '='): \"" + trimmed + "\"",
                                    filePath.c_str(), fileLineNumber);
                continue;
            }

            std::string key = Trim(trimmed.substr(0, eqPos));
            std::string val = Trim(trimmed.substr(eqPos + 1));

            if (key == "id")                        currentEntity.id = static_cast<EntityID>(std::stoul(val));
            else if (key == "name")                 currentEntity.name = val;
            else if (key == "active")               currentEntity.active = (std::atoi(val.c_str()) != 0);
            else if (key == "transform.x")          currentEntity.transform.x = std::strtof(val.c_str(), nullptr);
            else if (key == "transform.y")          currentEntity.transform.y = std::strtof(val.c_str(), nullptr);
            else if (key == "transform.width")      currentEntity.transform.width = std::strtof(val.c_str(), nullptr);
            else if (key == "transform.height")     currentEntity.transform.height = std::strtof(val.c_str(), nullptr);
            else if (key == "sprite.enabled")       currentEntity.sprite.enabled = (std::atoi(val.c_str()) != 0);
            else if (key == "sprite.texturePath")   currentEntity.sprite.texturePath = val;
            else if (key == "sprite.color")         currentEntity.sprite.color = ParseColor(val);
            else if (key == "sprite.sortingLayer" || key == "sortingLayer") currentEntity.sprite.sortingLayer = std::atoi(val.c_str());
            else if (key == "sprite.zOrder" || key == "zOrder")             currentEntity.sprite.zOrder = std::atoi(val.c_str());
            else if (key == "sprite.sourceX")       currentEntity.sprite.sourceX = std::strtof(val.c_str(), nullptr);
            else if (key == "sprite.sourceY")       currentEntity.sprite.sourceY = std::strtof(val.c_str(), nullptr);
            else if (key == "sprite.sourceW")       currentEntity.sprite.sourceW = std::strtof(val.c_str(), nullptr);
            else if (key == "sprite.sourceH")       currentEntity.sprite.sourceH = std::strtof(val.c_str(), nullptr);
            else if (key == "sprite.flipX")         currentEntity.sprite.flipX = (std::atoi(val.c_str()) != 0);
            else if (key == "sprite.flipY")         currentEntity.sprite.flipY = (std::atoi(val.c_str()) != 0);
            else if (key == "sprite.frameCount")    currentEntity.sprite.frameCount = std::max(1, std::atoi(val.c_str()));
            else if (key == "sprite.currentFrame")  currentEntity.sprite.currentFrame = std::max(0, std::atoi(val.c_str()));
            else if (key == "sprite.fps")           currentEntity.sprite.fps = std::max(0.0f, std::strtof(val.c_str(), nullptr));
            else if (key == "sprite.loop")          currentEntity.sprite.loop = (std::atoi(val.c_str()) != 0);
            else if (key == "sprite.isPlaying")     currentEntity.sprite.isPlaying = (std::atoi(val.c_str()) != 0);
            else if (key == "text.enabled")         currentEntity.text.enabled = (std::atoi(val.c_str()) != 0);
            else if (key == "text.content")         currentEntity.text.content = val;
            else if (key == "text.fontSize")        currentEntity.text.fontSize = std::atoi(val.c_str());
            else if (key == "text.color")           currentEntity.text.color = ParseColor(val);
            else if (key == "text.offsetX")         currentEntity.text.offsetX = std::strtof(val.c_str(), nullptr);
            else if (key == "text.offsetY")         currentEntity.text.offsetY = std::strtof(val.c_str(), nullptr);
            else if (key == "collider.enabled")     currentEntity.collider.enabled = (std::atoi(val.c_str()) != 0);
            else if (key == "collider.shape") {
                if (val == "Circle" || val == "circle" || std::atoi(val.c_str()) == 1) {
                    currentEntity.collider.shape = Physics::ColliderShape::Circle;
                } else {
                    currentEntity.collider.shape = Physics::ColliderShape::Box;
                }
            }
            else if (key == "collider.radius")      currentEntity.collider.radius = std::max(0.0f, std::strtof(val.c_str(), nullptr));
            else if (key == "collider.restitution") currentEntity.collider.restitution = std::max(0.0f, std::min(1.0f, std::strtof(val.c_str(), nullptr)));
            else if (key == "collider.isSolid")     currentEntity.collider.isSolid = (std::atoi(val.c_str()) != 0);
            else if (key == "collider.isStatic")    currentEntity.collider.isStatic = (std::atoi(val.c_str()) != 0);
            else if (key == "movement.enabled")     currentEntity.movement.enabled = (std::atoi(val.c_str()) != 0);
            else if (key == "movement.playerControlled") currentEntity.movement.playerControlled = (std::atoi(val.c_str()) != 0);
            else if (key == "movement.speed")       currentEntity.movement.speed = std::strtof(val.c_str(), nullptr);
            else if (key == "movement.mass")        currentEntity.movement.mass = std::max(0.01f, std::strtof(val.c_str(), nullptr));
            else if (key == "movement.gravityScale") currentEntity.movement.gravityScale = std::strtof(val.c_str(), nullptr);
            else if (key == "movement.velocityX")   currentEntity.movement.velocityX = std::strtof(val.c_str(), nullptr);
            else if (key == "movement.velocityY")   currentEntity.movement.velocityY = std::strtof(val.c_str(), nullptr);
            else if (key == "camera.enabled")       currentEntity.camera.enabled = (std::atoi(val.c_str()) != 0);
            else if (key == "camera.isPrimary")     currentEntity.camera.isPrimary = (std::atoi(val.c_str()) != 0);
            else if (key == "camera.followTarget")  currentEntity.camera.followTarget = (std::atoi(val.c_str()) != 0);
            else if (key == "camera.smoothFollow")  currentEntity.camera.smoothFollow = (std::atoi(val.c_str()) != 0);
            else if (key == "camera.targetName")    currentEntity.camera.targetName = val;
            else if (key == "camera.followSpeed")   currentEntity.camera.followSpeed = std::max(0.0f, std::strtof(val.c_str(), nullptr));
            else if (key == "camera.zoom")          currentEntity.camera.zoom = std::max(0.1f, std::strtof(val.c_str(), nullptr));
            else if (key == "camera.offsetX")       currentEntity.camera.offsetX = std::strtof(val.c_str(), nullptr);
            else if (key == "camera.offsetY")       currentEntity.camera.offsetY = std::strtof(val.c_str(), nullptr);
            else {
                Core::Logger::Write(Core::LogLevel::Warning,
                                    "Unknown entity property key '" + key + "'",
                                    filePath.c_str(), fileLineNumber);
            }
        }

        // Commit the final entity block at EOF
        if (hasActiveEntity) {
            scene.AddEntity(currentEntity);
        }

        TALOS_LOG_INFO("Scene loaded from '" + filePath + "' (" +
                       std::to_string(scene.GetEntities().size()) + " entities)");
        return true;
    }

}
