#include "ElementTreeCollector.h"
#include "wk-render-element-test-lib/TestSprite.h"
#include "wk-render-element/Element.h"
#include "wk-render-element/ElementTree.h"
#include "wk-render-test/WkTest.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Wukong::Render {

TEST(ElementTest, "根节点 attached changed") {
  auto tree = ElementTree::create();
  auto root = Element::createRoot(tree);

  EXPECT_TRUE(root->isAttached());
}

TEST(ElementTest, "根节点 index 为 0") {
  auto tree = ElementTree::create();
  auto root = Element::createRoot(tree);

  EXPECT_EQ(0, root->getIndex());
}

TEST(ElementTest, "根节点 HierarchyLevel 为 0") {
  auto tree = ElementTree::create();
  auto root = Element::createRoot(tree);

  EXPECT_EQ(0, root->getHierarchyLevel());
}

TEST(ElementTest, "添加自己为自己的子节点会崩") {
  auto element = Element::create();

  ASSERT_DEATH(element->appendChild(element), ".*");
}

TEST(ElementTest, "添加父节点为自己的子节点会崩") {
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->appendChild(element2);

  ASSERT_DEATH(element2->appendChild(element1), ".*");
}

TEST(ElementTest, "添加祖先节点为自己的子节点会崩") {
  auto element1 = Element::create();
  auto element2 = Element::create();
  auto element3 = Element::create();
  element1->appendChild(element2);
  element2->appendChild(element3);

  ASSERT_DEATH(element3->appendChild(element1), ".*");
}

TEST(ElementTest, "添加至树后，所有子节点都 attached 了") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->appendChild(element2);

  tree->getRoot()->appendChild(element1);

  EXPECT_TRUE(element1->isAttached());
  EXPECT_TRUE(element2->isAttached());
}

TEST(ElementTest, "从上树移除后，所有子节点都 detached 了") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->appendChild(element2);
  tree->getRoot()->appendChild(element1);

  element1->removeFromParent();

  EXPECT_FALSE(element1->isAttached());
  EXPECT_FALSE(element2->isAttached());
}

TEST(ElementTest, "添加兄弟节点后 index 增加") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  tree->getRoot()->appendChild(element1);

  tree->getRoot()->insertChild(element2, 0);

  EXPECT_EQ(1, element1->getIndex());
}

TEST(ElementTest, "移除兄弟节点后 index 减少") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  tree->getRoot()->appendChild(element1);
  tree->getRoot()->insertChild(element2, 0);

  element2->removeFromParent();

  EXPECT_EQ(0, element1->getIndex());
}

TEST(ElementTest, "设置接近的 relative transform，什么都没发生") {
  auto tree = ElementTree::create();
  auto collector = std::make_shared<ElementTreeCollector>();
  tree->addListener(collector);
  auto element = Element::create();
  tree->getRoot()->appendChild(element);
  tree->flush();
  collector->clear();

  auto transform = SkMatrix::Translate(0.0000001, 0.0000001);
  element->setRelativeTransform(transform);
  tree->flush();

  EXPECT_EQ(0, collector->getDirtyItems().size());
  EXPECT_EQ(0, collector->getFlushBeginItems().size());
  EXPECT_EQ(0, collector->getInsertItems().size());
  EXPECT_EQ(0, collector->getRemoveItems().size());
  EXPECT_EQ(0, collector->getUpdateItems().size());
  EXPECT_EQ(0, collector->getFlushEndItems().size());
}

TEST(ElementTest, "设置 relative transform 后，world transform 有变化") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  tree->getRoot()->appendChild(element1);
  auto element2 = Element::create();
  element1->appendChild(element2);

  constexpr auto dx = 100.0f;
  constexpr auto dy = 200.0f;
  element1->setRelativeTransform(SkMatrix::Translate(dx, dy));

  EXPECT_EQ(dx, element1->getWorldTransform().getTranslateX());
  EXPECT_EQ(dy, element1->getWorldTransform().getTranslateY());
  EXPECT_EQ(dx, element2->getWorldTransform().getTranslateX());
  EXPECT_EQ(dy, element2->getWorldTransform().getTranslateY());
}

