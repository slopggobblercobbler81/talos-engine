// Unit tests for Scene: entity lifecycle, hierarchy, filtering, physics/triggers, camera, and transitions
#include "tests/test_framework.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include <filesystem>
#include <string>
#include <vector>

using namespace Talos::Scene;
using namespace Talos::Physics;
using namespace Talos::Renderer;
namespace fs = std::filesystem;

namespace {

    fs::path TempDir() {
        fs::path dir = fs::temp_directory_path() / "talos_scene_runtime_tests";
        fs::create_directories(dir);
        return dir;
    }

}

TEST(EntityLifecycle_CreateAddDuplicateRemoveAndClear) {
    Scene scene;

    Entity& e1 = scene.CreateEntity("Hero", 10.0f, 20.0f);
    CHECK_EQ(e1.id, static_cast<EntityID>(1));
    CHECK_EQ(e1.name, std::string("Hero"));
    CHECK_NEAR(e1.transform.x, 10.0f, 1e-4f);
    CHECK_NEAR(e1.transform.y, 20.0f, 1e-4f);

    // AddEntity with explicit high ID advances internal ID counter
    Entity custom;
    custom.id = 10;
    custom.name = "Boss";
    custom.tag = "Enemy";
    scene.AddEntity(custom);

    Entity& e3 = scene.CreateEntity("Companion", 0.0f, 0.0f);
    CHECK_EQ(e3.id, static_cast<EntityID>(11));

    // DuplicateEntity generates sequential names, applies offset, and resets transient runtime flags
    Entity* heroPtr = scene.FindEntityById(1);
    CHECK(heroPtr != nullptr);
    heroPtr->tag = "Player";
    heroPtr->sprite.frameTimer = 0.5f;
    heroPtr->camera.runtimeInitialized = true;
    heroPtr->audio.runtimeStarted = true;

    Entity* clone1 = scene.DuplicateEntity(1, 32.0f, 48.0f);
    CHECK(clone1 != nullptr);
    CHECK_EQ(clone1->id, static_cast<EntityID>(12));
    CHECK_EQ(clone1->name, std::string("Hero_2"));
    CHECK_EQ(clone1->tag, std::string("Player"));
    CHECK_NEAR(clone1->transform.x, 42.0f, 1e-4f);
    CHECK_NEAR(clone1->transform.y, 68.0f, 1e-4f);
    CHECK_NEAR(clone1->sprite.frameTimer, 0.0f, 1e-4f);
    CHECK(!clone1->camera.runtimeInitialized);
    CHECK(!clone1->audio.runtimeStarted);

    Entity* clone2 = scene.DuplicateEntity(clone1->id, 10.0f, 10.0f);
    CHECK(clone2 != nullptr);
    CHECK_EQ(clone2->name, std::string("Hero_3"));

    CHECK(scene.DuplicateEntity(0) == nullptr);
    CHECK(scene.DuplicateEntity(9999) == nullptr);

    // RemoveEntity & Clear
    scene.RemoveEntity(10);
    CHECK(scene.FindEntityById(10) == nullptr);
    CHECK_EQ(scene.GetEntities().size(), static_cast<size_t>(4));

    scene.Clear();
    CHECK(scene.GetEntities().empty());
    Entity& afterClear = scene.CreateEntity("Fresh", 0.0f, 0.0f);
    CHECK_EQ(afterClear.id, static_cast<EntityID>(1));
}

