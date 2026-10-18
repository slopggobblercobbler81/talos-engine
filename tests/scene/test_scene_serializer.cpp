#include "tests/test_framework.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include <filesystem>
#include <fstream>

using namespace Talos::Scene;
namespace fs = std::filesystem;

namespace {

    fs::path TempDir() {
        fs::path dir = fs::temp_directory_path() / "talos_scene_serializer_tests";
        fs::create_directories(dir);
        return dir;
    }

    void WriteText(const std::string& path, const std::string& text) {
        std::ofstream out(path, std::ios::trunc);
        out << text;
    }

}

TEST(RoundTrip_PreservesEntityData) {
    Scene original;
    Entity& e = original.CreateEntity("Hero", 12.5f, -40.25f);
    e.active = false;
    e.transform.width = 64.0f;
    e.transform.height = 32.0f;
    e.sprite.texturePath = "examples/assets/textures/hero.png";
    e.sprite.color = { 10, 20, 30, 40 };
    e.sprite.sortingLayer = 2;
    e.sprite.zOrder = -7;
    e.sprite.flipX = true;
    e.sprite.frameCount = 4;
    e.sprite.currentFrame = 2;
    e.sprite.fps = 12.0f;
    e.sprite.loop = false;
    e.text.content = "Hello world";
    e.text.fontSize = 22;
    e.collider.enabled = true;
    e.collider.shape = Talos::Physics::ColliderShape::Circle;
    e.collider.radius = 18.0f;
    e.collider.restitution = 0.5f;
    e.collider.isStatic = true;
    e.movement.enabled = true;
    e.movement.speed = 150.0f;
    e.movement.mass = 2.5f;
    e.camera.enabled = true;
    e.camera.targetName = "Player";
    e.camera.zoom = 1.5f;

    std::string path = (TempDir() / "roundtrip.talos").string();
    CHECK(SceneSerializer::Save(original, path));

    Scene loaded;
    CHECK(SceneSerializer::Load(loaded, path));
    CHECK_EQ(loaded.GetEntities().size(), static_cast<size_t>(1));
    if (loaded.GetEntities().empty()) return;

    const Entity& r = loaded.GetEntities()[0];
    CHECK_EQ(r.id, e.id);
    CHECK_EQ(r.name, std::string("Hero"));
    CHECK(!r.active);
    CHECK_NEAR(r.transform.x, 12.5, 0.01);
    CHECK_NEAR(r.transform.y, -40.25, 0.01);
    CHECK_NEAR(r.transform.width, 64.0, 0.01);
    CHECK_EQ(r.sprite.texturePath, e.sprite.texturePath);
    CHECK_EQ(static_cast<int>(r.sprite.color.r), 10);
    CHECK_EQ(static_cast<int>(r.sprite.color.a), 40);
    CHECK_EQ(r.sprite.sortingLayer, 2);
    CHECK_EQ(r.sprite.zOrder, -7);
    CHECK(r.sprite.flipX);
    CHECK_EQ(r.sprite.frameCount, 4);
    CHECK_EQ(r.sprite.currentFrame, 2);
    CHECK(!r.sprite.loop);
    CHECK_EQ(r.text.content, std::string("Hello world"));
    CHECK_EQ(r.text.fontSize, 22);
    CHECK(r.collider.shape == Talos::Physics::ColliderShape::Circle);
    CHECK_NEAR(r.collider.radius, 18.0, 0.01);
    CHECK_NEAR(r.collider.restitution, 0.5, 0.01);
    CHECK(r.collider.isStatic);
    CHECK_NEAR(r.movement.mass, 2.5, 0.01);
    CHECK_EQ(r.camera.targetName, std::string("Player"));
    CHECK_NEAR(r.camera.zoom, 1.5, 0.01);
}

TEST(RoundTrip_PreservesEntityOrderAndIds) {
    Scene original;
    original.CreateEntity("A");
    original.CreateEntity("B");
    original.CreateEntity("C");

    std::string path = (TempDir() / "order.talos").string();
    CHECK(SceneSerializer::Save(original, path));

    Scene loaded;
    CHECK(SceneSerializer::Load(loaded, path));
    CHECK_EQ(loaded.GetEntities().size(), static_cast<size_t>(3));
    if (loaded.GetEntities().size() != 3) return;
    CHECK_EQ(loaded.GetEntities()[0].name, std::string("A"));
    CHECK_EQ(loaded.GetEntities()[2].name, std::string("C"));
    CHECK_EQ(loaded.GetEntities()[1].id, original.GetEntities()[1].id);
}