TEST(ElementTest, "设置 sprite 后，world interact bounds 有变化") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element2->setSprite(std::make_shared<TestSprite>(10, 10));
  element1->appendChild(element2);

  tree->getRoot()->appendChild(element1);

  EXPECT_EQ(SkRect::MakeWH(10, 10), element2->getWorldInteractBounds());
}

TEST(ElementTest, "设置相同的 children clip path，什么都没发生") {
  auto tree = ElementTree::create();
  auto collector = std::make_shared<ElementTreeCollector>();
  tree->addListener(collector);
  auto element = Element::create();
  tree->getRoot()->appendChild(element);
  tree->flush();
  collector->clear();

  element->setChildrenClipPath(std::nullopt);
  tree->flush();

  EXPECT_EQ(0, collector->getDirtyItems().size());
  EXPECT_EQ(0, collector->getFlushBeginItems().size());
  EXPECT_EQ(0, collector->getInsertItems().size());
  EXPECT_EQ(0, collector->getRemoveItems().size());
  EXPECT_EQ(0, collector->getUpdateItems().size());
  EXPECT_EQ(0, collector->getFlushEndItems().size());
}

TEST(ElementTest, "改变 relative transform 后，world interact bounds 有变化") {
  auto tree = ElementTree::create();
  auto element = Element::create();
  element->setSprite(std::make_shared<TestSprite>(10, 10));
  tree->getRoot()->appendChild(element);

  element->setRelativeTransform(SkMatrix::Translate(10, 10));

  EXPECT_EQ(SkRect::MakeXYWH(10, 10, 10, 10),
            element->getWorldInteractBounds());
}

TEST(ElementTest, "设置 mask 后，子节点也在 mask 中") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  element1->setMaskType(MaskType::OPAQUE);

  EXPECT_TRUE(element1->isTreeInMask());
  EXPECT_TRUE(element2->isTreeInMask());
}

TEST(ElementTest, "设置 layer blur 后，子节点也有 tree layer blur") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  element1->setLayerBlur(1);

  EXPECT_THAT(element1->getTreeLayerBlurs(), testing::ElementsAre(1));
  EXPECT_THAT(element2->getTreeLayerBlurs(), testing::ElementsAre(1));
}

TEST(ElementTest, "设置相同的 visible，没有变化") {
  auto tree = ElementTree::create();
  auto collector = std::make_shared<ElementTreeCollector>();
  tree->addListener(collector);
  auto element = Element::create();
  tree->getRoot()->appendChild(element);
  tree->flush();
  collector->clear();

  element->setVisible(true);
  tree->flush();

  EXPECT_EQ(0, collector->getDirtyItems().size());
  EXPECT_EQ(0, collector->getFlushBeginItems().size());
  EXPECT_EQ(0, collector->getInsertItems().size());
  EXPECT_EQ(0, collector->getRemoveItems().size());
  EXPECT_EQ(0, collector->getUpdateItems().size());
  EXPECT_EQ(0, collector->getFlushEndItems().size());
}

TEST(ElementTest, "设置相同的 interactive，没有变化") {
  auto tree = ElementTree::create();
  auto collector = std::make_shared<ElementTreeCollector>();
  tree->addListener(collector);
  auto element = Element::create();
  tree->getRoot()->appendChild(element);
  tree->flush();
  collector->clear();

  element->setInteractive(true);
  tree->flush();

  EXPECT_EQ(0, collector->getDirtyItems().size());
  EXPECT_EQ(0, collector->getFlushBeginItems().size());
  EXPECT_EQ(0, collector->getInsertItems().size());
  EXPECT_EQ(0, collector->getRemoveItems().size());
  EXPECT_EQ(0, collector->getUpdateItems().size());
  EXPECT_EQ(0, collector->getFlushEndItems().size());
}

TEST(ElementTest, "父节点设置 visible 为 false，后子节点不可见") {
  auto tree = ElementTree::create();
  auto element = Element::create();
  element->setSprite(std::make_shared<TestSprite>(10, 10));
  tree->getRoot()->appendChild(element);

  tree->getRoot()->setVisible(false);

  EXPECT_FALSE(element->isTreeVisible());
}