TEST(HierarchyOrdering_GetIndexMoveUpDownAndMoveToIndex) {
    Scene scene;
    EntityID idA = scene.CreateEntity("A").id;
    EntityID idB = scene.CreateEntity("B").id;
    EntityID idC = scene.CreateEntity("C").id;
    EntityID idD = scene.CreateEntity("D").id;

    CHECK_EQ(scene.GetEntityIndex(idA), 0);
    CHECK_EQ(scene.GetEntityIndex(idD), 3);
    CHECK_EQ(scene.GetEntityIndex(0), -1);
    CHECK_EQ(scene.GetEntityIndex(999), -1);

    // Boundary checks
    CHECK(!scene.MoveEntityUp(idA));
    CHECK(!scene.MoveEntityDown(idD));

    // Swap up/down
    CHECK(scene.MoveEntityUp(idB));
    CHECK_EQ(scene.GetEntityIndex(idB), 0);
    CHECK_EQ(scene.GetEntityIndex(idA), 1);

    CHECK(scene.MoveEntityDown(idB));
    CHECK_EQ(scene.GetEntityIndex(idA), 0);
    CHECK_EQ(scene.GetEntityIndex(idB), 1);

    // MoveEntityToIndex (forward and backward shifts + clamped bounds)
    CHECK(scene.MoveEntityToIndex(idA, 2)); // [B, C, A, D]
    CHECK_EQ(scene.GetEntityIndex(idB), 0);
    CHECK_EQ(scene.GetEntityIndex(idC), 1);
    CHECK_EQ(scene.GetEntityIndex(idA), 2);
    CHECK_EQ(scene.GetEntityIndex(idD), 3);

    CHECK(scene.MoveEntityToIndex(idD, 0)); // [D, B, C, A]
    CHECK_EQ(scene.GetEntityIndex(idD), 0);
    CHECK_EQ(scene.GetEntityIndex(idB), 1);
    CHECK_EQ(scene.GetEntityIndex(idC), 2);
    CHECK_EQ(scene.GetEntityIndex(idA), 3);

    CHECK(!scene.MoveEntityToIndex(idD, 0)); // Already at index 0
    CHECK(scene.MoveEntityToIndex(idD, 999)); // Clamped to last index (3)
    CHECK_EQ(scene.GetEntityIndex(idD), 3);
}

TEST(LookupAndFiltering_FindByNameTagIdAndFilterQueries) {
    Scene scene;
    {
        Entity& player = scene.CreateEntity("Knight");
        player.tag = "Player";
    }
    {
        Entity& goblin1 = scene.CreateEntity("Goblin_Scout");
        goblin1.tag = "Enemy";
    }
    {
        Entity& goblin2 = scene.CreateEntity("Goblin_Archer");
        goblin2.tag = "Enemy";
    }
    {
        Entity& sleepingOrc = scene.CreateEntity("Sleeping_Orc");
        sleepingOrc.tag = "Enemy";
        sleepingOrc.active = false;
    }
    {
        Entity& prop = scene.CreateEntity("Wooden_Crate");
        prop.tag = "";
    }

    const Entity* player = scene.FindEntity("Knight");
    const Entity* goblin1 = scene.FindEntity("Goblin_Scout");
    const Entity* prop = scene.FindEntity("Wooden_Crate");
    CHECK(player != nullptr);
    CHECK(goblin1 != nullptr);
    CHECK(prop != nullptr);

    CHECK(player->HasTag("Player"));
    CHECK(!player->HasTag("Enemy"));
    CHECK(!prop->HasTag(""));

    CHECK(scene.FindEntity("NonExistent") == nullptr);

    // FindEntityByTag & FindEntitiesByTag ignore inactive entities
    CHECK(scene.FindEntityByTag("Enemy") == goblin1);
    CHECK(scene.FindEntityByTag("") == nullptr);
    auto activeEnemies = scene.FindEntitiesByTag("Enemy");
    CHECK_EQ(activeEnemies.size(), static_cast<size_t>(2));

    // FilterEntities supports substring, tag:, #, name:, and tagFilter ("Untagged", "All", specific tag)
    CHECK_EQ(scene.FilterEntities("goblin").size(), static_cast<size_t>(2));
    CHECK_EQ(scene.FilterEntities("tag:enemy").size(), static_cast<size_t>(3)); // includes inactive in hierarchy filter
    CHECK_EQ(scene.FilterEntities("#player").size(), static_cast<size_t>(1));
    CHECK_EQ(scene.FilterEntities("name:crate").size(), static_cast<size_t>(1));
    CHECK_EQ(scene.FilterEntities("", "Untagged").size(), static_cast<size_t>(1));
    CHECK_EQ(scene.FilterEntities("", "All").size(), static_cast<size_t>(5));
    CHECK_EQ(scene.FilterEntities("archer", "Enemy").size(), static_cast<size_t>(1));
    CHECK_EQ(scene.FilterEntities("archer", "Player").size(), static_cast<size_t>(0));
}

