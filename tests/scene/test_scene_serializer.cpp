// Unit tests for SceneSerializer: round-trip, clamping, legacy keys, error handling
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
    CHECK(!e.sprite.enabled);
    CHECK(!e.text.enabled);
    CHECK(!e.collider.enabled);
    CHECK(!e.movement.enabled);
    CHECK(!e.camera.enabled);
    e.tag = "Player";
    e.active = false;
    e.transform.width = 64.0f;
    e.transform.height = 32.0f;
    e.sprite.enabled = true;
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
    e.collider.isTrigger = true;
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
    CHECK_EQ(r.tag, std::string("Player"));
    CHECK(r.HasTag("Player"));
    CHECK(!r.HasTag("Enemy"));
    CHECK(!r.active);
    CHECK_NEAR(r.transform.x, 12.5, 0.01);
    CHECK_NEAR(r.transform.y, -40.25, 0.01);
    CHECK_NEAR(r.transform.width, 64.0, 0.01);
    CHECK(r.sprite.enabled);
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
    CHECK(r.collider.isTrigger);
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

TEST(Trigger_DetectsOverlapAndEmitsEnterStayExitWithoutBlockingMovement) {
    Scene scene;

    // Stationary Circle Trigger Zone at [100..160, 0..60] (center = 130, 30, radius = 30)
    Entity& zone = scene.CreateEntity("CoinSensor", 100.0f, 0.0f);
    zone.transform.width = 60.0f;
    zone.transform.height = 60.0f;
    zone.sprite.enabled = false;
    zone.collider.enabled = true;
    zone.collider.shape = Talos::Physics::ColliderShape::Circle;
    zone.collider.isStatic = true;
    zone.collider.isTrigger = true;
    EntityID zoneId = zone.id;

    // Moving Box entity starting at x = 0, width = 40, height = 40, moving right
    Entity& runner = scene.CreateEntity("Runner", 0.0f, 10.0f);
    runner.transform.width = 40.0f;
    runner.transform.height = 40.0f;
    runner.sprite.enabled = false;
    runner.collider.enabled = true;
    runner.collider.shape = Talos::Physics::ColliderShape::Box;
    runner.collider.isSolid = true;
    runner.movement.enabled = true;
    runner.movement.playerControlled = false;
    runner.movement.gravityScale = 0.0f;
    EntityID runnerId = runner.id;

    int enterCount = 0;
    int stayCount  = 0;
    int exitCount  = 0;
    scene.SetTriggerCallback([&](Entity& trig, Entity& other, TriggerEventState state) {
        CHECK_EQ(trig.id, zoneId);
        CHECK_EQ(other.id, runnerId);
        if (state == TriggerEventState::Enter) ++enterCount;
        else if (state == TriggerEventState::Stay) ++stayCount;
        else if (state == TriggerEventState::Exit) ++exitCount;
    });

    // Step 1: Runner is far to the left (x = 0) -> no overlap
    scene.Update(0.0f);
    CHECK(scene.GetTriggerEvents().empty());
    CHECK(!scene.IsTriggerOverlapping(zoneId));

    // Step 2: Move Runner inside the trigger zone (x = 110) -> Enter event, position NOT pushed out
    Entity* rPtr = scene.FindEntityById(runnerId);
    rPtr->transform.x = 110.0f;
    scene.Update(0.0f);
    CHECK_NEAR(rPtr->transform.x, 110.0, 1e-4); // Pass-through: trigger never blocks or pushes body
    CHECK(scene.IsTriggerOverlapping(zoneId, runnerId));
    CHECK_EQ(scene.GetTriggerEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetTriggerEvents()[0].state == TriggerEventState::Enter);
    CHECK_EQ(enterCount, 1);
    CHECK_EQ(stayCount, 0);
    CHECK_EQ(exitCount, 0);

    // Step 3: Runner remains inside the trigger zone (x = 120) -> Stay event
    rPtr->transform.x = 120.0f;
    scene.Update(0.0f);
    CHECK_NEAR(rPtr->transform.x, 120.0, 1e-4);
    CHECK(scene.IsTriggerOverlapping(zoneId, runnerId));
    CHECK_EQ(scene.GetTriggerEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetTriggerEvents()[0].state == TriggerEventState::Stay);
    CHECK_EQ(enterCount, 1);
    CHECK_EQ(stayCount, 1);
    CHECK_EQ(exitCount, 0);

    // Step 4: Runner leaves the trigger zone completely (x = 250) -> Exit event
    rPtr->transform.x = 250.0f;
    scene.Update(0.0f);
    CHECK_NEAR(rPtr->transform.x, 250.0, 1e-4);
    CHECK(!scene.IsTriggerOverlapping(zoneId, runnerId));
    CHECK_EQ(scene.GetTriggerEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetTriggerEvents()[0].state == TriggerEventState::Exit);
    CHECK_EQ(enterCount, 1);
    CHECK_EQ(stayCount, 1);
    CHECK_EQ(exitCount, 1);
}

