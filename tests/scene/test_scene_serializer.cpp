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
    CHECK(!e.audio.enabled);
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
    e.audio.enabled = true;
    e.audio.audioPath = "examples/assets/audio/coin.wav";
    e.audio.isMusic = true;
    e.audio.playOnStart = false;
    e.audio.loop = true;
    e.audio.playOnCollision = true;
    e.audio.volume = 0.75f;
    e.audio.pitch = 1.25f;
    e.audio.pan = 0.25f;

    std::string path = (TempDir() / "roundtrip.talsc").string();
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
    CHECK(r.audio.enabled);
    CHECK_EQ(r.audio.audioPath, std::string("examples/assets/audio/coin.wav"));
    CHECK(r.audio.isMusic);
    CHECK(!r.audio.playOnStart);
    CHECK(r.audio.loop);
    CHECK(r.audio.playOnCollision);
    CHECK_NEAR(r.audio.volume, 0.75, 0.01);
    CHECK_NEAR(r.audio.pitch, 1.25, 0.01);
    CHECK_NEAR(r.audio.pan, 0.25, 0.01);
}

TEST(RoundTrip_PreservesEntityOrderAndIds) {
    Scene original;
    original.CreateEntity("A");
    original.CreateEntity("B");
    original.CreateEntity("C");

    std::string path = (TempDir() / "order.talsc").string();
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
    std::string path = (TempDir() / "replace.talsc").string();
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
    std::string path = (TempDir() / "ids.talsc").string();
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
    CHECK(!SceneSerializer::Load(scene, (TempDir() / "does_not_exist.talsc").string()));
}

TEST(Load_ClampsOutOfRangeValues) {
    std::string path = (TempDir() / "clamp.talsc").string();
    WriteText(path,
        "[Entity]\n"
        "id = 1\n"
        "name = Clamped\n"
        "collider.restitution = 5.0\n"
        "camera.zoom = 0.0\n"
        "sprite.frameCount = -3\n"
        "movement.mass = 0\n"
        "sprite.color = 300,-5,128,255\n"
        "audio.volume = 2.5\n"
        "audio.pitch = 0.01\n"
        "audio.pan = -1.0\n");

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
    CHECK_NEAR(e->audio.volume, 1.0, 1e-5);
    CHECK_NEAR(e->audio.pitch, 0.05, 1e-5);
    CHECK_NEAR(e->audio.pan, 0.0, 1e-5);
}

TEST(Load_ToleratesCommentsBlankLinesAndUnknownKeys) {
    std::string path = (TempDir() / "tolerant.talsc").string();
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
    std::string path = (TempDir() / "legacy.talsc").string();
    WriteText(path,
        "[Entity]\n"
        "id = 1\n"
        "name = Legacy\n"
        "sortingLayer = 3\n"
        "zOrder = 9\n"
        "collider.shape = Circle\n"
        "audio.clipPath = examples/assets/audio/jump.wav\n"
        "audioSource.enabled = 1\n");

    Scene scene;
    CHECK(SceneSerializer::Load(scene, path));
    const Entity* e = scene.FindEntity("Legacy");
    CHECK(e != nullptr);
    if (!e) return;
    CHECK_EQ(e->sprite.sortingLayer, 3);
    CHECK_EQ(e->sprite.zOrder, 9);
    CHECK(e->collider.shape == Talos::Physics::ColliderShape::Circle);
    CHECK(e->audio.enabled);
    CHECK_EQ(e->audio.audioPath, std::string("examples/assets/audio/jump.wav"));
}

TEST(Save_CreatesMissingParentDirectories) {
    fs::remove_all(TempDir() / "nested");
    fs::path nested = TempDir() / "nested" / "deeper" / "scene.talsc";

    Scene scene;
    scene.CreateEntity("X");
    CHECK(SceneSerializer::Save(scene, nested.string()));
    CHECK(fs::exists(nested));
}