TEST(Load_ReplacesPreviousSceneContents) {
    Scene source;
    source.CreateEntity("Only");
    std::string path = (TempDir() / "replace.talos").string();
    CHECK(SceneSerializer::Save(source, path));

    Scene target;
    target.CreateEntity("Stale1");
    target.CreateEntity("Stale2");
    CHECK(SceneSerializer::Load(target, path));
    CHECK_EQ(target.GetEntities().size(), static_cast<size_t>(1));
    CHECK(target.FindEntity("Stale1") == nullptr);
    CHECK(target.FindEntity("Only") != nullptr);
}

TEST(Load_NewEntitiesGetIdsAboveLoadedOnes) {
    Scene source;
    source.CreateEntity("A");
    source.CreateEntity("B");
    std::string path = (TempDir() / "ids.talos").string();
    CHECK(SceneSerializer::Save(source, path));

    Scene loaded;
    CHECK(SceneSerializer::Load(loaded, path));
    EntityID maxLoaded = 0;
    for (const auto& e : loaded.GetEntities()) {
        if (e.id > maxLoaded) maxLoaded = e.id;
    }
    Entity& fresh = loaded.CreateEntity("Fresh");
    CHECK(fresh.id > maxLoaded);
}

TEST(Load_MissingFileReturnsFalse) {
    Scene scene;
    CHECK(!SceneSerializer::Load(scene, (TempDir() / "does_not_exist.talos").string()));
}

TEST(Load_ClampsOutOfRangeValues) {
    std::string path = (TempDir() / "clamp.talos").string();
    WriteText(path,
        "[Entity]\n"
        "id = 1\n"
        "name = Clamped\n"
        "collider.restitution = 5.0\n"
        "camera.zoom = 0.0\n"
        "sprite.frameCount = -3\n"
        "movement.mass = 0\n"
        "sprite.color = 300,-5,128,255\n");

    Scene scene;
    CHECK(SceneSerializer::Load(scene, path));
    const Entity* e = scene.FindEntity("Clamped");
    CHECK(e != nullptr);
    if (!e) return;
    CHECK_NEAR(e->collider.restitution, 1.0, 1e-5);
    CHECK_NEAR(e->camera.zoom, 0.1, 1e-5);
    CHECK_EQ(e->sprite.frameCount, 1);
    CHECK(e->movement.mass >= 0.01f);
    CHECK_EQ(static_cast<int>(e->sprite.color.r), 255);
    CHECK_EQ(static_cast<int>(e->sprite.color.g), 0);
    CHECK_EQ(static_cast<int>(e->sprite.color.b), 128);
}

TEST(Load_ToleratesCommentsBlankLinesAndUnknownKeys) {
    std::string path = (TempDir() / "tolerant.talos").string();
    WriteText(path,
        "# comment\n"
        "stray = outside block\n"
        "\n"
        "[Entity]\n"
        "id = 7\n"
        "name = Tolerant\n"
        "not.a.real.key = 1\n"
        "this line has no equals sign\n"
        "transform.x = 3.0\n");

    Scene scene;
    CHECK(SceneSerializer::Load(scene, path));
    CHECK_EQ(scene.GetEntities().size(), static_cast<size_t>(1));
    const Entity* e = scene.FindEntity("Tolerant");
    CHECK(e != nullptr);
    if (e) CHECK_NEAR(e->transform.x, 3.0, 1e-5);
}

TEST(Load_AcceptsLegacySortingKeysAndNamedColliderShape) {
    std::string path = (TempDir() / "legacy.talos").string();
    WriteText(path,
        "[Entity]\n"
        "id = 1\n"
        "name = Legacy\n"
        "sortingLayer = 3\n"
        "zOrder = 9\n"
        "collider.shape = Circle\n");

    Scene scene;
    CHECK(SceneSerializer::Load(scene, path));
    const Entity* e = scene.FindEntity("Legacy");
    CHECK(e != nullptr);
    if (!e) return;
    CHECK_EQ(e->sprite.sortingLayer, 3);
    CHECK_EQ(e->sprite.zOrder, 9);
    CHECK(e->collider.shape == Talos::Physics::ColliderShape::Circle);
}

TEST(Save_CreatesMissingParentDirectories) {
    fs::remove_all(TempDir() / "nested");
    fs::path nested = TempDir() / "nested" / "deeper" / "scene.talos";

    Scene scene;
    scene.CreateEntity("X");
    CHECK(SceneSerializer::Save(scene, nested.string()));
    CHECK(fs::exists(nested));
}

TEST(Save_EmptySceneLoadsAsEmpty) {
    Scene empty;
    std::string path = (TempDir() / "empty.talos").string();
    CHECK(SceneSerializer::Save(empty, path));

    Scene loaded;
    loaded.CreateEntity("Stale");
    CHECK(SceneSerializer::Load(loaded, path));
    CHECK(loaded.GetEntities().empty());
}

TEST_MAIN()