TEST(TagsAndCollisionEvents_NotifiesScriptsWithEnterStayExitAndTagQueries) {
    Scene scene;

    Entity& player = scene.CreateEntity("Hero", 0.0f, 100.0f);
    player.tag = "Player";
    player.transform.width = 40.0f;
    player.transform.height = 40.0f;
    player.collider.enabled = true;
    player.collider.isSolid = true;
    player.collider.isStatic = false;
    player.movement.enabled = true;
    EntityID playerId = player.id;

    Entity& enemyA = scene.CreateEntity("GoblinA", 100.0f, 100.0f);
    enemyA.tag = "Enemy";
    enemyA.transform.width = 40.0f;
    enemyA.transform.height = 40.0f;
    enemyA.collider.enabled = true;
    enemyA.collider.isSolid = true;
    enemyA.collider.isStatic = true;
    EntityID enemyAId = enemyA.id;

    Entity& enemyB = scene.CreateEntity("GoblinB", 300.0f, 100.0f);
    enemyB.tag = "Enemy";
    enemyB.transform.width = 40.0f;
    enemyB.transform.height = 40.0f;

    Entity& coinTrigger = scene.CreateEntity("GoldCoin", 70.0f, 100.0f);
    coinTrigger.tag = "Coin";
    coinTrigger.transform.width = 40.0f;
    coinTrigger.transform.height = 40.0f;
    coinTrigger.collider.enabled = true;
    coinTrigger.collider.isTrigger = true;
    EntityID coinId = coinTrigger.id;

    // Verify tag lookup helpers
    CHECK(scene.FindEntityByTag("Player") != nullptr);
    CHECK_EQ(scene.FindEntityByTag("Player")->id, playerId);
    CHECK(scene.FindEntityByTag("NonExistent") == nullptr);
    CHECK_EQ(scene.FindEntitiesByTag("Enemy").size(), static_cast<size_t>(2));

    int colEnter = 0, colStay = 0, colExit = 0;
    std::string lastTagA, lastTagB;
    scene.SetCollisionCallback([&](Entity& a, Entity& b, const CollisionEvent& ev) {
        (void)a;
        (void)b;
        if (ev.state == CollisionEventState::Enter) ++colEnter;
        else if (ev.state == CollisionEventState::Stay) ++colStay;
        else if (ev.state == CollisionEventState::Exit) ++colExit;
        lastTagA = ev.entityATag;
        lastTagB = ev.entityBTag;
    });

    // Step 1: Separated -> no solid collision or trigger overlap with Player
    scene.Update(0.0f);
    CHECK(!scene.IsColliding(playerId));
    CHECK(!scene.IsCollidingWithTag(playerId, "Enemy"));
    CHECK_EQ(colEnter, 0);

    // Step 2: Move Player to overlap EnemyA (and Coin trigger) at x = 80 -> Enter event
    Entity* pPtr = scene.FindEntityById(playerId);
    pPtr->transform.x = 80.0f;
    scene.Update(0.0f);
    CHECK(scene.IsColliding(playerId, enemyAId));
    CHECK(scene.IsCollidingWithTag(playerId, "Enemy"));
    CHECK(!scene.IsCollidingWithTag(playerId, "Wall"));
    CHECK(scene.IsTriggerOverlappingWithTag(coinId, "Player"));
    CHECK(scene.IsTriggerOverlappingWithTag(playerId, "Coin"));
    CHECK_EQ(scene.GetCollisionEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetCollisionEvents()[0].state == CollisionEventState::Enter);
    CHECK_EQ(lastTagA, std::string("Player"));
    CHECK_EQ(lastTagB, std::string("Enemy"));
    CHECK_EQ(colEnter, 1);
    CHECK_EQ(colStay, 0);
    CHECK_EQ(colExit, 0);

    // Step 3: Move Player into EnemyA again in the next frame -> Stay event
    pPtr->transform.x = 80.0f;
    scene.Update(0.0f);
    CHECK(scene.IsCollidingWithTag(playerId, "Enemy"));
    CHECK_EQ(scene.GetCollisionEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetCollisionEvents()[0].state == CollisionEventState::Stay);
    CHECK_EQ(colEnter, 1);
    CHECK_EQ(colStay, 1);
    CHECK_EQ(colExit, 0);

    // Step 4: Move Player far away (x = 0) -> Exit event
    pPtr->transform.x = 0.0f;
    scene.Update(0.0f);
    CHECK(!scene.IsColliding(playerId));
    CHECK(!scene.IsCollidingWithTag(playerId, "Enemy"));
    CHECK_EQ(scene.GetCollisionEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetCollisionEvents()[0].state == CollisionEventState::Exit);
    CHECK_EQ(colEnter, 1);
    CHECK_EQ(colStay, 1);
    CHECK_EQ(colExit, 1);
}

TEST_MAIN()