TEST(ElementTest, "父节点设置 visible 为 false，后子节点不交互") {
  auto tree = ElementTree::create();
  auto element = Element::create();
  element->setSprite(std::make_shared<TestSprite>(10, 10));
  tree->getRoot()->appendChild(element);

  tree->getRoot()->setInteractive(false);

  EXPECT_FALSE(element->isHitTestable());
  EXPECT_FALSE(element->isRectTestable());
}

TEST(ElementTest, "获取的 DropShadow 为之前设置的 DropShadow") {
  auto element = Element::create();

  element->setDropShadows({{}});

  EXPECT_EQ(element->getDropShadows(), std::vector{DropShadow{}});
}

TEST(ElementTest, "获取的 InnerShadow 为之前设置的 InnerShadow") {
  auto element = Element::create();

  element->setInnerShadows({{}});

  EXPECT_EQ(element->getInnerShadows(), std::vector{InnerShadow{}});
}

TEST(ElementTest, "获取的 LayerBlur 为之前设置的 LayerBlur") {
  auto element = Element::create();

  element->setLayerBlur(3);

  EXPECT_EQ(element->getLayerBlur(), 3);
}

TEST(ElementTest, "设置接近 0 的 LayerBlur 后为 0") {
  auto element = Element::create();

  element->setLayerBlur(0.00001);

  EXPECT_EQ(element->getLayerBlur(), 0);
}

TEST(ElementTest, "设置接近 1 的 opacity 后为 1") {
  auto element = Element::create();

  element->setOpacity(0.99999);

  EXPECT_EQ(element->getOpacity(), 1);
}

TEST(ElementTest, "获取的 BackgroundBlur 为之前设置的 BackgroundBlur") {
  auto element = Element::create();

  element->setBackgroundBlur(3);

  EXPECT_EQ(element->getBackgroundBlur(), 3);
}

TEST(ElementTest, "设置接近 0 的 BackgroundBlur 后为 0") {
  auto element = Element::create();

  element->setBackgroundBlur(0.00001);

  EXPECT_EQ(element->getBackgroundBlur(), 0);
}

TEST(ElementTest, "获取的 MaskType 为之前设置的 MaskType") {
  auto element = Element::create();

  element->setMaskType(MaskType::OPAQUE);

  EXPECT_EQ(element->getMaskType(), MaskType::OPAQUE);
}

TEST(ElementTest,
     "获取的 ShouldInterruptMask 为之前设置的 ShouldInterruptMask") {
  auto element = Element::create();

  element->setShouldInterruptMask(true);

  EXPECT_TRUE(element->shouldInterruptMask());
}

TEST(ElementTest, "设置 DropShadow 后，getSelfDropShadows 有返回值") {
  auto tree = ElementTree::create();
  auto element = Element::create();
  tree->getRoot()->appendChild(element);

  element->setDropShadows({{}});

  EXPECT_THAT(element->getSelfDropShadows(),
              testing::ElementsAre(DropShadow{}));
}

TEST(ElementTest, "设置 InnerShadow 后，getSelfInnerShadows 有返回值") {
  auto tree = ElementTree::create();
  auto element = Element::create();
  tree->getRoot()->appendChild(element);

  element->setInnerShadows({{}});

  EXPECT_THAT(element->getSelfInnerShadows(),
              testing::ElementsAre(InnerShadow{}));
}

TEST(ElementTest, "设置 BackgroundBlur 后，getSelfBackgroundBlur 有返回值") {
  auto tree = ElementTree::create();
  auto element = Element::create();
  tree->getRoot()->appendChild(element);

  element->setBackgroundBlur(1);

  EXPECT_EQ(element->getSelfBackgroundBlur(), 1);
}

TEST(ElementTest, "兄弟节点设置 MaskType 后，isSelfMasked 有返回 true") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  tree->getRoot()->appendChild(element1);
  tree->getRoot()->appendChild(element2);

  element1->setMaskType(MaskType::OPAQUE);

  EXPECT_EQ(element2->getMaskId(), element1->getId());
}