TEST(SpriteAnimation_LoopAndNonLoopFrameSteppingAndUVSync) {
    Scene scene;
    {
        Entity& looping = scene.CreateEntity("CoinSpin");
        looping.sprite.enabled = true;
        looping.sprite.frameCount = 4;
        looping.sprite.currentFrame = 0;
        looping.sprite.fps = 10.0f; // 0.1s per frame
        looping.sprite.loop = true;
        looping.sprite.isPlaying = true;
        looping.sprite.sourceW = 32.0f;
        looping.sprite.sourceH = 32.0f;
    }
    {
        Entity& oneShot = scene.CreateEntity("Explosion");
        oneShot.sprite.enabled = true;
        oneShot.sprite.frameCount = 3;
        oneShot.sprite.currentFrame = 0;
        oneShot.sprite.fps = 10.0f; // 0.1s per frame
        oneShot.sprite.loop = false;
        oneShot.sprite.isPlaying = true;
        oneShot.sprite.sourceW = 64.0f;
        oneShot.sprite.sourceH = 64.0f;
    }

    Entity* looping = scene.FindEntity("CoinSpin");
    Entity* oneShot = scene.FindEntity("Explosion");
    CHECK(looping != nullptr);
    CHECK(oneShot != nullptr);

    // Advance 2 frames (0.20s)
    scene.UpdateAnimations(0.205f);
    CHECK_EQ(looping->sprite.currentFrame, 2);
    CHECK_NEAR(looping->sprite.sourceX, 64.0f, 1e-4f);
    CHECK_EQ(oneShot->sprite.currentFrame, 2);
    CHECK_NEAR(oneShot->sprite.sourceX, 128.0f, 1e-4f);

    // Advance 2 more frames (0.20s): looping wraps to frame 0; oneShot clamps at frame 2 and stops
    scene.UpdateAnimations(0.205f);
    CHECK_EQ(looping->sprite.currentFrame, 0);
    CHECK(looping->sprite.isPlaying);
    CHECK_NEAR(looping->sprite.sourceX, 0.0f, 1e-4f);

    CHECK_EQ(oneShot->sprite.currentFrame, 2);
    CHECK(!oneShot->sprite.isPlaying);
    CHECK_NEAR(oneShot->sprite.sourceX, 128.0f, 1e-4f);
}

TEST(RuntimePhysics_GravityDampingAndSolidCollisionLifecycle) {
    Scene scene;

    // Autonomous falling body vs static floor
    EntityID crateId = 0;
    EntityID floorId = 0;
    {
        Entity& crate = scene.CreateEntity("Crate", 0.0f, 0.0f);
        crate.tag = "Dynamic";
        crate.transform.width = 40.0f;
        crate.transform.height = 40.0f;
        crate.movement.enabled = true;
        crate.movement.playerControlled = false;
        crate.movement.gravityScale = 1.0f;
        crate.movement.velocityY = 100.0f;
        crate.collider.enabled = true;
        crate.collider.isSolid = true;
        crate.collider.isStatic = false;
        crateId = crate.id;
    }
    {
        Entity& floor = scene.CreateEntity("Floor", -50.0f, 35.0f);
        floor.tag = "Ground";
        floor.transform.width = 200.0f;
        floor.transform.height = 40.0f;
        floor.collider.enabled = true;
        floor.collider.isSolid = true;
        floor.collider.isStatic = true;
        floorId = floor.id;
    }

    Entity* crate = scene.FindEntityById(crateId);
    CHECK(crate != nullptr);

    std::vector<CollisionEventState> statesSeen;
    scene.SetCollisionCallback([&](Entity&, Entity&, const CollisionEvent& ev) {
        statesSeen.push_back(ev.state);
    });

    // Frame 1: Crate overlaps Floor -> Enter event + MTV separation pushes Crate above Floor (y = -5)
    scene.Update(0.016f);
    CHECK_EQ(scene.GetCollisionEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetCollisionEvents()[0].state == CollisionEventState::Enter);
    CHECK(scene.IsColliding(crateId, floorId));
    CHECK(scene.IsColliding(crateId));
    CHECK(scene.IsCollidingWithTag(crateId, "Ground"));
    CHECK_NEAR(crate->transform.y, -5.0f, 1e-3f);

    // Frame 2: Gravity pulls Crate down into Floor again -> Stay event
    scene.Update(0.016f);
    CHECK_EQ(scene.GetCollisionEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetCollisionEvents()[0].state == CollisionEventState::Stay);

    // Frame 3: Move Crate far above Floor with zero gravity -> Exit event
    crate->movement.gravityScale = 0.0f;
    crate->movement.velocityY = 0.0f;
    crate->transform.y = -200.0f;
    scene.Update(0.016f);
    CHECK_EQ(scene.GetCollisionEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetCollisionEvents()[0].state == CollisionEventState::Exit);
    CHECK(!scene.IsColliding(crateId, floorId));

    CHECK_EQ(statesSeen.size(), static_cast<size_t>(3));
    CHECK(statesSeen[0] == CollisionEventState::Enter);
    CHECK(statesSeen[1] == CollisionEventState::Stay);
    CHECK(statesSeen[2] == CollisionEventState::Exit);

    // Verify SceneDiagnostics counters updated during Update()
    const SceneDiagnostics& diag = scene.GetDiagnostics();
    CHECK_EQ(diag.totalEntities, static_cast<size_t>(2));
    CHECK_EQ(diag.activeEntities, static_cast<size_t>(2));
    CHECK_EQ(diag.collider.activeCount, static_cast<size_t>(2));
    CHECK_EQ(diag.movement.activeCount, static_cast<size_t>(1));
}

