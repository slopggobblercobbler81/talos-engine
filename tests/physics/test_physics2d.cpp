// Unit tests for Physics2D: overlap, manifolds, impulse resolution
#include "tests/test_framework.h"
#include "physics/physics2d.h"

using namespace Talos::Physics;

TEST(BoxOverlap_DetectsIntersectionAndSeparation) {
    CHECK(Physics2D::CheckOverlap(Rect2D{ 0, 0, 10, 10 }, Rect2D{ 5, 5, 10, 10 }));
    CHECK(!Physics2D::CheckOverlap(Rect2D{ 0, 0, 10, 10 }, Rect2D{ 20, 0, 10, 10 }));
}

TEST(BoxOverlap_TouchingEdgesDoNotOverlap) {
    CHECK(!Physics2D::CheckOverlap(Rect2D{ 0, 0, 10, 10 }, Rect2D{ 10, 0, 10, 10 }));
}

TEST(BoxOverlap_ZeroSizedBoxNeverOverlaps) {
    CHECK(!Physics2D::CheckOverlap(Rect2D{ 0, 0, 0, 10 }, Rect2D{ 0, 0, 10, 10 }));
    CHECK(!Physics2D::CheckOverlap(Rect2D{ 0, 0, 10, 10 }, Rect2D{ 5, 5, 10, 0 }));
}

TEST(CircleOverlap_DetectsIntersectionAndSeparation) {
    CHECK(Physics2D::CheckOverlap(Circle2D{ 0, 0, 5 }, Circle2D{ 9, 0, 5 }));
    CHECK(!Physics2D::CheckOverlap(Circle2D{ 0, 0, 5 }, Circle2D{ 10, 0, 5 }));
    CHECK(!Physics2D::CheckOverlap(Circle2D{ 0, 0, 0 }, Circle2D{ 0, 0, 5 }));
}

TEST(CircleBoxOverlap_IsSymmetric) {
    Circle2D c{ -3, 5, 5 };
    Rect2D b{ 0, 0, 10, 10 };
    CHECK(Physics2D::CheckOverlap(c, b));
    CHECK(Physics2D::CheckOverlap(b, c));
    CHECK(!Physics2D::CheckOverlap(Circle2D{ -6, 5, 5 }, b));
}

TEST(ShapeDispatch_MatchesTypedOverloads) {
    Shape2D box    = Shape2D::MakeBox(0, 0, 10, 10);
    Shape2D circle = Shape2D::MakeCircle(13, 5, 5);
    CHECK(Physics2D::CheckOverlap(box, circle));
    CHECK(Physics2D::CheckOverlap(circle, box));
    CHECK(!Physics2D::CheckOverlap(box, Shape2D::MakeBox(50, 50, 10, 10)));
}

TEST(ManifoldBoxBox_PicksAxisOfLeastPenetration) {
    CollisionManifold m = Physics2D::ComputeBoxBox(Rect2D{ 0, 0, 10, 10 }, Rect2D{ 8, 0, 10, 10 });
    CHECK(m.colliding);
    CHECK_NEAR(m.normalX, 1.0, 1e-5);
    CHECK_NEAR(m.normalY, 0.0, 1e-5);
    CHECK_NEAR(m.penetration, 2.0, 1e-5);
}

TEST(ManifoldBoxBox_NormalPointsFromAToB) {
    CollisionManifold m = Physics2D::ComputeBoxBox(Rect2D{ 8, 0, 10, 10 }, Rect2D{ 0, 0, 10, 10 });
    CHECK(m.colliding);
    CHECK_NEAR(m.normalX, -1.0, 1e-5);
}

TEST(ManifoldBoxBox_NoCollisionReturnsEmptyManifold) {
    CollisionManifold m = Physics2D::ComputeBoxBox(Rect2D{ 0, 0, 10, 10 }, Rect2D{ 50, 0, 10, 10 });
    CHECK(!m.colliding);
    CHECK_NEAR(m.penetration, 0.0, 1e-5);
}

TEST(ManifoldCircleCircle_ComputesNormalAndPenetration) {
    CollisionManifold m = Physics2D::ComputeCircleCircle(Circle2D{ 0, 0, 5 }, Circle2D{ 9, 0, 5 });
    CHECK(m.colliding);
    CHECK_NEAR(m.normalX, 1.0, 1e-5);
    CHECK_NEAR(m.penetration, 1.0, 1e-5);
}

TEST(ManifoldCircleCircle_ConcentricFallsBackToHorizontalPush) {
    CollisionManifold m = Physics2D::ComputeCircleCircle(Circle2D{ 3, 3, 5 }, Circle2D{ 3, 3, 5 });
    CHECK(m.colliding);
    CHECK_NEAR(m.normalX, 1.0, 1e-5);
    CHECK_NEAR(m.normalY, 0.0, 1e-5);
}