TEST(ElementTest, "设置 Sprite 后，更新 TreeContentPath") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  auto sprite = std::make_shared<TestSprite>(10, 10);
  element2->setSprite(sprite);
  element2->setRelativeTransform(SkMatrix::Translate(10, 10));
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  auto path = element1->getSimpleTreeContentPath();
  auto bounds = path->getBounds();

  EXPECT_EQ(bounds, SkRect::MakeXYWH(10, 10, 10, 10));
}

TEST(ElementTest, "设置 Children Clip Path 后，更新 WorldRenderBounds") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->setChildrenClipPath(SkPath::Rect(SkRect::MakeXYWH(0, 0, 10, 10)));
  element2->setSprite(std::make_shared<TestSprite>(5, 20));
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  auto bounds = element2->getWorldRenderBounds();

  EXPECT_EQ(bounds, SkRect::MakeXYWH(0, 0, 5, 10));
}

TEST(ElementTest, "设置 Children Clip Path 后，更新 WorldInteractBounds") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->setChildrenClipPath(SkPath::Rect(SkRect::MakeXYWH(0, 0, 10, 10)));
  element2->setSprite(std::make_shared<TestSprite>(5, 20));
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  auto bounds = element2->getWorldInteractBounds();

  EXPECT_EQ(bounds, SkRect::MakeXYWH(0, 0, 5, 10));
}

TEST(ElementTest, "设置 Children Clip Path 后，更新 WorldTreeInteractBounds") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->setChildrenClipPath(SkPath::Rect(SkRect::MakeXYWH(0, 0, 10, 10)));
  element2->setSprite(std::make_shared<TestSprite>(5, 20));
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  auto bounds = element1->getWorldTreeInteractBounds();

  EXPECT_EQ(bounds, SkRect::MakeXYWH(0, 0, 5, 10));
}

TEST(ElementTest, "设置 Sprite 后，更新 WorldRenderBounds") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->setMaskType(MaskType::OPAQUE);
  element1->setRelativeTransform(SkMatrix::Translate(10, 10));
  element1->setSprite(std::make_shared<TestSprite>(10, 10));
  element2->setRelativeTransform(SkMatrix::Translate(5, 5));
  element2->setSprite(std::make_shared<TestSprite>(10, 10));
  tree->getRoot()->appendChild(element1);
  tree->getRoot()->appendChild(element2);

  auto bounds1 = element1->getWorldRenderBounds();
  auto bounds2 = element2->getWorldRenderBounds();
  element1->setMaskType(MaskType::NONE);
  auto bounds3 = element1->getWorldRenderBounds();
  auto bounds4 = element2->getWorldRenderBounds();

  EXPECT_EQ(bounds1, SkRect::MakeXYWH(10, 10, 10, 10));
  EXPECT_EQ(bounds2, SkRect::MakeXYWH(10, 10, 5, 5));
  EXPECT_EQ(bounds3, SkRect::MakeXYWH(10, 10, 10, 10));
  EXPECT_EQ(bounds4, SkRect::MakeXYWH(5, 5, 10, 10));
}

TEST(ElementTest, "设置 DropShadow 后，getSimpleSelfDropShadowPaths 有值") {
  auto elementTree = ElementTree::create();
  auto element = Element::create();
  auto sprite = std::make_shared<TestSprite>(10, 10);
  sprite->enableFill(true);
  sprite->enableStroke(true);
  element->setSprite(sprite);
  element->setShouldUseShadowSpread(true);
  element->setDropShadows({
      DropShadow{
          .color = SK_ColorRED,
          .offset = {10, 10},
          .radius = 10,
          .spread = 10,
          .showShadowBehindNode = false,
      },
  });
  elementTree->getRoot()->appendChild(element);

  auto path = element->getSimpleSelfDropShadowPaths()->front();
  auto bounds = path.getBounds();

  EXPECT_EQ(bounds, SkRect::MakeLTRB(-10, -10, 20, 20));
}

TEST(ElementTest, "子节点为空，simpleTreeContentPath 为父节点的值") {
  auto elementTree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  element1->setSprite(std::make_shared<TestSprite>(10, 10));
  elementTree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  auto path = element1->getSimpleTreeContentPath();
  auto bounds = path->getBounds();

  EXPECT_EQ(bounds, SkRect::MakeWH(10, 10));
}