TEST(TriggerSensorOverlaps_EnterStayExitWithoutBlockingMovement) {
    Scene scene;

    EntityID runnerId = 0;
    EntityID coinId = 0;
    {
        Entity& runner = scene.CreateEntity("Runner", 0.0f, 0.0f);
        runner.tag = "Player";
        runner.transform.width = 20.0f;
        runner.transform.height = 20.0f;
        runner.collider.enabled = true;
        runner.collider.isSolid = true;
        runnerId = runner.id;
    }
    {
        Entity& coinZone = scene.CreateEntity("CoinSensor", 5.0f, 0.0f);
        coinZone.tag = "Coin";
        coinZone.transform.width = 20.0f;
        coinZone.transform.height = 20.0f;
        coinZone.collider.enabled = true;
        coinZone.collider.isTrigger = true;
        coinId = coinZone.id;
    }

    Entity* runner = scene.FindEntityById(runnerId);
    CHECK(runner != nullptr);

    std::vector<TriggerEventState> triggerStates;
    scene.SetTriggerCallback([&](Entity& trig, Entity& other, TriggerEventState st) {
        CHECK_EQ(trig.id, coinId);
        CHECK_EQ(other.id, runnerId);
        triggerStates.push_back(st);
    });

    // Frame 1: Overlapping -> Enter (and runner position is NOT pushed away by trigger)
    scene.Update(0.016f);
    CHECK_NEAR(runner->transform.x, 0.0f, 1e-4f);
    CHECK(scene.IsTriggerOverlapping(coinId, runnerId));
    CHECK(scene.IsTriggerOverlapping(coinId));
    CHECK(scene.IsTriggerOverlappingWithTag(coinId, "Player"));
    CHECK_EQ(scene.GetTriggerEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetTriggerEvents()[0].state == TriggerEventState::Enter);

    // Frame 2: Still overlapping -> Stay
    scene.Update(0.016f);
    CHECK_EQ(scene.GetTriggerEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetTriggerEvents()[0].state == TriggerEventState::Stay);

    // Frame 3: Move runner outside trigger -> Exit
    runner->transform.x = 200.0f;
    scene.Update(0.016f);
    CHECK(!scene.IsTriggerOverlapping(coinId));
    CHECK_EQ(scene.GetTriggerEvents().size(), static_cast<size_t>(1));
    CHECK(scene.GetTriggerEvents()[0].state == TriggerEventState::Exit);

    CHECK_EQ(triggerStates.size(), static_cast<size_t>(3));
    CHECK(triggerStates[0] == TriggerEventState::Enter);
    CHECK(triggerStates[1] == TriggerEventState::Stay);
    CHECK(triggerStates[2] == TriggerEventState::Exit);
}