TEST(Save_EmptySceneLoadsAsEmpty) {
    Scene empty;
    std::string path = (TempDir() / "empty.talsc").string();
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

TEST(DuplicateEntity_ClonesComponentsAndGeneratesUniqueIdsAndNames) {
    Scene scene;
    Entity& orig = scene.CreateEntity("Hero", 100.0f, 200.0f);
    orig.tag = "Player";
    orig.active = true;
    orig.transform.width = 48.0f;
    orig.transform.height = 64.0f;
    orig.sprite.enabled = true;
    orig.sprite.texturePath = "examples/assets/textures/hero.png";
    orig.sprite.color = { 12, 34, 56, 255 };
    orig.sprite.sortingLayer = SortingLayer::Characters;
    orig.sprite.zOrder = 5;
    orig.sprite.flipX = true;
    orig.sprite.frameCount = 4;
    orig.sprite.currentFrame = 2;
    orig.sprite.frameTimer = 0.75f;
    orig.text.enabled = true;
    orig.text.content = "Hero Label";
    orig.text.fontSize = 20;
    orig.collider.enabled = true;
    orig.collider.shape = Talos::Physics::ColliderShape::Circle;
    orig.collider.radius = 24.0f;
    orig.collider.restitution = 0.6f;
    orig.collider.isTrigger = true;
    orig.movement.enabled = true;
    orig.movement.speed = 250.0f;
    orig.movement.mass = 2.0f;
    orig.movement.gravityScale = 1.0f;
    orig.camera.enabled = true;
    orig.camera.zoom = 1.5f;
    orig.camera.runtimeInitialized = true;
    orig.audio.enabled = true;
    orig.audio.audioPath = "examples/assets/audio/jump.wav";
    orig.audio.volume = 0.8f;
    orig.audio.runtimeStarted = true;

    EntityID origId = orig.id;

    // Invalid IDs return nullptr without mutating the scene
    CHECK(scene.DuplicateEntity(0) == nullptr);
    CHECK(scene.DuplicateEntity(999) == nullptr);
    CHECK_EQ(scene.GetEntities().size(), static_cast<size_t>(1));

    // First duplication: "Hero" -> "Hero_2" with default +16/+16 offset
    Entity* clone1 = scene.DuplicateEntity(origId);
    CHECK(clone1 != nullptr);
    if (!clone1) return;

    EntityID clone1Id = clone1->id;
    CHECK(clone1Id != origId);
    CHECK_EQ(clone1->name, std::string("Hero_2"));
    CHECK_EQ(clone1->tag, std::string("Player"));
    CHECK(clone1->active);
    CHECK_NEAR(clone1->transform.x, 116.0, 0.01);
    CHECK_NEAR(clone1->transform.y, 216.0, 0.01);
    CHECK_NEAR(clone1->transform.width, 48.0, 0.01);
    CHECK_NEAR(clone1->transform.height, 64.0, 0.01);
    CHECK(clone1->sprite.enabled);
    CHECK_EQ(clone1->sprite.texturePath, std::string("examples/assets/textures/hero.png"));
    CHECK_EQ(static_cast<int>(clone1->sprite.color.r), 12);
    CHECK_EQ(clone1->sprite.zOrder, 5);
    CHECK(clone1->sprite.flipX);
    CHECK_EQ(clone1->sprite.frameCount, 4);
    CHECK_EQ(clone1->sprite.currentFrame, 2);
    CHECK_NEAR(clone1->sprite.frameTimer, 0.0, 0.001);
    CHECK(clone1->text.enabled);
    CHECK_EQ(clone1->text.content, std::string("Hero Label"));
    CHECK(clone1->collider.enabled);
    CHECK(clone1->collider.shape == Talos::Physics::ColliderShape::Circle);
    CHECK_NEAR(clone1->collider.radius, 24.0, 0.01);
    CHECK_NEAR(clone1->collider.restitution, 0.6, 0.01);
    CHECK(clone1->collider.isTrigger);
    CHECK(clone1->movement.enabled);
    CHECK_NEAR(clone1->movement.speed, 250.0, 0.01);
    CHECK(clone1->camera.enabled);
    CHECK_NEAR(clone1->camera.zoom, 1.5, 0.01);
    CHECK(!clone1->camera.runtimeInitialized);
    CHECK(clone1->audio.enabled);
    CHECK_EQ(clone1->audio.audioPath, std::string("examples/assets/audio/jump.wav"));
    CHECK(!clone1->audio.runtimeStarted);

    // Duplicating "Hero_2" -> "Hero_3" with custom offset
    Entity* clone2 = scene.DuplicateEntity(clone1Id, 24.0f, -10.0f);
    CHECK(clone2 != nullptr);
    if (!clone2) return;
    CHECK(clone2->id != origId);
    CHECK(clone2->id != clone1Id);
    CHECK_EQ(clone2->name, std::string("Hero_3"));
    CHECK_NEAR(clone2->transform.x, 140.0, 0.01);
    CHECK_NEAR(clone2->transform.y, 206.0, 0.01);

    // Duplicating original "Hero" again skips "Hero_2" and "Hero_3" -> "Hero_4"
    Entity* clone3 = scene.DuplicateEntity(origId);
    CHECK(clone3 != nullptr);
    if (!clone3) return;
    CHECK_EQ(clone3->name, std::string("Hero_4"));
    CHECK_EQ(scene.GetEntities().size(), static_cast<size_t>(4));
}

TEST(PrefabSaveLoadAndInstantiateRoundTrip) {
    // 1. BuildPrefabPath normalization
    CHECK_EQ(SceneSerializer::BuildPrefabPath("Gold Coin", "examples/prefabs"),
             std::string("examples/prefabs/gold_coin.prefab"));
    CHECK_EQ(SceneSerializer::BuildPrefabPath("  Enemy__Boss #1! ", "examples/prefabs/"),
             std::string("examples/prefabs/enemy_boss_1.prefab"));
    CHECK_EQ(SceneSerializer::BuildPrefabPath("   ", "examples/prefabs"),
             std::string("examples/prefabs/entity.prefab"));

    const std::string prefabPath = "build/test_tmp_scenes/prefabs/gold_coin.prefab";
    std::error_code ec;
    std::filesystem::remove_all("build/test_tmp_scenes/prefabs", ec);

    // 2. Configure an entity with all 7 component groups and dirty transient runtime fields
    Entity templateEntity;
    templateEntity.id = 42;
    templateEntity.name = "Gold Coin";
    templateEntity.tag = "Coin";
    templateEntity.active = true;
    templateEntity.transform = {320.0f, 180.0f, 36.0f, 36.0f};

    templateEntity.sprite.enabled = true;
    templateEntity.sprite.texturePath = "examples/assets/textures/sheet.png";
    templateEntity.sprite.color = {255, 215, 40, 255};
    templateEntity.sprite.sortingLayer = 2;
    templateEntity.sprite.zOrder = 7;
    templateEntity.sprite.sourceW = 16.0f;
    templateEntity.sprite.sourceH = 16.0f;
    templateEntity.sprite.flipX = true;
    templateEntity.sprite.frameCount = 4;
    templateEntity.sprite.currentFrame = 2;
    templateEntity.sprite.fps = 10.0f;
    templateEntity.sprite.frameTimer = 0.75f; // Transient

    templateEntity.text.enabled = true;
    templateEntity.text.content = "+100";
    templateEntity.text.fontSize = 16;
    templateEntity.text.offsetY = -20.0f;

    templateEntity.collider.enabled = true;
    templateEntity.collider.shape = Talos::Physics::ColliderShape::Circle;
    templateEntity.collider.radius = 18.0f;
    templateEntity.collider.restitution = 0.4f;
    templateEntity.collider.isSolid = false;
    templateEntity.collider.isTrigger = true;

    templateEntity.movement.enabled = true;
    templateEntity.movement.speed = 120.0f;
    templateEntity.movement.mass = 1.5f;

    templateEntity.camera.enabled = true;
    templateEntity.camera.zoom = 1.25f;
    templateEntity.camera.runtimeInitialized = true; // Transient

    templateEntity.audio.enabled = true;
    templateEntity.audio.audioPath = "examples/assets/audio/coin.wav";
    templateEntity.audio.playOnCollision = true;
    templateEntity.audio.volume = 0.85f;
    templateEntity.audio.runtimeStarted = true; // Transient

    CHECK(SceneSerializer::SavePrefab(templateEntity, prefabPath));

    // 3. LoadPrefab round-trip verification + transient reset
    Entity loadedPrefab;
    CHECK(SceneSerializer::LoadPrefab(prefabPath, loadedPrefab));
    CHECK_EQ(loadedPrefab.id, static_cast<EntityID>(0));
    CHECK_EQ(loadedPrefab.name, std::string("Gold Coin"));
    CHECK_EQ(loadedPrefab.tag, std::string("Coin"));
    CHECK(loadedPrefab.active);
    CHECK_NEAR(loadedPrefab.transform.x, 320.0, 0.01);
    CHECK_NEAR(loadedPrefab.transform.y, 180.0, 0.01);
    CHECK_NEAR(loadedPrefab.transform.width, 36.0, 0.01);
    CHECK_NEAR(loadedPrefab.transform.height, 36.0, 0.01);
    CHECK(loadedPrefab.sprite.enabled);
    CHECK_EQ(loadedPrefab.sprite.texturePath, std::string("examples/assets/textures/sheet.png"));
    CHECK_EQ(loadedPrefab.sprite.sortingLayer, 2);
    CHECK_EQ(loadedPrefab.sprite.zOrder, 7);
    CHECK(loadedPrefab.sprite.flipX);
    CHECK_EQ(loadedPrefab.sprite.frameCount, 4);
    CHECK_NEAR(loadedPrefab.sprite.frameTimer, 0.0, 0.001);
    CHECK(loadedPrefab.text.enabled);
    CHECK_EQ(loadedPrefab.text.content, std::string("+100"));
    CHECK(loadedPrefab.collider.enabled);
    CHECK(loadedPrefab.collider.shape == Talos::Physics::ColliderShape::Circle);
    CHECK_NEAR(loadedPrefab.collider.radius, 18.0, 0.01);
    CHECK(loadedPrefab.collider.isTrigger);
    CHECK(loadedPrefab.movement.enabled);
    CHECK_NEAR(loadedPrefab.movement.speed, 120.0, 0.01);
    CHECK(loadedPrefab.camera.enabled);
    CHECK_NEAR(loadedPrefab.camera.zoom, 1.25, 0.01);
    CHECK(!loadedPrefab.camera.runtimeInitialized);
    CHECK(loadedPrefab.audio.enabled);
    CHECK_EQ(loadedPrefab.audio.audioPath, std::string("examples/assets/audio/coin.wav"));
    CHECK(loadedPrefab.audio.playOnCollision);
    CHECK(!loadedPrefab.audio.runtimeStarted);

    // 4. InstantiatePrefab into a Scene (sequential names, unique IDs, override coordinates)
    Scene scene;
    Entity* inst1 = SceneSerializer::InstantiatePrefab(scene, prefabPath);
    CHECK(inst1 != nullptr);
    if (!inst1) return;
    EntityID id1 = inst1->id;
    CHECK(id1 > 0);
    CHECK_EQ(inst1->name, std::string("Gold Coin"));
    CHECK_NEAR(inst1->transform.x, 320.0, 0.01);
    CHECK_NEAR(inst1->transform.y, 180.0, 0.01);

    Entity* inst2 = SceneSerializer::InstantiatePrefab(scene, prefabPath, 500.0f, 260.0f, true);
    CHECK(inst2 != nullptr);
    if (!inst2) return;
    EntityID id2 = inst2->id;
    CHECK(id2 != id1);
    CHECK_EQ(inst2->name, std::string("Gold Coin_2"));
    CHECK_NEAR(inst2->transform.x, 500.0, 0.01);
    CHECK_NEAR(inst2->transform.y, 260.0, 0.01);

    Entity* inst3 = SceneSerializer::InstantiatePrefab(scene, prefabPath);
    CHECK(inst3 != nullptr);
    if (!inst3) return;
    CHECK(inst3->id != id1);
    CHECK(inst3->id != id2);
    CHECK_EQ(inst3->name, std::string("Gold Coin_3"));
    CHECK_EQ(scene.GetEntities().size(), static_cast<size_t>(3));

    // 5. Missing prefab file should fail cleanly without mutating the scene
    CHECK(SceneSerializer::InstantiatePrefab(scene, "build/test_tmp_scenes/prefabs/missing.prefab") == nullptr);
    CHECK_EQ(scene.GetEntities().size(), static_cast<size_t>(3));

    std::filesystem::remove_all("build/test_tmp_scenes/prefabs", ec);
}

TEST(HierarchyReordering_MoveUpDownAndToIndexPreserveEntitiesAndPersistOrder) {
    Scene scene;
    EntityID id1 = scene.CreateEntity("Background").id;
    EntityID id2 = scene.CreateEntity("Player").id;
    EntityID id3 = scene.CreateEntity("Slime Enemy").id;
    EntityID id4 = scene.CreateEntity("HUD Overlay").id;

    CHECK_EQ(scene.GetEntityIndex(id1), 0);
    CHECK_EQ(scene.GetEntityIndex(id2), 1);
    CHECK_EQ(scene.GetEntityIndex(id3), 2);
    CHECK_EQ(scene.GetEntityIndex(id4), 3);
    CHECK_EQ(scene.GetEntityIndex(999), -1);

    // Boundary no-ops: first cannot move up, last cannot move down, missing ID returns false
    CHECK(!scene.MoveEntityUp(id1));
    CHECK(!scene.MoveEntityDown(id4));
    CHECK(!scene.MoveEntityUp(999));
    CHECK(!scene.MoveEntityDown(999));
    CHECK(!scene.MoveEntityToIndex(999, 0));

    // Move Player (id2) up to index 0 -> [id2, id1, id3, id4]
    CHECK(scene.MoveEntityUp(id2));
    CHECK_EQ(scene.GetEntityIndex(id2), 0);
    CHECK_EQ(scene.GetEntityIndex(id1), 1);

    // Move Background (id1) down to index 2 -> [id2, id3, id1, id4]
    CHECK(scene.MoveEntityDown(id1));
    CHECK_EQ(scene.GetEntityIndex(id3), 1);
    CHECK_EQ(scene.GetEntityIndex(id1), 2);

    // Move HUD Overlay (id4) directly to index 1 -> [id2, id4, id3, id1]
    CHECK(scene.MoveEntityToIndex(id4, 1));
    CHECK_EQ(scene.GetEntities()[0].id, id2);
    CHECK_EQ(scene.GetEntities()[1].id, id4);
    CHECK_EQ(scene.GetEntities()[2].id, id3);
    CHECK_EQ(scene.GetEntities()[3].id, id1);

    // Move Player (id2) to an out-of-range target index (clamped to end) -> [id4, id3, id1, id2]
    CHECK(scene.MoveEntityToIndex(id2, 99));
    CHECK_EQ(scene.GetEntityIndex(id2), 3);
    CHECK_EQ(scene.GetEntityIndex(id4), 0);

    // Moving to current index is a no-op returning false
    CHECK(!scene.MoveEntityToIndex(id2, 3));

    // Save and reload scene to verify hierarchy order persists in .talsc files
    std::string path = (TempDir() / "reordered_hierarchy.talsc").string();
    CHECK(SceneSerializer::Save(scene, path));

    Scene loaded;
    CHECK(SceneSerializer::Load(loaded, path));
    CHECK_EQ(loaded.GetEntities().size(), static_cast<size_t>(4));
    CHECK_EQ(loaded.GetEntities()[0].id, id4);
    CHECK_EQ(loaded.GetEntities()[1].id, id3);
    CHECK_EQ(loaded.GetEntities()[2].id, id1);
    CHECK_EQ(loaded.GetEntities()[3].id, id2);
}

TEST(HierarchyFilter_SearchByNameOrTagAndPrefixFilters) {
    Scene scene;

    Entity& hero = scene.CreateEntity("Hero Knight");
    hero.tag = "Player";

    Entity& slime = scene.CreateEntity("Green Slime");
    slime.tag = "Enemy";

    Entity& bat = scene.CreateEntity("Cave Bat");
    bat.tag = "Enemy";

    Entity& coin = scene.CreateEntity("Gold Coin");
    coin.tag = "Coin";

    Entity& tree = scene.CreateEntity("Background Tree");
    tree.tag = ""; // Untagged

    // 1. Empty filters return all 5 entities in hierarchy order
    auto all = scene.FilterEntities("", "");
    CHECK_EQ(all.size(), static_cast<size_t>(5));
    CHECK_EQ( scene.FilterEntities("   ", "All").size(), static_cast<size_t>(5));

    // 2. Case-insensitive substring search matches either name or tag
    auto searchEnemy = scene.FilterEntities("enemy");
    CHECK_EQ(searchEnemy.size(), static_cast<size_t>(2));
    CHECK_EQ(searchEnemy[0]->name, std::string("Green Slime"));
    CHECK_EQ(searchEnemy[1]->name, std::string("Cave Bat"));

    auto searchKnight = scene.FilterEntities("KNIGHT");
    CHECK_EQ(searchKnight.size(), static_cast<size_t>(1));
    CHECK_EQ(searchKnight[0]->name, std::string("Hero Knight"));

    // 3. Explicit prefix queries: tag:<query>, #<query>, name:<query>
    auto hashCoin = scene.FilterEntities("#coin");
    CHECK_EQ(hashCoin.size(), static_cast<size_t>(1));
    CHECK_EQ(hashCoin[0]->name, std::string("Gold Coin"));

    auto tagEnemy = scene.FilterEntities("tag:ENEMY");
    CHECK_EQ(tagEnemy.size(), static_cast<size_t>(2));

    // 'name:enemy' only searches entity.name (not entity.tag), so it returns 0 for "Green Slime" / "Cave Bat"
    auto nameEnemyOnly = scene.FilterEntities("name:enemy");
    CHECK(nameEnemyOnly.empty());

    auto nameSlimeOnly = scene.FilterEntities("name:slime");
    CHECK_EQ(nameSlimeOnly.size(), static_cast<size_t>(1));
    CHECK_EQ(nameSlimeOnly[0]->name, std::string("Green Slime"));

    // 4. Tag chip filter combined with text search query
    auto enemyChipAll = scene.FilterEntities("", "Enemy");
    CHECK_EQ(enemyChipAll.size(), static_cast<size_t>(2));

    auto enemyChipBat = scene.FilterEntities("bat", "Enemy");
    CHECK_EQ(enemyChipBat.size(), static_cast<size_t>(1));
    CHECK_EQ(enemyChipBat[0]->name, std::string("Cave Bat"));

    // 5. Special "Untagged" / "None" tag filter
    auto untagged = scene.FilterEntities("", "Untagged");
    CHECK_EQ(untagged.size(), static_cast<size_t>(1));
    CHECK_EQ(untagged[0]->name, std::string("Background Tree"));
}

TEST(MultiScene_NormalizeDiscoverAndBuildUniqueScenePaths) {
    // 1. NormalizeScenePath appends .talsc when extension is missing and normalizes slashes
    CHECK_EQ(SceneSerializer::NormalizeScenePath("examples\\scenes\\arena"),
             std::string("examples/scenes/arena.talsc"));
    CHECK_EQ(SceneSerializer::NormalizeScenePath("  examples/scenes/main.talsc  "),
             std::string("examples/scenes/main.talsc"));
    CHECK_EQ(SceneSerializer::NormalizeScenePath(""), std::string(""));

    // 2. BuildUniqueScenePath & DiscoverScenes in an isolated directory
    std::filesystem::path multiDir = TempDir() / "multi_scenes";
    std::error_code ec;
    std::filesystem::remove_all(multiDir, ec);
    std::filesystem::create_directories(multiDir, ec);

    std::string dirStr = multiDir.generic_string();
    std::string p1 = SceneSerializer::BuildUniqueScenePath("Dungeon Level", dirStr);
    CHECK_EQ(p1, dirStr + "/dungeon_level.talsc");

    Scene s1;
    s1.CreateEntity("DungeonHero", 100.0f, 100.0f);
    CHECK(SceneSerializer::Save(s1, p1));

    // Since dungeon_level.talsc now exists on disk, next unique path must be dungeon_level_2.talsc
    std::string p2 = SceneSerializer::BuildUniqueScenePath("Dungeon Level", dirStr);
    CHECK_EQ(p2, dirStr + "/dungeon_level_2.talsc");
    CHECK(SceneSerializer::Save(s1, p2));

    // Create a non-scene file (.talos project and .prefab template) to verify DiscoverScenes filters only .talsc
    {
        std::ofstream dummyProject(multiDir / "project.talos");
        dummyProject << "[Project]\nname = Dummy\n";
        std::ofstream dummyPrefab(multiDir / "hero.prefab");
        dummyPrefab << "[Entity]\nname = Hero\n";
    }

    auto discovered = SceneSerializer::DiscoverScenes(dirStr);
    CHECK_EQ(discovered.size(), static_cast<size_t>(2));
    if (discovered.size() == 2) {
        CHECK_EQ(discovered[0], dirStr + "/dungeon_level.talsc");
        CHECK_EQ(discovered[1], dirStr + "/dungeon_level_2.talsc");
    }

    std::filesystem::remove_all(multiDir, ec);
}

TEST(MultiScene_ImmediateDeferredAndMidUpdateSceneSwitching) {
    std::filesystem::path switchDir = TempDir() / "runtime_switch";
    std::error_code ec;
    std::filesystem::remove_all(switchDir, ec);
    std::filesystem::create_directories(switchDir, ec);

    std::string alphaPath = (switchDir / "scene_alpha.talsc").generic_string();
    std::string betaPath  = (switchDir / "scene_beta.talsc").generic_string();

    // Prepare Scene Alpha (Overworld)
    {
        Scene alpha;
        Entity& hero = alpha.CreateEntity("OverworldPlayer", 120.0f, 180.0f);
        hero.tag = "Player";
        alpha.CreateEntity("OverworldTree", 300.0f, 220.0f);
        CHECK(SceneSerializer::Save(alpha, alphaPath));
    }

    // Prepare Scene Beta (Boss Arena with two overlapping trigger colliders for mid-Update callback test)
    {
        Scene beta;
        Entity& runner = beta.CreateEntity("ArenaPlayer", 200.0f, 200.0f);
        runner.tag = "Player";
        runner.transform.width = 48.0f;
        runner.transform.height = 48.0f;
        runner.collider.enabled = true;
        runner.movement.enabled = true;

        Entity& portal = beta.CreateEntity("ExitPortal", 200.0f, 200.0f);
        portal.tag = "Portal";
        portal.transform.width = 48.0f;
        portal.transform.height = 48.0f;
        portal.collider.enabled = true;
        portal.collider.isTrigger = true;
        portal.collider.isSolid = false;

        CHECK(SceneSerializer::Save(beta, betaPath));
    }

    Scene runtimeScene;
    int callbackCount = 0;
    std::string lastCallbackPath;
    runtimeScene.SetSceneChangeCallback([&](const std::string& loadedPath) {
        ++callbackCount;
        lastCallbackPath = loadedPath;
    });

    // 1. Immediate ChangeScene (also testing automatic .talsc extension normalization)
    std::string alphaWithoutExt = (switchDir / "scene_alpha").generic_string();
    CHECK(runtimeScene.ChangeScene(alphaWithoutExt, false));
    CHECK_EQ(runtimeScene.GetSceneFilePath(), alphaPath);
    CHECK_EQ(runtimeScene.GetEntities().size(), static_cast<size_t>(2));
    CHECK(runtimeScene.FindEntity("OverworldPlayer") != nullptr);
    CHECK_EQ(callbackCount, 1);
    CHECK_EQ(lastCallbackPath, alphaPath);

    // 2. Failed ChangeScene preserves the currently active scene and path
    CHECK(!runtimeScene.ChangeScene((switchDir / "missing_scene.talsc").generic_string(), false));
    CHECK_EQ(runtimeScene.GetSceneFilePath(), alphaPath);
    CHECK_EQ(runtimeScene.GetEntities().size(), static_cast<size_t>(2));
    CHECK_EQ(callbackCount, 1);

    // 3. ReloadCurrentScene restores modified in-memory entities from disk
    Entity* overworldPlayer = runtimeScene.FindEntity("OverworldPlayer");
    CHECK(overworldPlayer != nullptr);
    if (overworldPlayer) {
        overworldPlayer->transform.x = 999.0f;
    }
    CHECK(runtimeScene.ReloadCurrentScene(false));
    overworldPlayer = runtimeScene.FindEntity("OverworldPlayer");
    CHECK(overworldPlayer != nullptr);
    if (overworldPlayer) {
        CHECK_NEAR(overworldPlayer->transform.x, 120.0, 0.01);
    }
    CHECK_EQ(callbackCount, 2);

    // 4. Deferred RequestSceneChange queues the transition until Update() or ApplyPendingSceneChange()
    runtimeScene.RequestSceneChange(betaPath);
    CHECK(runtimeScene.HasPendingSceneChange());
    CHECK_EQ(runtimeScene.GetPendingScenePath(), betaPath);
    // Still in Scene Alpha before applying:
    CHECK(runtimeScene.FindEntity("OverworldPlayer") != nullptr);

    CHECK(runtimeScene.ApplyPendingSceneChange());
    CHECK(!runtimeScene.HasPendingSceneChange());
    CHECK_EQ(runtimeScene.GetSceneFilePath(), betaPath);
    CHECK(runtimeScene.FindEntity("ArenaPlayer") != nullptr);
    CHECK(runtimeScene.FindEntity("OverworldPlayer") == nullptr);
    CHECK_EQ(callbackCount, 3);

    // 5. Mid-Update safety: calling ChangeScene inside a Trigger Enter callback during Scene::Update()
    // must automatically defer the transition until the end of Update() and complete cleanly.
    bool triggerFiredDuringUpdate = false;
    runtimeScene.SetTriggerCallback([&](Entity&, Entity&, TriggerEventState state) {
        if (state != TriggerEventState::Enter) return;
        triggerFiredDuringUpdate = true;
        // Calling immediate ChangeScene while m_isUpdating is true automatically queues it
        runtimeScene.ChangeScene(alphaPath, false);
        CHECK(runtimeScene.HasPendingSceneChange());
        // Inside the callback (mid-Update), ArenaPlayer must still exist safely:
        CHECK(runtimeScene.FindEntity("ArenaPlayer") != nullptr);
    });

    runtimeScene.Update(1.0f / 60.0f);
    CHECK(triggerFiredDuringUpdate);
    CHECK(!runtimeScene.HasPendingSceneChange());
    CHECK_EQ(runtimeScene.GetSceneFilePath(), alphaPath);
    CHECK(runtimeScene.FindEntity("OverworldPlayer") != nullptr);
    CHECK(runtimeScene.FindEntity("ArenaPlayer") == nullptr);
    CHECK_EQ(callbackCount, 4);

    std::filesystem::remove_all(switchDir, ec);
}

TEST_MAIN()