TEST(ElementTest, "有 fill 和 stroke，simpleTreeContentPath 为之和") {
  auto elementTree = ElementTree::create();
  auto element1 = Element::create();
  auto sprite = std::make_shared<TestSprite>(10, 10);
  sprite->enableFill(true);
  sprite->enableStroke(true);
  element1->setSprite(sprite);
  elementTree->getRoot()->appendChild(element1);

  auto path = element1->getSimpleTreeContentPath();
  auto bounds = path->getBounds();

  EXPECT_EQ(bounds, SkRect::MakeWH(10, 10));
}

TEST(ElementTest,
     "有 stroke，设置 DropShadow 后，getSimpleSelfDropShadowPaths 为空") {
  auto elementTree = ElementTree::create();
  auto element = Element::create();
  auto sprite = std::make_shared<TestSprite>(10, 10);
  sprite->enableStroke(true);
  element->setSprite(sprite);
  element->setDropShadows({
      DropShadow{
          .color = SK_ColorRED,
          .offset = {10, 10},
          .radius = 10,
          .spread = 10,
          .showShadowBehindNode = false,
      },
  });
  elementTree->getRoot()->appendChild(element);

  EXPECT_EQ(element->getSimpleSelfDropShadowPaths(), std::nullopt);
}

TEST(ElementTest,
     "设置 DropShadow + ChildrenClipPath，getSelfDropShadow 无 spread") {
  auto elementTree = ElementTree::create();
  auto element = Element::create();
  auto sprite = std::make_shared<TestSprite>(10, 10);
  sprite->enableStroke(true);
  element->setSprite(sprite);
  element->setShouldUseShadowSpread(false);
  element->setDropShadows({
      DropShadow{
          .color = SK_ColorRED,
          .offset = {10, 10},
          .radius = 10,
          .spread = 10,
          .showShadowBehindNode = false,
      },
  });
  elementTree->getRoot()->appendChild(element);

  auto shadow = element->getSelfDropShadows().front();

  EXPECT_EQ(shadow.spread, 0);
}

TEST(ElementTest, "设置 InnerShadow 后，getSimpleSelfInnerShadowPaths 有值") {
  auto elementTree = ElementTree::create();
  auto element = Element::create();
  auto sprite = std::make_shared<TestSprite>(10, 10);
  element->setSprite(sprite);
  element->setInnerShadows({
      InnerShadow{
          .color = SK_ColorRED,
          .offset = {10, 10},
          .radius = 10,
          .spread = 2,
      },
  });
  elementTree->getRoot()->appendChild(element);

  auto path = element->getSimpleSelfInnerShadowPaths()->front();
  auto bounds = path.getBounds();

  EXPECT_FALSE(bounds.isEmpty());
}

TEST(ElementTest,
     "有 stroke，设置 InnerShadow 后，getSimpleSelfInnerShadowPaths 为空") {
  auto elementTree = ElementTree::create();
  auto element = Element::create();
  auto sprite = std::make_shared<TestSprite>(10, 10);
  sprite->enableStroke(true);
  element->setSprite(sprite);
  element->setInnerShadows({
      InnerShadow{
          .color = SK_ColorRED,
          .offset = {10, 10},
          .radius = 10,
          .spread = 2,
      },
  });
  elementTree->getRoot()->appendChild(element);

  EXPECT_EQ(element->getSimpleSelfInnerShadowPaths(), std::nullopt);
}

TEST(ElementTest, "SelfMasked 属性变更") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  auto element3 = Element::create();
  tree->getRoot()->appendChild(element1);
  tree->getRoot()->appendChild(element2);
  tree->getRoot()->appendChild(element3);

  element1->setMaskType(MaskType::OPAQUE);

  EXPECT_EQ(element1->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element2->getMaskId(), element1->getId());
  EXPECT_EQ(element3->getMaskId(), element1->getId());

  element1->setVisible(false);

  EXPECT_EQ(element1->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element2->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element3->getMaskId(), Element::kInvalidId);

  element1->setVisible(true);

  EXPECT_EQ(element1->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element2->getMaskId(), element1->getId());
  EXPECT_EQ(element3->getMaskId(), element1->getId());

  element2->setShouldInterruptMask(true);

  EXPECT_EQ(element1->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element2->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element3->getMaskId(), Element::kInvalidId);

  element2->setVisible(false);

  EXPECT_EQ(element1->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element2->getMaskId(), Element::kInvalidId);
  EXPECT_EQ(element3->getMaskId(), Element::kInvalidId);
}