TEST(CameraResolution_PrimaryPriorityFixedAndTargetFollow) {
    Scene scene;
    Camera2DView view = {};
    CHECK(!scene.ResolveRuntimeCamera(0.0f, 0.0f, 800.0f, 600.0f, view));

    {
        Entity& target = scene.CreateEntity("HeroTarget", 100.0f, 200.0f);
        target.transform.width = 40.0f;
        target.transform.height = 60.0f; // Center = (120, 230)
    }
    {
        Entity& secondaryCam = scene.CreateEntity("SecondaryCam", 0.0f, 0.0f);
        secondaryCam.camera.enabled = true;
        secondaryCam.camera.isPrimary = false;
        secondaryCam.camera.followTarget = false;
    }
    {
        Entity& primaryCam = scene.CreateEntity("PrimaryCam", 0.0f, 0.0f);
        primaryCam.camera.enabled = true;
        primaryCam.camera.isPrimary = true;
        primaryCam.camera.followTarget = true;
        primaryCam.camera.smoothFollow = false;
        primaryCam.camera.targetName = "HeroTarget";
        primaryCam.camera.offsetX = 10.0f;
        primaryCam.camera.offsetY = -20.0f;
        primaryCam.camera.zoom = 10.0f; // Will be clamped to 5.0
    }

    Entity* target = scene.FindEntity("HeroTarget");
    Entity* primaryCam = scene.FindEntity("PrimaryCam");
    CHECK(target != nullptr);
    CHECK(primaryCam != nullptr);
    CHECK(scene.FindActiveCameraEntity() == primaryCam);

    CHECK(scene.ResolveRuntimeCamera(0.0f, 0.0f, 800.0f, 600.0f, view));
    CHECK_NEAR(view.targetX, 130.0f, 1e-4f); // 120 + 10
    CHECK_NEAR(view.targetY, 210.0f, 1e-4f); // 230 - 20
    CHECK_NEAR(view.offsetX, 400.0f, 1e-4f);
    CHECK_NEAR(view.offsetY, 300.0f, 1e-4f);
    CHECK_NEAR(view.zoom, 5.0f, 1e-4f);

    // Move target and run Update(): smoothFollow=true interpolates toward target
    primaryCam->camera.smoothFollow = true;
    primaryCam->camera.followSpeed = 10.0f;
    scene.Update(0.016f); // Initializes camera at (130, 210)
    CHECK(primaryCam->camera.runtimeInitialized);

    target->transform.x = 300.0f; // New desiredX = 300 + 20 + 10 = 330
    scene.Update(0.1f);
    CHECK(primaryCam->camera.currentX > 130.0f);
    CHECK(primaryCam->camera.currentX < 330.0f);
}

TEST(MultiSceneTransitions_ImmediateDeferredAndMidUpdateSceneChange) {
    fs::path dir = TempDir();
    fs::path sceneAPath = dir / "level_a.talsc";
    fs::path sceneBPath = dir / "level_b.talsc";

    {
        Scene sA;
        Entity& portal = sA.CreateEntity("PortalTrigger", 0.0f, 0.0f);
        portal.collider.enabled = true;
        portal.collider.isTrigger = true;
        Entity& player = sA.CreateEntity("PlayerInA", 0.0f, 0.0f);
        player.collider.enabled = true;
        CHECK(SceneSerializer::Save(sA, sceneAPath.string()));
    }
    {
        Scene sB;
        sB.CreateEntity("BossInB", 250.0f, 300.0f);
        CHECK(SceneSerializer::Save(sB, sceneBPath.string()));
    }

    Scene activeScene;
    std::string lastNotifiedScene;
    activeScene.SetSceneChangeCallback([&](const std::string& newPath) {
        lastNotifiedScene = newPath;
    });

    // 1. Immediate ChangeScene loads level_a.talsc
    CHECK(activeScene.ChangeScene(sceneAPath.string(), false));
    CHECK(activeScene.FindEntity("PlayerInA") != nullptr);
    CHECK(!lastNotifiedScene.empty());

    // 2. Modify scene in memory, then ReloadCurrentScene restores from disk
    activeScene.CreateEntity("TemporaryEntity");
    CHECK_EQ(activeScene.GetEntities().size(), static_cast<size_t>(3));
    CHECK(activeScene.ReloadCurrentScene(false));
    CHECK_EQ(activeScene.GetEntities().size(), static_cast<size_t>(2));

    // 3. Queue and cancel deferred transition
    activeScene.RequestSceneChange(sceneBPath.string());
    CHECK(activeScene.HasPendingSceneChange());
    activeScene.CancelPendingSceneChange();
    CHECK(!activeScene.HasPendingSceneChange());

    // 4. Calling ChangeScene inside a TriggerCallback during Update() defers safely until end of Update()
    bool midUpdateVerified = false;
    activeScene.SetTriggerCallback([&](Entity&, Entity&, TriggerEventState state) {
        if (state == TriggerEventState::Enter) {
            CHECK(activeScene.ChangeScene(sceneBPath.string(), false));
            // Must still be in Scene A mid-Update so entity references remain valid
            CHECK(activeScene.HasPendingSceneChange());
            CHECK(activeScene.FindEntity("PlayerInA") != nullptr);
            midUpdateVerified = true;
        }
    });

    activeScene.Update(0.016f);
    CHECK(midUpdateVerified);
    CHECK(!activeScene.HasPendingSceneChange());
    CHECK(activeScene.FindEntity("BossInB") != nullptr);
    CHECK(activeScene.FindEntity("PlayerInA") == nullptr);

    fs::remove_all(dir);
}

TEST_MAIN()