TEST(ManifoldCircleBox_OutsideCenter) {
    CollisionManifold m = Physics2D::ComputeCircleBox(Circle2D{ -3, 5, 5 }, Rect2D{ 0, 0, 10, 10 });
    CHECK(m.colliding);
    CHECK_NEAR(m.normalX, 1.0, 1e-5);
    CHECK_NEAR(m.normalY, 0.0, 1e-5);
    CHECK_NEAR(m.penetration, 2.0, 1e-5);
}

TEST(ManifoldCircleBox_CenterInsideEjectsAlongNearestFace) {
    CollisionManifold m = Physics2D::ComputeCircleBox(Circle2D{ 1, 5, 2 }, Rect2D{ 0, 0, 10, 10 });
    CHECK(m.colliding);
    CHECK_NEAR(m.normalX, 1.0, 1e-5);
    CHECK_NEAR(m.penetration, 3.0, 1e-5);
}

TEST(ManifoldBoxCircle_InvertsCircleBoxNormal) {
    CollisionManifold m = Physics2D::ComputeBoxCircle(Rect2D{ 0, 0, 10, 10 }, Circle2D{ 13, 5, 5 });
    CHECK(m.colliding);
    CHECK_NEAR(m.normalX, 1.0, 1e-5);
    CHECK_NEAR(m.penetration, 2.0, 1e-5);
}

TEST(Resolve_StaticBodyDoesNotMoveAndDynamicIsPushedOut) {
    PhysicsBodyState wall;   wall.isStatic = true; wall.x = 0.0f;
    PhysicsBodyState ball;   ball.x = 8.0f; ball.velocityX = -20.0f;
    CollisionManifold m;     m.colliding = true; m.normalX = 1.0f; m.penetration = 2.0f;

    Physics2D::ResolveCollision(wall, ball, m);

    CHECK_NEAR(wall.x, 0.0, 1e-5);
    CHECK_NEAR(ball.x, 10.0, 1e-5);
    CHECK_NEAR(ball.velocityX, 0.0, 1e-5); // inelastic: velocity along normal cancelled
}

TEST(Resolve_ElasticBounceReversesVelocity) {
    PhysicsBodyState wall;   wall.isStatic = true; wall.restitution = 1.0f;
    PhysicsBodyState ball;   ball.velocityX = -20.0f;
    CollisionManifold m;     m.colliding = true; m.normalX = 1.0f; m.penetration = 1.0f;

    Physics2D::ResolveCollision(wall, ball, m);

    CHECK_NEAR(ball.velocityX, 20.0, 1e-4);
}

TEST(Resolve_EqualMassHeadOnElasticCollisionSwapsVelocities) {
    PhysicsBodyState a; a.velocityX = 10.0f;  a.restitution = 1.0f;
    PhysicsBodyState b; b.velocityX = -10.0f; b.restitution = 1.0f;
    CollisionManifold m; m.colliding = true; m.normalX = 1.0f; m.penetration = 2.0f;

    Physics2D::ResolveCollision(a, b, m);

    CHECK_NEAR(a.velocityX, -10.0, 1e-4);
    CHECK_NEAR(b.velocityX, 10.0, 1e-4);
    CHECK_NEAR(a.x, -1.0, 1e-5); // positional correction split evenly
    CHECK_NEAR(b.x, 1.0, 1e-5);
}

TEST(Resolve_SeparatingBodiesReceiveNoImpulse) {
    PhysicsBodyState a; a.velocityX = -5.0f;
    PhysicsBodyState b; b.velocityX = 5.0f;
    CollisionManifold m; m.colliding = true; m.normalX = 1.0f; m.penetration = 1.0f;

    Physics2D::ResolveCollision(a, b, m);

    CHECK_NEAR(a.velocityX, -5.0, 1e-5);
    CHECK_NEAR(b.velocityX, 5.0, 1e-5);
}

TEST(Resolve_LowSpeedImpactSuppressesBounce) {
    PhysicsBodyState wall; wall.isStatic = true; wall.restitution = 1.0f;
    PhysicsBodyState ball; ball.velocityX = -5.0f;
    CollisionManifold m;   m.colliding = true; m.normalX = 1.0f; m.penetration = 1.0f;

    Physics2D::ResolveCollision(wall, ball, m);

    CHECK_NEAR(ball.velocityX, 0.0, 1e-5);
}

TEST(Resolve_TwoStaticBodiesAreIgnored) {
    PhysicsBodyState a; a.isStatic = true; a.x = 1.0f;
    PhysicsBodyState b; b.isStatic = true; b.x = 2.0f;
    CollisionManifold m; m.colliding = true; m.normalX = 1.0f; m.penetration = 1.0f;

    Physics2D::ResolveCollision(a, b, m);

    CHECK_NEAR(a.x, 1.0, 1e-5);
    CHECK_NEAR(b.x, 2.0, 1e-5);
}

TEST(Resolve_NonCollidingManifoldIsNoOp) {
    PhysicsBodyState a; a.x = 1.0f;
    PhysicsBodyState b; b.x = 2.0f;
    Physics2D::ResolveCollision(a, b, CollisionManifold{});
    CHECK_NEAR(a.x, 1.0, 1e-5);
    CHECK_NEAR(b.x, 2.0, 1e-5);
}

TEST_MAIN()