TEST(ElementTest, "对于 render bounds，clip 后应用 layer blur") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  auto element2 = Element::create();
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);

  element1->setChildrenClipPath(SkPath::Rect(SkRect::MakeWH(10, 10)));
  element2->setSprite(std::make_shared<TestSprite>(20, 20));
  element2->setLayerBlur(10);
  tree->flush();

  EXPECT_TRUE(
      element2->getWorldRenderBounds().contains(SkRect::MakeWH(11, 11)));
}

/** render evaluate time 没有用到，影响性能，暂时先关掉了
TEST(ElementTest, "sprite_render_path_evaluate_time") {
    auto tree = ElementTree::create();
    auto element1 = Element::create();
    auto sprite = std::make_shared<TestSprite>(10, 10);
    sprite->enableStroke(true);
    element1->setSprite(sprite);
    tree->getRoot()->appendChild(element1);

    auto evaluateTime = 0.f;
    evaluateTime = element1->getSpriteRenderEvaluateTime();
    EXPECT_EQ(evaluateTime, 180);

    sprite->enableStroke(false);
    element1->setSprite(sprite);

    evaluateTime = element1->getSpriteRenderEvaluateTime();
    EXPECT_EQ(evaluateTime, 60);
}

TEST(ElementTest, "drop_shadow_render_path_evaluate_time") {
    auto elementTree = ElementTree::create();
    auto element = Element::create();
    auto sprite = std::make_shared<TestSprite>(10, 10);
    sprite->enableStroke(true);
    element->setSprite(sprite);

    std::vector<DropShadow> dropShadows{DropShadow{
        .color = SK_ColorRED,
        .offset = {10, 10},
        .radius = 10,
        .spread = 10,
        .showShadowBehindNode = false,
    }};

    element->setDropShadows(dropShadows);
    elementTree->getRoot()->appendChild(element);

    auto evaluateTime = 0.f;
    evaluateTime = element->getDropShadowRenderEvaluateTime();

    EXPECT_EQ(evaluateTime, 1200);

    dropShadows.clear();

    EXPECT_EQ(evaluateTime, 1200);

    element->setDropShadows(dropShadows);

    evaluateTime = element->getDropShadowRenderEvaluateTime();

    EXPECT_EQ(evaluateTime, 0.f);
}

TEST(ElementTest, "inner_shadow_render_path_evaluate_time") {
    auto elementTree = ElementTree::create();
    auto element = Element::create();
    auto sprite = std::make_shared<TestSprite>(10, 10);
    sprite->enableStroke(true);
    element->setSprite(sprite);

    std::vector<InnerShadow> innerShadows{
        InnerShadow{.color = SK_ColorRED, .offset = {10, 10}, .radius = 10,
.spread = 10}};

    element->setInnerShadows(innerShadows);
    elementTree->getRoot()->appendChild(element);

    auto evaluateDropTime = element->getDropShadowRenderEvaluateTime();

    EXPECT_EQ(evaluateDropTime, 0);

    auto evaluateTime = 0.f;
    evaluateTime = element->getInnerShadowRenderEvaluateTime();

    EXPECT_EQ(ceil(evaluateTime), 2732);

    innerShadows.clear();

    element->setInnerShadows(innerShadows);

    evaluateTime = element->getInnerShadowRenderEvaluateTime();

    EXPECT_EQ(evaluateTime, 0.f);
}
**/

#pragma mark - Cast

class Sprite1 : public Sprite {
public:
  SPRITE_TYPE_IDS
};

class Sprite2 final : public Sprite1 {
public:
  SPRITE_TYPE_IDS_EXTEND(Sprite1)
};

TEST(ElementTest, "设置子类 Sprite 后可以获取父类 Sprite") {
  auto element = Element::create();

  element->setSprite(std::make_shared<Sprite2>());

  EXPECT_TRUE(element->getSprite<Sprite2>());
  EXPECT_TRUE(element->getSprite<Sprite1>());
}

TEST(ElementTest, "设置父类 Sprite 后无法获取子类 Sprite") {
  auto element = Element::create();

  element->setSprite(std::make_shared<Sprite1>());

  EXPECT_FALSE(element->getSprite<Sprite2>());
  EXPECT_TRUE(element->getSprite<Sprite1>());
}

#pragma mark - Interact

static void assertForEachIntersectedSubtree(
    const std::shared_ptr<Element> &element, const SkRect &rect,
    const std::vector<std::shared_ptr<Element>> &result) {
  std::vector<std::shared_ptr<Element>> actual;
  element->forEachIntersectedSubtree(
      rect, [&actual](const std::shared_ptr<Element> &element) {
        actual.push_back(element);
      });
  EXPECT_THAT(actual, testing::UnorderedElementsAreArray(result));
}

TEST(ElementTest, "forEachIntersectedSubtree 选中元素") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  element1->setSprite(std::make_shared<TestSprite>(10, 10));
  element1->setRelativeTransform(SkMatrix::Translate(10, 10));
  auto element2 = Element::create();
  element2->setSprite(std::make_shared<TestSprite>(10, 10));
  element2->setRelativeTransform(SkMatrix::Translate(10, 10));
  auto element3 = Element::create();
  element3->setSprite(std::make_shared<TestSprite>(10, 10));
  element3->setRelativeTransform(SkMatrix::Translate(10, 10));
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);
  element2->appendChild(element3);
  tree->flush();

  assertForEachIntersectedSubtree(tree->getRoot(), SkRect::MakeWH(10, 10), {});
  assertForEachIntersectedSubtree(tree->getRoot(),
                                  SkRect::MakeXYWH(5, 5, 10, 10), {element1});
  assertForEachIntersectedSubtree(tree->getRoot(),
                                  SkRect::MakeXYWH(15, 15, 10, 10), {element1});
}

TEST(ElementTest, "isSubtreeFullCovered 为子树的交互区域") {
  auto tree = ElementTree::create();
  auto element1 = Element::create();
  element1->setSprite(std::make_shared<TestSprite>(10, 10));
  element1->setRelativeTransform(SkMatrix::Translate(10, 10));
  auto element2 = Element::create();
  element2->setSprite(std::make_shared<TestSprite>(10, 10));
  element2->setRelativeTransform(SkMatrix::Translate(10, 10));
  auto element3 = Element::create();
  element3->setSprite(std::make_shared<TestSprite>(10, 10));
  element3->setRelativeTransform(SkMatrix::Translate(10, 10));
  tree->getRoot()->appendChild(element1);
  element1->appendChild(element2);
  element2->appendChild(element3);
  tree->flush();

  EXPECT_TRUE(element1->isSubtreeFullCovered(SkRect::MakeXYWH(10, 10, 30, 30)));
  EXPECT_FALSE(element1->isSubtreeFullCovered(SkRect::MakeXYWH(9, 9, 30, 30)));
  EXPECT_FALSE(
      element1->isSubtreeFullCovered(SkRect::MakeXYWH(10, 10, 29, 29)));
}

TEST(ElementTest, "clearChildren 大小为 0 的 chilren") {
  auto rootElement = Element::create();
  rootElement->clearChildren();

  EXPECT_TRUE(rootElement->getChildren().empty());
}

TEST(ElementTest, "clearChildren 大小为 1 的 chilren") {
  auto rootElement = Element::create();
  rootElement->appendChild(Element::create());
  rootElement->clearChildren();

  EXPECT_TRUE(rootElement->getChildren().empty());
}

TEST(ElementTest, "clearChildren 大小为 10 的 chilren") {
  auto rootElement = Element::create();
  for (auto i = 0; i < 10; i++) {
    rootElement->appendChild(Element::create());
  }
  rootElement->clearChildren();

  EXPECT_TRUE(rootElement->getChildren().empty());
}

} // namespace Wukong::Render
