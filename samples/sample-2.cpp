#include <wk-util/StringUtils.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "TestAppContext.hpp"
#include "catch2/catch_message.hpp"
#include "catch2/generators/catch_generators.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "matchers/Matchers.h"
#include "wk-document/command/AutoLayoutCommand.h"
#include "wk-document/command/BoundsCommand.h"
#include "wk-document/command/DeleteNodeCommand.h"
#include "wk-document/command/FlattenCommand.h"
#include "wk-document/command/GroupCommand.h"
#include "wk-document/command/SetHoveredNodeCommand.hpp"
#include "wk-document/command/TextMeasureCommand.h"
#include "wk-document/command/VectorStateCommand.h"
#include "wk-document/node/BaseFrameMixinProperty.h"
#include "wk-document/node/FrameNode.h"
#include "wk-document/node/InstanceNode.h"
#include "wk-document/node/NodeProperty.h"
#include "wk-document/node/NodeType.h"
#include "wk-document/node/RectangleNode.h"
#include "wk-document/node/TextNodeProperty.h"
#include "wk-document/node/ViewState.h"
#include "wk-document/node/view-state/AutoLayout.h"
#include "wk-editor-render/ControlElementIds.h"
#include "wk-editor-render/sprite/handle/PolygonBorderSprite.h"
#include "wk-editor-render/sprite/handle/SizeTipSprite.h"
#include "wk-editor-render/sprite/handle/SkeletonSprite.h"
#include "wk-editor-render/sprite/handle/ToolTipSprite.hpp"
#include "wk-editor-render/sprite/handle/autolayout/AutoLayoutGuideLineSprite.h"
#include "wk-editor-render/sprite/handle/autolayout/AutoLayoutGuideLineTipSprite.h"
#include "wk-editor-render/sprite/handle/autolayout/AutoLayoutInsertSegmentSprite.h"
#include "wk-editor-render/sprite/handle/autolayout/AutoLayoutRegionSprite.h"

namespace Wukong {
using Catch::Approx;

std::string formatPoint(const SkPoint &p) {
  return toFixedString(p.x(), 1) + ", " + toFixedString(p.y(), 1);
}

std::string formatChange(const SkPoint &from, const SkPoint &to,
                         const std::string &name, float change) {
  return "鼠标从 " + formatPoint(from) + " 拖动到 " + formatPoint(to) + "，" +
         name + "应该变动 " + toFixedString(change, 0) + "px";
}

SCENARIO(
    "[WK-7505] "
    "新建自动布局时，如果子图层存在同等大小的矩形，则将其属性复制到容器上") {
  auto _ = createAppContext();

  GIVEN("一大一小两个矩形，小矩形被大矩形覆盖") {
    auto r1 = _.drawRectangle(100, 100, 100, 100);
    auto r2 = _.drawRectangle(20, 20, 120, 120);

    r1.setStrokeWeight(10.0);

    WHEN("从两个矩形新建自动布局") {
      _.addAutoLayout({r1, r2});

      THEN("容器样式都与大矩形保持一致，并且以大矩形为 frame "
           "生成自动布局属性，大矩形被删除") {
        auto f = r2.getParent<FrameNode>().value();

        REQUIRE(f.getWidth() == 60);
        REQUIRE(f.getHeight() == 60);
        REQUIRE(f.getStackVerticalPadding() == 20);
        REQUIRE(f.getStrokeWeight() == 10);
        REQUIRE(f.getChildren().size() == 1);
      }
    }
  }
}

SCENARIO("[WK-8182] 包含文本节点的自动布局实例，修改间距和边距不应该报错") {
  auto _ = createAppContext();

  GIVEN("包含矩形和文本的自动布局实例") {
    auto r = _.drawRectangle(100, 100, 100, 100);
    auto t = _.createText(50, 50, 200, 200, "Test Text");

    _.addAutoLayout({r, t});

    auto f = r.getParent().value().as<FrameNode>();

    _.setSelection({f});
    _.componentSelection();

    f = _.getSelectionNodes().front().as<FrameNode>();
    REQUIRE(f.is(NodeType::Component));

    // 创建实例
    auto center = f.getBoundsInWorld().center();
    _.dispatchDraggingEventSuite(center.x(), center.y(), center.x(),
                                 center.y() + 200, {.altKey = true});

    auto selection = _.getSelectionNodes();
    REQUIRE(selection.size() == 1);

    auto fInstance = selection.front().as<FrameNode>();
    REQUIRE(fInstance.is(NodeType::Instance));

    WHEN("改变实例的间距") {
      float spacing = 11.4514f;

      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = fInstance.getId(), .stackSpacing = spacing}));

      THEN("成功改动") { REQUIRE(fInstance.getStackSpacing() == spacing); }
    }
  }
}

SCENARIO("[WK-8545] command+x 剪切自动布局图层时，可能由于 hoveredId "
         "对应的节点不存在而报错") {
  auto _ = createAppContext();

  GIVEN("自动布局容器") {
    auto f = _.drawFrame(100, 100, 100, 100);
    _.addAutoLayout({f});

    auto page = _.documentRoot->currentPage();

    WHEN("set hover 一个不存在的节点") {
      _.commandInvoker->invoke(SetHoveredNodeCommand({.nodeId = "12312"}));

      THEN("hover id 不应该更新") {
        REQUIRE_FALSE(page.getHoveredNodeId().has_value());
      }
    }
  }
}

SCENARIO(
    "[WK-6847] 自动布局多个元素冲突解决时，应将变更信息集中到一个 toast 中") {
  auto _ = createAppContext();

  GIVEN("两个相同配置的自动布局容器") {
    auto r1 = _.drawRectangle(100, 100, 100, 100);
    auto r2 = _.drawRectangle(100, 100, 200, 100);
    _.addAutoLayout({r1, r2});
    auto f1 = r1.getParent<FrameNode>().value();

    _.copyNodesToClipboard({f1});
    _.pasteFromClipboard();

    auto selection = _.getSelectionNodes();
    REQUIRE(selection.size() == 1);

    auto f2 = selection.front().as<FrameNode>();
    auto r3 = f2.getChildren().front().as<RectangleNode>();
    auto r4 = f2.getChildren().back().as<RectangleNode>();

    _.setSelection({r1, r2, r3, r4});

    WHEN("将两个容器的子元素非排列方向均设置为填充容器") {
      _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
          {.direction = StackMode::Vertical,
           .value = AutoLayoutWHValue::WH_Fill}));

      THEN("提示 f1 和 f2 被设置为固定高度") {
        auto message = _.fakeToastService->getLatestMessage();

        REQUIRE(message.has_value());
        REQUIRE(message.value() == "画板 1、画板 2 被设定为固定高度");
      }
    }
  }
}

SCENARIO("[WK-6648] 自动布局容器的边界交互区域") {
  auto _ = createAppContext();
  _.scaleViewport(1.0);

  GIVEN("自动布局容器") {
    auto f1 = _.drawFrame(100, 100, 100, 100);
    _.flushNodeEvents();

    _.dispatchClick(150, 150);
    _.flushNodeEvents();
    REQUIRE(!_.getSelection().empty());

    WHEN("拖拽元素右边界交互区域") {
      std::vector<std::tuple<SkPoint, SkPoint, SkScalar>> cases = {
          {{208, 150}, {209, 150}, 1.f},
          {{192, 150}, {193, 150}, -1.f},
          {{209, 150}, {210, 150}, 0.f},
          {{191, 150}, {190, 150}, 0.f}};

      auto i = GENERATE(0, 3);
      auto dpr = _.renderTree->getDevicePixelRatio();
      THEN("元素宽度产生对应改动") {
        auto [from, to, expectWidthChange] = cases[i];
        auto beforeWidth = f1.getWidth();
        _.dispatchSlightDraggingEventSuite(from.x(), from.y(), to.x(), to.y());
        _.flushNodeEvents();
        auto afterWidth = f1.getWidth();

        INFO(formatChange(from, to, "宽度", expectWidthChange));
        REQUIRE(afterWidth - beforeWidth == expectWidthChange);
      }
    }
  }
}

SCENARIO("[WK-6648] 自动布局容器的间距交互区域") {
  auto _ = createAppContext();
  GIVEN("自动布局容器，包含两个子元素") {
    auto r1 = _.drawRectangle(100, 100, 0, 0);
    auto r2 = _.drawRectangle(100, 100, 140, 0);
    _.addAutoLayout({r1, r2});
    auto f = r1.getParent<FrameNode>().value();

    REQUIRE(f.getStackMode() != StackMode::None);
    REQUIRE(f.getStackSpacing() == 40);
    _.setSelection({f});

    WHEN("拖拽间距交互区域（以手柄为中心的 22*22 区域）") {
      std::vector<std::tuple<SkPoint, SkPoint, SkScalar>> cases = {
          {{120, 50}, {121, 50}, 2.f},
          {{110, 39}, {111, 39}, 2.f},
          {{130, 61}, {129, 61}, -2.f},
          {{109, 40}, {109, 40}, 0.f},
          {{132, 61}, {131, 61}, 0.f}};

      auto i = GENERATE(0, 4);
      THEN("间距发生对应改动") {
        auto [from, to, expect] = cases[i];
        auto beforeSpace = f.getStackSpacing();
        _.dispatchSlightDraggingEventSuite(from.x(), from.y(), to.x(), to.y());
        auto afterSpace = f.getStackSpacing();
        INFO(formatChange(from, to, "间距", expect));

        REQUIRE(afterSpace - beforeSpace == expect);
      }
    }
  }
}

SCENARIO("[WK-6648] 自动布局容器的边距交互区域") {
  auto _ = createAppContext();

  GIVEN("自动布局容器") {
    auto f = _.drawFrame(100, 100, 0, 0);
    _.addAutoLayout({f});

    WHEN("左边距 40，拖拽左边距交互区域") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f.getId(), .stackHorizontalPadding = 40.f}));
      REQUIRE(f.getStackHorizontalPadding() == 40);
      _.setSelection({f});
      _.flushNodeEvents();

      std::vector<std::tuple<SkPoint, SkPoint, SkScalar>> cases = {
          {{20, 50}, {21, 50}, 1.f}, {{10, 50}, {11, 50}, 1.f},
          {{8, 50}, {9, 50}, 0.f},   {{30, 50}, {31, 50}, 1.f},
          {{32, 50}, {33, 50}, 0.f},
      };

      auto i = GENERATE(0, 4);
      THEN("边距发生对应改动") {
        auto [from, to, expect] = cases[i];
        auto beforeSpace = f.getStackHorizontalPadding();
        _.dispatchSlightDraggingEventSuite(from.x(), from.y(), to.x(), to.y());
        auto afterSpace = f.getStackHorizontalPadding();
        INFO(formatChange(from, to, "左边距", expect));

        REQUIRE(afterSpace - beforeSpace == expect);
      }
    }
    WHEN("拖拽元素外的左边距交互区域") {
      REQUIRE(f.getStackHorizontalPadding() == 10);
      _.setSelection({f});
      _.flushNodeEvents();

      std::vector<std::tuple<SkPoint, SkPoint, SkScalar>> cases = {
          {{5, 50}, {6, 50}, 1.f},
          {{-1, 50}, {0, 50}, 0.f},
          {{0, 50}, {1, 50}, 1.f},
      };

      auto i = GENERATE(0, 2);
      THEN("边距不应该发生变化") {
        auto [from, to, expect] = cases[i];
        auto beforeSpace = f.getStackHorizontalPadding();
        _.dispatchSlightDraggingEventSuite(from.x(), from.y(), to.x(), to.y());
        auto afterSpace = f.getStackHorizontalPadding();
        INFO(formatChange(from, to, "左边距", expect));

        REQUIRE(afterSpace - beforeSpace == expect);
      }
    }
  }
  GIVEN("两个子元素：一条直线和一个矩形") {
    auto l = _.drawLine(10, 10, 10, 80);
    auto r = _.drawRectangle(70, 80, 20, 10);
    _.addAutoLayout({l, r});

    auto f = l.getParent<FrameNode>().value();

    REQUIRE(f.getStackSpacing() == 10);
    REQUIRE(f.getStackHorizontalPadding() == 0);

    _.setSelection({f});

    WHEN("拖拽左边距和子元素重叠区域") {
      THEN("边距点击优先级大于子元素") {
        auto from = SkPoint::Make(85, 50);
        auto to = SkPoint::Make(90, 50);
        auto beforeSpace = f.getStackPaddingRight();
        _.dispatchSlightDraggingEventSuite(from.x(), from.y(), to.x(), to.y());
        auto afterSpace = f.getStackPaddingRight();
        INFO(formatChange(from, to, "右边距", 5.f));

        REQUIRE(afterSpace - beforeSpace == 5);
      }
    }

    WHEN("拖拽左边距和间距重叠交互区域") {
      THEN("边距点击优先级大于间距") {
        auto from = SkPoint::Make(15, 50);
        auto to = SkPoint::Make(16, 50);
        auto beforeSpace = f.getStackHorizontalPadding();
        _.dispatchSlightDraggingEventSuite(from.x(), from.y(), to.x(), to.y());
        auto afterSpace = f.getStackHorizontalPadding();
        INFO(formatChange(from, to, "左边距", 1.f));

        REQUIRE(afterSpace - beforeSpace == 1);
      }
    }

    WHEN("拖拽间距和子元素重叠交互区域") {
      THEN("间距优先级大于子元素") {
        auto from = SkPoint::Make(22, 50);
        auto to = SkPoint::Make(23, 50);
        auto beforeSpace = f.getStackSpacing();
        _.dispatchSlightDraggingEventSuite(from.x(), from.y(), to.x(), to.y());
        auto afterSpace = f.getStackSpacing();
        INFO(formatChange(from, to, "间距", 2.f));

        REQUIRE(afterSpace - beforeSpace == 2);
      }
    }
  }
}

SCENARIO("[WK-6648] 自动布局容器手柄区域、非手柄区域、子图层的优先级") {
  auto _ = createAppContext();
  auto page = _.documentRoot->currentPage();

  GIVEN("自动布局容器") {
    auto f = _.drawFrame(70, 120, 0, 0);
    auto l1 = _.drawLine(20, 20, 20, 100);
    auto l2 = _.drawLine(40, 20, 40, 100);
    auto r = _.drawRectangle(30, 80, 0, 0);

    _.addAutoLayout({f});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r.getId(), .stackPositioning = StackPositioning::Absolute}));
    _.commandInvoker->invoke(UpdateAutoLayoutCommand({
        .nodeId = f.getId(),
        .stackVerticalPadding = 20.f,
        .stackPaddingBottom = 20.f,
        .stackHorizontalPadding = 20.f,
        .stackPaddingRight = 20.f,
        .stackSpacing = 20.f,
    }));

    auto bounds = f.getBoundsInWorld();
    REQUIRE(f.getChildren().size() == 3);
    REQUIRE(bounds.width() == Approx(60.f));

    _.setSelection({f});

    WHEN("悬浮左边距、间距、上边距手柄区域") {
      std::vector<std::tuple<SkPoint, SkPoint>> cases = {
          {{12, 60}, {10, 60}}, {{25, 60}, {30, 60}}, {{12, 12}, {30, 10}}};

      auto i = GENERATE(0, 2);
      THEN("触发容器对应的悬浮状态") {
        auto [p, expectCenter] = cases[i];
        _.dispatchMouseMove(p.x(), p.y());

        auto region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        auto regionCenter = region->getFillGeometry().getBounds().center();

        INFO("悬浮 " + formatPoint(p) +
             " ，应该出现边距 / 间距指示框，中心为 " +
             formatPoint(expectCenter) + "，却得到了 " +
             formatPoint(regionCenter));
        REQUIRE(regionCenter.x() == Approx(expectCenter.x()));
        REQUIRE(regionCenter.y() == Approx(expectCenter.y()));
      }
    }
    WHEN("点击左边距、间距、上边距手柄区域") {
      std::vector<SkPoint> cases = {{12, 60}, {30, 60}, {30, 12}};
      auto rBounds = r.getBoundsInWorld();

      auto i = GENERATE(0, 2);
      THEN("弹出间距、边距输入框") {
        auto p = cases[i];
        _.dispatchMouseMove(p.x(), p.y());
        _.dispatchClick(p.x(), p.y());
        INFO("点击 " + formatPoint(p) + " ，应该出现输入框");
        REQUIRE(page.getAutoLayoutSpaceInput() != FloatSpaceInputType::None);
      }
    }
    WHEN("点击左边距、间距、上边距非手柄区域") {
      std::vector<SkPoint> cases = {{12, 12}, {10, 75}, {25, 75}};

      auto i = GENERATE(0, 2);
      THEN("选中子元素") {
        auto p = cases[i];
        _.dispatchClick(p.x(), p.y());

        REQUIRE(_.getSelection().size() == 1);
        REQUIRE(_.getSelection().front() == r.getId());
      }
    }
  }
}

SCENARIO("AutoLayout 属性") {
  auto _ = createAppContext();

  GIVEN("一个矩形节点") {
    auto rect = _.drawRectangle(100, 100, 100, 100);

    WHEN("打开文档") {
      THEN("能获取到「子图层」的 AutoLayout 属性") {
        REQUIRE(rect.getStackChildPrimaryGrow() == 0);
        REQUIRE(rect.getStackChildAlignSelf() == StackCounterAlign::Auto);
        REQUIRE(rect.getStackPositioning() == StackPositioning::Auto);
      }
    }

    WHEN("设置矩形的 AutoLayout 属性") {
      rect.setStackChildPrimaryGrow(1);
      rect.setStackChildAlignSelf(StackCounterAlign::Stretch);
      rect.setStackPositioning(StackPositioning::Absolute);
      _.flushNodeEvents();

      THEN("能获取到修改后的 AutoLayout 属性") {
        REQUIRE(rect.getStackChildPrimaryGrow() == 1);
        REQUIRE(rect.getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE(rect.getStackPositioning() == StackPositioning::Absolute);
      }
    }

    WHEN("设置鼠标悬浮的宽高类型值") {
      auto setValue = AutoLayoutHoverMenuItem::HeightFillContainer;
      _.commandInvoker->invoke(UpdateAutoLayoutHoverItem(
          {.value = AutoLayoutHoverMenuItem::HeightFillContainer}));

      THEN("能够正常更新") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutHoverMenuItem() ==
                setValue);
      }
    }
  }

  GIVEN("设置宽高类型") {
    auto frame = _.drawFrame(100, 100, 100, 100);
    auto r1 = _.drawRectangle(10, 10, 110, 110);
    auto f1 = _.drawFrame(10, 10, 140, 140);

    _.addAutoLayout({frame});
    _.addAutoLayout({f1});

    WHEN("单选自动布局子元素") {
      _.setSelection({r1});
      _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
          {.direction = StackMode::Horizontal,
           .value = AutoLayoutWHValue::WH_Fill}));

      THEN("能够正常设置") {
        REQUIRE(r1.getStackChildAlignSelf() == StackCounterAlign::Stretch);
      }
    }

    WHEN("单选自动布局容器") {
      _.setSelection({frame});
      _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
          {.direction = StackMode::Horizontal,
           .value = AutoLayoutWHValue::WH_Hug}));

      THEN("能够正常设置") {
        REQUIRE(frame.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
      }
    }

    WHEN("多选自动布局容器和子元素，设置为固定尺寸") {
      _.setSelection({r1});
      _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
          {.direction = StackMode::Horizontal,
           .value = AutoLayoutWHValue::WH_Fill}));

      _.setSelection({f1});
      _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
          {.direction = StackMode::Horizontal,
           .value = AutoLayoutWHValue::WH_Hug}));

      _.setSelection({f1, r1});
      _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
          {.direction = StackMode::Horizontal,
           .value = AutoLayoutWHValue::WH_Fixed}));

      THEN("能够正常设置") {
        REQUIRE(r1.getStackChildAlignSelf() == StackCounterAlign::Auto);
        REQUIRE(f1.getStackCounterSizing() == StackSize::StackSizeFixed);
      }
    }

    WHEN("设置即是自动布局容器，又是自动布局子元素的宽高类型") {
      auto f2 = _.drawFrame(100, 100, 200, 200);
      auto f3 = _.drawFrame(10, 10, 210, 210);

      _.addAutoLayout({f2});
      _.addAutoLayout({f3});

      REQUIRE(f3.getParentHandle() == f2.getHandle());

      // 容器纵向排列
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f3.getId(), .stackMode = StackMode::Vertical}));

      // 将子节点设置为固定宽高
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f3.getId(),
           .stackMode = StackMode::Horizontal,
           .stackPrimarySizing = StackSize::StackSizeFixed,
           .stackCounterSizing = StackSize::StackSizeFixed}));
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = f3.getId(),
           .stackChildPrimaryGrow = 0.0,
           .stackChildAlignSelf = StackCounterAlign::Auto}));
      _.setSelection({f3});

      WHEN("将 f3 设置为宽度填充，高度包围") {
        _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
            {.direction = StackMode::Horizontal,
             .value = AutoLayoutWHValue::WH_Fill}));
        _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
            {.direction = StackMode::Vertical,
             .value = AutoLayoutWHValue::WH_Hug}));

        THEN("设置成功") {
          auto state = _.getAutoLayoutViewState();

          REQUIRE(state.wh.widthType == AutoLayoutWHValue::WH_Fill);
          REQUIRE(state.wh.heightType == AutoLayoutWHValue::WH_Hug);
        }
      }
    }

    WHEN("设置文本节点自动布局子元素的属性") {
      auto f = _.drawFrame(100, 100, 200, 200);

      auto t =
          _.createText(50, 50, 0, 0, "f2 child text", TextAutoResize::NONE, 14);
      f.appendChild(t);

      _.addAutoLayout({f});
      _.setSelection({t});

      THEN("正确修改宽度类型") {
        std::vector<
            std::tuple<TextAutoResize, AutoLayoutWHValue, TextAutoResize>>
            cases = {
                {TextAutoResize::HEIGHT, AutoLayoutWHValue::WH_Hug,
                 TextAutoResize::WIDTH_AND_HEIGHT},
                {TextAutoResize::NONE, AutoLayoutWHValue::WH_Hug,
                 TextAutoResize::WIDTH_AND_HEIGHT},
                {TextAutoResize::WIDTH_AND_HEIGHT, AutoLayoutWHValue::WH_Fixed,
                 TextAutoResize::HEIGHT},
                {TextAutoResize::WIDTH_AND_HEIGHT, AutoLayoutWHValue::WH_Fill,
                 TextAutoResize::HEIGHT},
            };
        int count = 0;

        for (auto [autoResize, whValue, expectAutoResize] : cases) {
          t.setTextAutoResize(autoResize);
          auto state = _.getAutoLayoutViewState();
          _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
              {.direction = StackMode::Horizontal, .value = whValue}));

          INFO("Failed at Case #" << count++);
          REQUIRE(t.getTextAutoResize() == expectAutoResize);
        }
      }

      THEN("正确修改高度类型") {
        std::vector<
            std::tuple<TextAutoResize, AutoLayoutWHValue, TextAutoResize>>
            cases = {
                {TextAutoResize::NONE, AutoLayoutWHValue::WH_Hug,
                 TextAutoResize::HEIGHT},
                {TextAutoResize::WIDTH_AND_HEIGHT, AutoLayoutWHValue::WH_Fixed,
                 TextAutoResize::NONE},
                {TextAutoResize::HEIGHT, AutoLayoutWHValue::WH_Fill,
                 TextAutoResize::NONE},
            };
        int count = 0;

        for (auto [autoResize, whValue, expectAutoResize] : cases) {
          t.setTextAutoResize(autoResize);
          auto state = _.getAutoLayoutViewState();
          _.commandInvoker->invoke(UpdateSelectedAutoLayoutWHValue(
              {.direction = StackMode::Vertical, .value = whValue}));

          INFO("Failed at Case #" << count++);
          REQUIRE(t.getTextAutoResize() == expectAutoResize);
        }
      }
    }
  }

  GIVEN("一个 Frame 节点") {
    auto frame = _.drawFrame(100, 100, 100, 100);

    WHEN("打开文档") {
      THEN("能获取到「容器」和「子图层」的 AutoLayout 属性") {
        REQUIRE(frame.getStackMode() == StackMode::None);
        REQUIRE(frame.getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMin);
        REQUIRE(frame.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(frame.getStackCounterAlignItems() == StackAlign::StackAlignMin);
        REQUIRE(frame.getStackCounterSizing() == StackSize::StackSizeFixed);
        REQUIRE(frame.getStackVerticalPadding() == 0);
        REQUIRE(frame.getStackPaddingBottom() == 0);
        REQUIRE(frame.getStackHorizontalPadding() == 0);
        REQUIRE(frame.getStackPaddingRight() == 0);
        REQUIRE(frame.getStackSpacing() == 0);
        REQUIRE(frame.getStackReverseZIndex() == false);
        REQUIRE(frame.getStackChildPrimaryGrow() == 0);
        REQUIRE(frame.getStackChildAlignSelf() == StackCounterAlign::Auto);
        REQUIRE(frame.getStackPositioning() == StackPositioning::Auto);
      }
    }

    WHEN("调整 frame 的 AutoLayout 属性") {
      frame.setStackMode(StackMode::Horizontal);
      frame.setStackPrimaryAlignItems(StackJustify::StackJustifyCenter);
      frame.setStackPrimarySizing(
          StackSize::StackSizeResizeToFitWithImplicitSize);
      frame.setStackCounterAlignItems(StackAlign::StackAlignBaseLine);
      frame.setStackCounterSizing(
          StackSize::StackSizeResizeToFitWithImplicitSize);
      frame.setStackVerticalPadding(30);
      frame.setStackPaddingBottom(40);
      frame.setStackHorizontalPadding(50);
      frame.setStackPaddingRight(60);
      frame.setStackSpacing(70);
      frame.setStackReverseZIndex(true);
      _.flushNodeEvents();

      THEN("能获取到修改后的 AutoLayout 属性") {
        REQUIRE(frame.getStackMode() == StackMode::Horizontal);
        REQUIRE(frame.getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyCenter);
        REQUIRE(frame.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(frame.getStackCounterAlignItems() ==
                StackAlign::StackAlignBaseLine);
        REQUIRE(frame.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(frame.getStackVerticalPadding() == 30);
        REQUIRE(frame.getStackPaddingBottom() == 40);
        REQUIRE(frame.getStackHorizontalPadding() == 50);
        REQUIRE(frame.getStackPaddingRight() == 60);
        REQUIRE(frame.getStackSpacing() == 70);
        REQUIRE(frame.getStackReverseZIndex() == true);
      }
    }

    WHEN("修改对齐到文本基线的 frame 的方向为纵向") {
      frame.setStackCounterAlignItems(StackAlign::StackAlignBaseLine);
      frame.setStackMode(StackMode::Horizontal);

      THEN("自动取消文本基线对齐") {
        _.commandInvoker->invoke(UpdateAutoLayoutCommand(
            {.nodeId = frame.getId(), .stackMode = StackMode::Vertical}));
        REQUIRE(frame.getStackCounterAlignItems() ==
                StackAlign::StackAlignCenter);
      }
    }

    WHEN("设置 frame 的 AutoLayout 属性，然后复制 frame 为 frame2") {
      frame.setStackMode(StackMode::Vertical);
      frame.setStackPrimaryAlignItems(StackJustify::StackJustifyMax);
      frame.setStackPrimarySizing(
          StackSize::StackSizeResizeToFitWithImplicitSize);
      frame.setStackCounterAlignItems(StackAlign::StackAlignCenter);
      frame.setStackCounterSizing(
          StackSize::StackSizeResizeToFitWithImplicitSize);
      frame.setStackVerticalPadding(10);
      frame.setStackPaddingBottom(20);
      frame.setStackHorizontalPadding(30);
      frame.setStackPaddingRight(40);
      frame.setStackSpacing(50);
      frame.setStackReverseZIndex(true);
      frame.setStackChildPrimaryGrow(1);
      frame.setStackChildAlignSelf(StackCounterAlign::Stretch);
      frame.setStackPositioning(StackPositioning::Auto);

      _.dispatchDraggingEventSuite(150, 150, 250, 250, {.altKey = true});

      THEN("AutoLayout 属性被复制") {
        let &selection = _.getSelection();
        let frame2 =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(selection.size() == 1);
        REQUIRE(frame2->getId() != frame.getId());
        REQUIRE(frame2->getStackMode() == StackMode::Vertical);
        REQUIRE(frame2->getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMax);
        REQUIRE(frame2->getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(frame2->getStackCounterAlignItems() ==
                StackAlign::StackAlignCenter);
        REQUIRE(frame2->getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(frame2->getStackVerticalPadding() == 10);
        REQUIRE(frame2->getStackPaddingBottom() == 20);
        REQUIRE(frame2->getStackHorizontalPadding() == 30);
        REQUIRE(frame2->getStackPaddingRight() == 40);
        REQUIRE(frame2->getStackSpacing() == 50);
        REQUIRE(frame2->getStackReverseZIndex() == true);
        REQUIRE(frame2->getStackChildPrimaryGrow() == 1);
        REQUIRE(frame2->getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE(frame2->getStackPositioning() == StackPositioning::Auto);
      }
    }
  }
}

SCENARIO("AutoLayout 的展示") {
  auto _ = createAppContext();

  GIVEN("一个 Frame 节点包含两个 Rectangle，添加默认的 AutoLayout") {
    auto f1 = _.drawFrame(400, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 60, 50);
    auto rect2 = _.drawRectangle(100, 100, 240, 50);
    _.setSelection({f1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("选中 frame") {
      _.setSelection({f1});
      THEN("显示 bounds 的 sizeTip 为 '包围内容 x 包围内容'") {
        let sprite = _.getElSpriteById<SizeTipSprite>(ElId::BoundsSizetip);
        REQUIRE(sprite->getDisplayWidth() == "包围内容");
        REQUIRE(sprite->getDisplayHeight() == "包围内容");
      }
    }

    WHEN("[未选中悬浮] 鼠标悬浮在 frame 内空白处") {
      _.setSelection({});
      _.dispatchMouseMove(200, 100);

      THEN("展示 frame 边界和 rect1 和 rect2 的虚线边界") {
        let frameSkeleton =
            _.getElSpriteById<SkeletonSprite>(ElId::HoverSkeleton);
        let rect1Skeleton = _.getElSpriteById<PolygonBorderSprite>(
            (std::string(ElId::Skeleton) + "-" + rect1.getId()).c_str());
        let rect2Skeleton = _.getElSpriteById<PolygonBorderSprite>(
            (std::string(ElId::Skeleton) + "-" + rect2.getId()).c_str());
        REQUIRE(frameSkeleton != nullptr);
        REQUIRE(rect1Skeleton != nullptr);
        REQUIRE(rect2Skeleton != nullptr);
        REQUIRE(rect1Skeleton->getDashed());
        REQUIRE(rect2Skeleton->getDashed());
      }
    }

    WHEN("[未选中悬浮] 将 rect2 设置为隐藏，鼠标悬浮在 frame 内空白处") {
      rect2.setVisible(false);
      _.setSelection({});
      _.dispatchMouseMove(20, 100);

      THEN("展示 rect1 的虚线边界，不展示 rect2 的虚线边界") {
        let renderItem1 = _.renderTree->getRenderItemById(
            std::string(ElId::Skeleton) + "-" + rect1.getId());
        let renderItem2 = _.renderTree->getRenderItemById(
            std::string(ElId::Skeleton) + "-" + rect2.getId());
        REQUIRE(renderItem1 != nullptr);
        REQUIRE(renderItem2 == nullptr);
      }
    }

    WHEN("[选中未悬浮] 选中 frame") {
      _.setSelection({f1});

      THEN("不展示 rect1 和 rect2 的虚线边界") {
        let renderItem1 = _.renderTree->getRenderItemById(
            std::string(ElId::Skeleton) + "-" + rect1.getId());
        let renderItem2 = _.renderTree->getRenderItemById(
            std::string(ElId::Skeleton) + "-" + rect2.getId());
        REQUIRE(renderItem1 == nullptr);
        REQUIRE(renderItem2 == nullptr);
      }
    }

    WHEN("实际渲染区域够大，选中 frame，鼠标悬浮内部非间距、边距区域") {
      _.setSelection({f1});
      _.dispatchMouseMove(100, 100);

      THEN("显示边界和间距手柄") {
        let topPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingTop);
        let bottomPaddingHandle = _.renderTree->getRenderItemById(
            ElId::AutoLayoutHandlePaddingBottom);
        let leftPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingLeft);
        let rightPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingRight);
        let spacingBetweenHandle = _.renderTree->getRenderItemById(
            ElId::AutoLayoutHandleSpacingBetween);

        REQUIRE(topPaddingHandle != nullptr);
        REQUIRE(bottomPaddingHandle != nullptr);
        REQUIRE(leftPaddingHandle != nullptr);
        REQUIRE(rightPaddingHandle != nullptr);
        REQUIRE(spacingBetweenHandle != nullptr);
      }
    }

    WHEN("实际渲染区域略小，选中 frame，鼠标悬浮内部非间距、边距区域") {
      _.setSelection({f1});
      auto viewport = _.documentRoot->currentPage().getViewport();
      viewport.zoom = 0.25f;
      _.documentRoot->currentPage().setViewport(viewport);
      _.dispatchMouseMove(100, 100);

      THEN("显示边距手柄，间距手柄不可交互") {
        let topPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingTop);
        let bottomPaddingHandle = _.renderTree->getRenderItemById(
            ElId::AutoLayoutHandlePaddingBottom);
        let leftPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingLeft);
        let rightPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingRight);
        let spacingBetweenHandle = _.renderTree->getRenderItemById(
            ElId::AutoLayoutHandleDegenerateSpacingBetween);

        REQUIRE(topPaddingHandle != nullptr);
        REQUIRE(bottomPaddingHandle != nullptr);
        REQUIRE(leftPaddingHandle != nullptr);
        REQUIRE(rightPaddingHandle != nullptr);
        REQUIRE(spacingBetweenHandle != nullptr);
        REQUIRE(spacingBetweenHandle->isInteractive() == false);
      }
    }

    WHEN("实际渲染区域小，选中 frame，鼠标悬浮内部非间距、边距区域") {
      _.setSelection({f1});
      auto viewport = _.documentRoot->currentPage().getViewport();
      viewport.zoom = 0.1f;
      _.documentRoot->currentPage().setViewport(viewport);
      _.dispatchMouseMove(100, 100);

      THEN("不显示边界和间距手柄") {
        let topPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingTop);
        let bottomPaddingHandle = _.renderTree->getRenderItemById(
            ElId::AutoLayoutHandlePaddingBottom);
        let leftPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingLeft);
        let rightPaddingHandle =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandlePaddingRight);
        let spacingBetweenHandle = _.renderTree->getRenderItemById(
            ElId::AutoLayoutHandleDegenerateSpacingBetween);

        REQUIRE((!topPaddingHandle || !topPaddingHandle->isAttached()));
        REQUIRE((!bottomPaddingHandle || !bottomPaddingHandle->isAttached()));
        REQUIRE((!leftPaddingHandle || !leftPaddingHandle->isAttached()));
        REQUIRE((!rightPaddingHandle || !rightPaddingHandle->isAttached()));
        REQUIRE((!spacingBetweenHandle || !spacingBetweenHandle->isAttached()));
      }
    }

    WHEN("[未选中悬浮] 鼠标悬浮在 rect1") {
      _.dispatchMouseMove(100, 100);

      THEN("显示 rect1 的 skeleton") {
        let skeleton = _.getElSpriteById<SkeletonSprite>(ElId::HoverSkeleton);
        REQUIRE(skeleton != nullptr);
      }
    }

    WHEN("[选中子图层] 选中 rect1，鼠标悬浮在 rect1") {
      _.setSelection({rect1});
      _.dispatchMouseMove(100, 100);

      THEN("nodeTargetSkeletonContainer 渲染在 boundsContainer 之前") {
        let items = _.renderTree->getRenderItems();
        bool nodeTargetSkeletonContainerExist = false;
        for (let &item : items) {
          if (item->getRelatedId() == "nodeTargetSkeletonContainer") {
            nodeTargetSkeletonContainerExist = true;
          }
          if (item->getRelatedId() == "boundsContainer") {
            REQUIRE(nodeTargetSkeletonContainerExist);
            return;
          }
        }
      }

      THEN("展示 frame 的虚线边界") {
        let frameSkeleton = _.getElSpriteById<PolygonBorderSprite>(
            (std::string(ElId::TargetSkeleton) + "-" + f1.getId()).c_str());
        REQUIRE(frameSkeleton != nullptr);
        REQUIRE(frameSkeleton->getDashed());
      }
    }

    WHEN("[选中子图层 - 悬浮容器] 选中 rect1，鼠标悬浮在 (200, 100)") {
      _.setSelection({rect1});
      _.dispatchMouseMove(200, 100);

      THEN("展示 frame 的实心边界和 rect2 的虚线边界") {
        let frameSkeleton =
            _.getElSpriteById<SkeletonSprite>(ElId::HoverSkeleton);
        let rect2Skeleton = _.getElSpriteById<PolygonBorderSprite>(
            (std::string(ElId::Skeleton) + "-" + rect2.getId()).c_str());

        REQUIRE(frameSkeleton != nullptr);
        REQUIRE(rect2Skeleton != nullptr);
        REQUIRE(rect2Skeleton->getDashed());
      }
    }

    WHEN("[选中子图层 - 悬浮其它图层] 选中 rect1，鼠标悬浮在 (300, 100)") {
      _.setSelection({rect1});
      _.dispatchMouseMove(300, 100);

      THEN("展示 frame 的虚线边界和 rect2 的虚线边界") {
        let frameSkeleton =
            _.getElSpriteById<SkeletonSprite>(ElId::HoverSkeleton);
        let rect2Skeleton = _.getElSpriteById<PolygonBorderSprite>(
            (std::string(ElId::Skeleton) + "-" + rect2.getId()).c_str());

        REQUIRE(frameSkeleton != nullptr);
        REQUIRE(rect2Skeleton != nullptr);
        REQUIRE(rect2Skeleton->getDashed());
      }
    }

    WHEN("设置 rect1 的宽、高为 填充容器、填充容器，选中 rect1") {
      rect1.setStackChildPrimaryGrow(1);
      rect1.setStackChildAlignSelf(StackCounterAlign::Stretch);
      _.setSelection({rect1});
      _.flushNodeEvents();

      THEN("展示 rect1 bounds 的 sizeTip 为 '填充容器 x 填充容器'") {
        let sprite = _.getElSpriteById<SizeTipSprite>(ElId::BoundsSizetip);
        REQUIRE(sprite->getDisplayWidth() == "填充容器");
        REQUIRE(sprite->getDisplayHeight() == "填充容器");
      }
    }

    WHEN("[调整子图层的顺序] 鼠标拖动 rect1") {
      _.setSelection({rect1});
      _.dispatchHalfDraggingEventSuite(100, 100, 150, 150);

      THEN("展示拖动的虚线边框") {
        let skeleton = _.getElSpriteById<PolygonBorderSprite>(
            (std::string(ElId::Skeleton) + "-" + rect1.getId()).c_str());
        REQUIRE(skeleton != nullptr);
      }
      _.dispatchMouseUp(150, 150);
    }

    WHEN("[将子图层移入容器] 创建 rect3，将 rect3 拖拽到 frame 中间") {
      auto rect3 = _.drawRectangle(100, 100, 410, 50);
      _.dispatchHalfDraggingEventSuite(450, 100, 200, 100);

      THEN("展示拖动的虚线边框和蓝色横线") {
        let skeleton = _.getElSpriteById<PolygonBorderSprite>(
            (std::string(ElId::Skeleton) + "-" + rect3.getId()).c_str());
        let segment = _.getElSpriteById<AutoLayoutInsertSegmentSprite>(
            ElId::AutoLayoutGuideInsertSegment);
        REQUIRE(skeleton != nullptr);
        REQUIRE(segment != nullptr);
      }
      _.dispatchMouseUp(200, 100);
    }

    WHEN("[间距相关操作] 选中 frame，悬浮在间距") {
      _.setSelection({f1});
      _.dispatchMouseMove(170, 100);

      THEN("展示间距区域") {
        let region = _.renderTree->getRenderItemById(ElId::AutoLayoutRegion);
        REQUIRE(region != nullptr);
      }
    }

    WHEN("[间距相关操作] 选中 frame，鼠标在间距按下") {
      _.setSelection({f1});
      _.dispatchMouseDown(170, 100);

      THEN("展示间距区域") {
        let region = _.renderTree->getRenderItemById(ElId::AutoLayoutRegion);
        REQUIRE(region != nullptr);
      }
      _.dispatchMouseUp(170, 100);
    }

    WHEN("[间距相关操作] 选中容器，并悬浮间距手柄") {
      _.setSelection({f1});
      _.dispatchMouseMove(200, 100);

      THEN("展示间距区域和数值的 tooltip") {
        let region = _.renderTree->getRenderItemById(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(region != nullptr);
        REQUIRE(toolTip != nullptr);
      }
    }

    WHEN("[间距相关操作] 选中容器，操作间距手柄") {
      _.setSelection({f1});
      _.dispatchHalfDraggingEventSuite(200, 100, 220, 100);

      THEN("展示间距数值的 tooltip 和 间距边框") {
        let region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(toolTip != nullptr);
        REQUIRE(!region->getBorderGeometry().isEmpty());
      }
      _.dispatchMouseUp(220, 100);
    }

    WHEN("[边距相关操作] 选中容器，并悬浮边距") {
      _.setSelection({f1});
      _.dispatchMouseMove(20, 20);

      THEN("展示边距的区域") {
        let region = _.renderTree->getRenderItemById(ElId::AutoLayoutRegion);
        REQUIRE(region != nullptr);
      }
    }

    WHEN("[边距相关操作] 选中容器，并悬浮边距手柄") {
      _.setSelection({f1});
      _.dispatchMouseMove(200, 25);

      THEN("展示边距的区域和数值的 tooltip") {
        let region = _.renderTree->getRenderItemById(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(region != nullptr);
        REQUIRE(toolTip != nullptr);
        REQUIRE(toolTip->getSprite<ToolTipSprite>()->getText() == "50");
      }
    }

    WHEN("[边距相关操作] 选中容器，操作边距手柄") {
      _.setSelection({f1});
      _.dispatchHalfDraggingEventSuite(200, 25, 200, 35);

      THEN("展示边距数值的 tooltip 和 边距边框") {
        let region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(!region->getBorderGeometry().isEmpty());
        REQUIRE(toolTip != nullptr);
        REQUIRE(toolTip->getSprite<ToolTipSprite>()->getText() == "60");
      }
      _.dispatchMouseUp(200, 35);
    }

    WHEN("[边距相关操作] 选中容器，并按住 option，悬浮边距手柄") {
      _.setSelection({f1});
      _.dispatchMouseMove(200, 25, {.altKey = true});

      THEN("展示上下边距的区域和上边距的数值的 tooltip") {
        let region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(region->getFillGeometry().countPoints() == 10);
        REQUIRE(toolTip != nullptr);
      }
    }

    WHEN("[边距相关操作] 选中容器，并按住 option 和 shift，悬浮边距手柄") {
      _.setSelection({f1});
      _.dispatchMouseMove(200, 25, {.shiftKey = true, .altKey = true});

      THEN("展示四边距的区域和上边距的数值的 tooltip") {
        let region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(region->getFillGeometry().countPoints() == 20);
        REQUIRE(toolTip != nullptr);
      }
    }

    WHEN("[边距相关操作] 选中容器，按住 option，操作边距手柄") {
      _.dispatchMouseMove(200, 25, {.altKey = true});
      _.dispatchHalfDraggingEventSuite(200, 25, 200, 35, {.altKey = true});

      THEN("展示边距数值的 tooltip 和 上下边距边框") {
        let region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(!region->getBorderGeometry().isEmpty());
        REQUIRE(region->getBorderGeometry().countPoints() == 10);
        REQUIRE(toolTip != nullptr);
      }
      _.dispatchMouseUp(200, 35);
    }

    WHEN("[边距相关操作] 选中容器，按住 option 和 shift，操作边距手柄") {
      _.dispatchMouseMove(200, 25, {.shiftKey = true, .altKey = true});
      _.dispatchHalfDraggingEventSuite(200, 25, 200, 35,
                                       {.shiftKey = true, .altKey = true});

      THEN("展示边距数值的 tooltip 和 四边距边框") {
        let region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        let toolTip =
            _.renderTree->getRenderItemById(ElId::AutoLayoutHandleToolTip);
        REQUIRE(region->getBorderGeometry().countPoints() == 20);
        REQUIRE(toolTip != nullptr);
      }
      _.dispatchMouseUp(200, 35);
    }

    WHEN("[边距相关操作] 选中容器，点击边距区域，然后鼠标移出容器") {
      _.setSelection({f1});
      _.dispatchMouseMove(200, 15);
      _.dispatchClick(200, 15);
      _.dispatchMouseMove(200, -20);

      THEN("展示边距边框") {
        let region =
            _.getElSpriteById<AutoLayoutRegionSprite>(ElId::AutoLayoutRegion);
        REQUIRE(!region->getBorderGeometry().isEmpty());
      }
    }

    WHEN("属性面板中鼠标悬浮在 rect1.width 的 '填充容器'") {
      _.setSelection({rect1});
      _.documentRoot->currentPage().setAutoLayoutHoverMenuItem(
          AutoLayoutHoverMenuItem::WidthFillContainer);
      _.flushNodeEvents();

      THEN("展示容器的 tooltip 和左右提示线") {
        let guideLine = _.getElSpriteById<AutoLayoutGuideLineSprite>(
            ElId::AutoLayoutGuideLine);
        let guideLineTip = _.getElSpriteById<AutoLayoutGuideLineTipSprite>(
            ElId::AutoLayoutGuideToolTip);
        REQUIRE(guideLineTip != nullptr);
        let lines = guideLine->getLines();
        REQUIRE(lines.size() == 2);
        REQUIRE(lines[0].startPoint == SkPoint::Make(60, 100));
        REQUIRE(lines[0].endPoint == SkPoint::Make(0, 100));
        REQUIRE(lines[1].startPoint == SkPoint::Make(160, 100));
        REQUIRE(lines[1].endPoint == SkPoint::Make(400, 100));
      }
    }

    WHEN("属性面板中鼠标悬浮在 rect1.height 的 '填充容器'") {
      _.setSelection({rect1});
      _.documentRoot->currentPage().setAutoLayoutHoverMenuItem(
          AutoLayoutHoverMenuItem::HeightFillContainer);
      _.flushNodeEvents();

      THEN("展示容器的 tooltip 和上下提示线") {
        let guideLine = _.getElSpriteById<AutoLayoutGuideLineSprite>(
            ElId::AutoLayoutGuideLine);
        let guideLineTip = _.getElSpriteById<AutoLayoutGuideLineTipSprite>(
            ElId::AutoLayoutGuideToolTip);
        REQUIRE(guideLineTip != nullptr);
        let lines = guideLine->getLines();
        REQUIRE(lines.size() == 2);
        REQUIRE(lines[0].startPoint == SkPoint::Make(110, 50));
        REQUIRE(lines[0].endPoint == SkPoint::Make(110, 0));
        REQUIRE(lines[1].startPoint == SkPoint::Make(110, 150));
        REQUIRE(lines[1].endPoint == SkPoint::Make(110, 200));
      }
    }

    WHEN("属性面板中鼠标悬浮在 frame.width 的 '包围内容'") {
      _.setSelection({f1});
      _.documentRoot->currentPage().setAutoLayoutHoverMenuItem(
          AutoLayoutHoverMenuItem::WidthHug);
      _.flushNodeEvents();

      THEN("展示包裹内容的 tooltip、虚线边框和左右提示线") {
        let guideLine = _.getElSpriteById<AutoLayoutGuideLineSprite>(
            ElId::AutoLayoutGuideLine);
        let guideLineTip = _.getElSpriteById<AutoLayoutGuideLineTipSprite>(
            ElId::AutoLayoutGuideToolTip);
        let guideBounds =
            _.getElSpriteById<PolygonBorderSprite>(ElId::AutoLayoutGuideBounds);
        REQUIRE(guideLineTip != nullptr);
        REQUIRE(guideBounds != nullptr);
        let lines = guideLine->getLines();
        REQUIRE(lines.size() == 2);
        REQUIRE(lines[0].startPoint == SkPoint::Make(0, 100));
        REQUIRE(lines[0].endPoint == SkPoint::Make(60, 100));
        REQUIRE(lines[1].startPoint == SkPoint::Make(400, 100));
        REQUIRE(lines[1].endPoint == SkPoint::Make(340, 100));
      }
    }

    WHEN("属性面板中鼠标悬浮在 frame.height 的 '包围内容'") {
      _.setSelection({f1});
      _.documentRoot->currentPage().setAutoLayoutHoverMenuItem(
          AutoLayoutHoverMenuItem::HeightHug);
      _.flushNodeEvents();

      THEN("展示包裹内容的 tooltip、虚线边框和上下提示线") {
        let guideLine = _.getElSpriteById<AutoLayoutGuideLineSprite>(
            ElId::AutoLayoutGuideLine);
        let guideLineTip = _.getElSpriteById<AutoLayoutGuideLineTipSprite>(
            ElId::AutoLayoutGuideToolTip);
        let guideBounds =
            _.getElSpriteById<PolygonBorderSprite>(ElId::AutoLayoutGuideBounds);
        REQUIRE(guideLineTip != nullptr);
        REQUIRE(guideBounds != nullptr);
        let lines = guideLine->getLines();
        REQUIRE(lines.size() == 2);
        REQUIRE(lines[0].startPoint == SkPoint::Make(200, 0));
        REQUIRE(lines[0].endPoint == SkPoint::Make(200, 50));
        REQUIRE(lines[1].startPoint == SkPoint::Make(200, 200));
        REQUIRE(lines[1].endPoint == SkPoint::Make(200, 150));
      }
    }
  }
}

SCENARIO("AutoLayout 的编辑") {
  auto _ = createAppContext();

  GIVEN("[多选] 两个自动布局的 Frame 节点") {
    auto f1 = _.drawFrame(100, 100, 100, 100);
    auto f2 = _.drawFrame(100, 100, 300, 300);

    _.addAutoLayout({f1});
    _.addAutoLayout({f2});

    WHEN("选中两个节点，并更改排列方向") {
      REQUIRE(f1.getStackMode() == StackMode::Vertical);
      REQUIRE(f2.getStackMode() == StackMode::Vertical);

      BatchAutoLayoutUpdatedParam param;
      for (auto node : {f1, f2}) {
        param.params.push_back(
            {.nodeId = node.getId(), .stackMode = StackMode::Horizontal});
      }

      _.commandInvoker->invoke(UpdateBatchAutoLayoutCommand(param));

      THEN("两个 frame 的排列方向均更新") {
        REQUIRE(f1.getStackMode() == StackMode::Horizontal);
        REQUIRE(f2.getStackMode() == StackMode::Horizontal);
      }
    }

    WHEN("浮动输入框打开状态") {
      WHEN("批量更新自动布局的值") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::Horizontal));
        _.commandInvoker->invoke(UpdateBatchAutoLayoutCommand({.params = {}}));

        THEN("浮动输入框自动被关闭") {
          auto state = _.getAutoLayoutViewState();
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::None);
        }
      }

      WHEN("批量更新自动布局的值，但指定不关闭输入框") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::Horizontal));
        _.commandInvoker->invoke(UpdateBatchAutoLayoutCommand(
            {.params = {}, .clearFloatInput = false}));

        THEN("浮动输入框不关闭") {
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::Horizontal);
        }
      }

      WHEN("上边距单边输入框轮换下一个逻辑") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::Top));
        _.commandInvoker->invoke(NextAutoLayoutFloatInputCommand());

        THEN("轮换为右边距输入框") {
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::Right);
        }
      }

      WHEN("上边距单边输入框轮换上一个逻辑") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::Top));
        _.commandInvoker->invoke(NextAutoLayoutFloatInputCommand(true));

        THEN("轮换为右边距输入框") {
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::Space);
        }
      }

      WHEN("双边距单边输入框轮换下一个逻辑") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::Vertical));
        _.commandInvoker->invoke(NextAutoLayoutFloatInputCommand());

        THEN("轮换为右边距输入框") {
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::Space);
        }
      }

      WHEN("双边距单边输入框轮换上一个逻辑") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::Horizontal));
        _.commandInvoker->invoke(NextAutoLayoutFloatInputCommand(true));

        THEN("轮换为右边距输入框") {
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::Space);
        }
      }

      WHEN("四边距单边输入框轮换下一个逻辑") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::All));
        _.commandInvoker->invoke(NextAutoLayoutFloatInputCommand());

        THEN("轮换为右边距输入框") {
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::Space);
        }

        THEN("再次轮换，回到四边距") {
          _.commandInvoker->invoke(NextAutoLayoutFloatInputCommand());
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::All);
        }
      }

      WHEN("四边距单边输入框轮换上一个逻辑") {
        _.commandInvoker->invoke(
            UpdateAutoLayoutFloatInputCommand(FloatSpaceInputType::All));
        _.commandInvoker->invoke(NextAutoLayoutFloatInputCommand(true));

        THEN("轮换为右边距输入框") {
          REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                  FloatSpaceInputType::Space);
        }
      }
    }
  }

  GIVEN("[多选] 两个自动布局子节点") {
    auto f1 = _.drawFrame(100, 100, 100, 100);
    auto r1 = _.drawFrame(20, 20, 110, 110);
    auto r2 = _.drawFrame(20, 20, 130, 130);

    _.addAutoLayout({f1});

    WHEN("选中两个节点，并更改宽高类型") {
      REQUIRE(r1.getStackChildPrimaryGrow() == 0.0);
      REQUIRE(r2.getStackChildPrimaryGrow() == 0.0);

      BatchAutoLayoutChildUpdatedParam param;
      for (auto node : {r1, r2}) {
        param.params.push_back(
            {.nodeId = node.getId(), .stackChildPrimaryGrow = 1.0});
      }

      _.commandInvoker->invoke(UpdateBatchAutoLayoutChildCommand(param));

      THEN("两个节点的宽高类型均被更新") {
        REQUIRE(r1.getStackChildPrimaryGrow() == 1.0);
        REQUIRE(r2.getStackChildPrimaryGrow() == 1.0);
      }
    }
  }
}

SCENARIO("AutoLayout 的添加") {
  auto _ = createAppContext();

  GIVEN("[单选] 一个 Frame 节点") {
    auto frame = _.drawFrame(100, 100, 100, 100);

    WHEN("为 frame 添加 AutoLayout") {
      _.setSelection({frame});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("frame 的 AutoLayout 属性被设置，宽高保持不变") {
        REQUIRE(frame.getStackMode() == StackMode::Vertical);
        REQUIRE(frame.getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMin);
        REQUIRE(frame.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(frame.getStackCounterAlignItems() == StackAlign::StackAlignMin);
        REQUIRE(frame.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(frame.getStackVerticalPadding() == 10);
        REQUIRE(frame.getStackPaddingBottom() == 10);
        REQUIRE(frame.getStackHorizontalPadding() == 10);
        REQUIRE(frame.getStackPaddingRight() == 10);
        REQUIRE(frame.getStackSpacing() == 10);
        REQUIRE(frame.getWidth() == 100);
        REQUIRE(frame.getHeight() == 100);
        REQUIRE(frame.getClipsContent() == false);
      }
    }
  }

  GIVEN("[单选] 为已存在 AutoLayout 的 frame 节点") {
    auto frame = _.drawFrame(100, 100, 100, 100);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("为 frame 添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame2，frame 成为 frame2 的子元素，frame2 添加 "
           "AutoLayout，边距为 10") {
        let selection = _.getSelection();
        let frame2 =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame2->getChildrenHandles().size() == 1);
        REQUIRE(frame2->getChildren().front().getId() == frame.getId());
        REQUIRE(frame2->getStackMode() == StackMode::Horizontal);
        REQUIRE(frame2->getStackHorizontalPadding() == 10);
        REQUIRE(frame2->getStackVerticalPadding() == 10);
        REQUIRE(frame2->getStackPaddingRight() == 10);
        REQUIRE(frame2->getStackPaddingBottom() == 10);
        REQUIRE(frame2->getFills().empty());
      }
    }
  }

  GIVEN("[单选] 一个 group 节点") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 150, 0);
    auto group = _.createGroup({rect1, rect2});
    _.setSelection({group});

    WHEN("为 group 添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("group 转化为 frame，在 frame 上添加 AutoLayout") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getChildrenHandles().size() == 2);
        REQUIRE(frame->getStackMode() == StackMode::Horizontal);
      }
    }
  }

  GIVEN("[满足推荐条件的多选] 选择的图层构成 1 个标准行，且等间距") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 200, 0);
    auto rect3 = _.drawRectangle(100, 100, 400, 0);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN(
          "创建 frame 包含三个矩形，frame 添加横向的、边距为 0 的 AutoLayout") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getChildrenHandles().size() == 3);
        REQUIRE(frame->getStackMode() == StackMode::Horizontal);
        REQUIRE(frame->getStackVerticalPadding() == 0);
        REQUIRE(frame->getStackHorizontalPadding() == 0);
        REQUIRE(frame->getStackPaddingBottom() == 0);
        REQUIRE(frame->getStackPaddingRight() == 0);
      }
    }
  }

  GIVEN("[满足推荐条件的多选] 选择的图层构成 1 个标准列，且等间距") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 0, 200);
    auto rect3 = _.drawRectangle(100, 100, 0, 400);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN(
          "创建 frame 包含三个矩形，frame 添加纵向的、边距为 0 的 AutoLayout") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getChildrenHandles().size() == 3);
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE(frame->getStackVerticalPadding() == 0);
        REQUIRE(frame->getStackHorizontalPadding() == 0);
        REQUIRE(frame->getStackPaddingBottom() == 0);
        REQUIRE(frame->getStackPaddingRight() == 0);
      }
    }
  }
  /*
          GIVEN("[满足推荐条件的多选]
     最下层存在一个矩形，能包裹其它选中图层，且其它图层满足要求") { auto rect1 =
     _.drawRectangle(500, 500, 0, 0); auto rect2 = _.drawRectangle(100, 100, 50,
     50); auto rect3 = _.drawRectangle(100, 100, 250, 50);
              _.setSelection({rect1, rect2, rect3});

              WHEN("为选中的图层添加 AutoLayout") {
                  THEN("rect1 转为 frame， 包含 rect2、rect3；frame
     添加横向的、边距为 50 的 AutoLayout") {
                      //                let selection = _.getSelection();
                      //                let frame =
     _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
                      //                REQUIRE(frame->getChildrenIds().size()
     == 2);
                      //                REQUIRE(frame->getStackMode() ==
     StackMode::Vertical);
                      //                REQUIRE(frame->getPaddingTop() == 50);
                      //                REQUIRE(frame->getPaddingBottom() ==
     50);
                      //                REQUIRE(frame->getPaddingLeft() == 50);
                      //                REQUIRE(frame->getPaddingRight() == 50);
                  }
              }
          }
  */

  GIVEN("[不满足推荐条件的多选] 三个宽高为 100 的矩形位置在 (0, 0)，(300, 0), "
        "(200, 100)") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 300, 0);
    auto rect3 = _.drawRectangle(100, 100, 100, 100);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含三个矩形，frame 添加横向的、间距为 50 的 "
           "AutoLayout") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getChildrenHandles().size() == 3);
        REQUIRE(frame->getStackMode() == StackMode::Horizontal);
        REQUIRE(frame->getStackSpacing() == 50);
      }
    }
  }

  GIVEN("[不满足推荐条件的多选] 三个宽高为 100 的矩形位置在 (0, 0)，(50, 150), "
        "(0, 250)") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 50, 150);
    auto rect3 = _.drawRectangle(100, 100, 0, 250);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含三个矩形，frame 添加纵向的、间距为 25 的 "
           "AutoLayout") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getChildrenHandles().size() == 3);
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE(frame->getStackSpacing() == 25);
      }
    }
  }

  GIVEN("[横纵方向的判定] 两个矩形上下排列，中间部分有重叠，重叠区域宽大于高") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 0, 80);

    WHEN("选中两个矩形，添加 AutoLayout") {
      _.setSelection({rect1, rect2});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN(
          "创建 frame 包含两个矩形，frame 添加纵向的、间距为 0 的 AutoLayout") {
        let selection = _.getSelectionNodes();
        let frame = selection.front().tryAs<FrameNode>();
        REQUIRE(frame->getChildrenHandles().size() == 2);
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE(frame->getStackSpacing() == 0);
      }
    }
  }

  GIVEN("[对齐方式的默认值] 三个宽高为 100 的矩形位置在 (0, 0)，(150, 50), "
        "(300, 0)") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 150, 50);
    auto rect3 = _.drawRectangle(100, 100, 300, 0);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含三个矩形，frame 添加横向的 AutoLayout，对齐方式为 "
           "左、上") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getStackMode() == StackMode::Horizontal);
        REQUIRE(frame->getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMin);
        REQUIRE(frame->getStackCounterAlignItems() ==
                StackAlign::StackAlignMin);
      }
    }
  }

  GIVEN("[对齐方式的默认值] 三个矩形 (100, 100, 0, 0)，(100, 200, 150, -50), "
        "(100, 100, 300, 0)") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 200, 150, -50);
    auto rect3 = _.drawRectangle(100, 100, 300, 0);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含三个矩形，frame 添加横向的 AutoLayout，对齐方式为 "
           "左、中") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getStackMode() == StackMode::Horizontal);
        REQUIRE(frame->getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMin);
        REQUIRE(frame->getStackCounterAlignItems() ==
                StackAlign::StackAlignCenter);
      }
    }
  }

  GIVEN("[对齐方式的默认值] 三个矩形 (100, 100, 0, 0)，(110, 100, -10, 100), "
        "(100, 100, 0, 200)") {
    auto rect1 = _.drawRectangle(100, 100, 0, 0);
    auto rect2 = _.drawRectangle(110, 100, -10, 100);
    auto rect3 = _.drawRectangle(100, 100, 0, 200);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含三个矩形，frame 添加纵向的 AutoLayout，对齐方式为 "
           "右、上") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE(frame->getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMin);
        REQUIRE(frame->getStackCounterAlignItems() ==
                StackAlign::StackAlignMax);
      }
    }
  }

  GIVEN("[顺序] 三个矩形 (200, 100, 0, 0)、(100, 200, 200, 100)、(200, 100, 0, "
        "140)") {
    auto rect1 = _.drawRectangle(200, 100, 0, 0);
    auto rect2 = _.drawRectangle(100, 200, 200, 100);
    auto rect3 = _.drawRectangle(200, 100, 0, 140);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含三个矩形，frame 添加纵向的 "
           "AutoLayout，矩形排列顺序为 rect1、rect3、rect2") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(0, 0));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(100, 100));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(0, 300));
      }
    }
  }

  GIVEN("[间距] 三个矩形 (200, 100, 0, 0)、(200, 200, 0, 150)、(200, 100, 0, "
        "350)") {
    auto rect1 = _.drawRectangle(200, 100, 0, 0);
    auto rect2 = _.drawRectangle(200, 100, 0, 150);
    auto rect3 = _.drawRectangle(200, 100, 0, 350);
    _.setSelection({rect1, rect2, rect3});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含三个矩形，frame 添加纵向的 AutoLayout，间距为 75") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE(frame->getStackSpacing() == 75);
      }
    }
  }

  GIVEN("[间距] 两个矩形 (200, 100, 0, 0)、(200, 200, 0, 50)") {
    auto rect1 = _.drawRectangle(200, 100, 0, 0);
    auto rect2 = _.drawRectangle(200, 200, 0, 50);
    _.setSelection({rect1, rect2});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 frame 包含两个矩形，frame 添加纵向的 AutoLayout，间距为 0") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE(frame->getStackSpacing() == 0);
      }
    }
  }

  GIVEN("[边距] frame (400, 200, 0, 0) 包含两个矩形 (100, 100, 60, 75) 和 "
        "(100, 100, 250, 20)") {
    auto frame = _.drawFrame(400, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 60, 75);
    auto rect2 = _.drawRectangle(100, 100, 250, 20);
    _.setSelection({frame});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("frame 添加横向的 AutoLayout，左右边距为 50，上下边距为 20") {
        REQUIRE(frame.getStackMode() == StackMode::Horizontal);
        REQUIRE(frame.getStackHorizontalPadding() == 50);
        REQUIRE(frame.getStackPaddingRight() == 50);
        REQUIRE(frame.getStackVerticalPadding() == 20);
        REQUIRE(frame.getStackPaddingBottom() == 20);
      }
    }
  }

  GIVEN("frame(100, 100, 0, 0)，frame 外有矩形 r1(50, 50, 0, 100)，frame "
        "内有矩形 r2(50, 50, 25, 25)") {
    auto frame = _.drawFrame(100, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 25, 125);
    auto r2 = _.drawRectangle(50, 50, 25, 25);

    WHEN("选中 r1，r2，添加 AutoLayout") {
      _.setSelection({r1, r2});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("r1, r2 创建 frame2 包裹的自动布局，世界坐标不变") {
        REQUIRE(r1.getParent()->is(NodeType::Frame));
        REQUIRE_THAT(r1.getWorldTransform(),
                     TransformMatcher::Translate(25, 125));
        REQUIRE_THAT(r2.getWorldTransform(),
                     TransformMatcher::Translate(25, 25));
      }
    }
  }

  GIVEN("画布上有一条垂直直线") {
    auto line = _.drawLine(0, 0, 100, 0);
    _.setSelection({line});

    WHEN("为直线添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("直线添加纵向的 AutoLayout，边距为 10") {
        auto node = _.getSelectionNodes().front();
        let frame = node.tryAs<FrameNode>();
        REQUIRE(frame->getStackMode() == StackMode::Vertical);
        REQUIRE(frame->getStackHorizontalPadding() == 10);
        REQUIRE(frame->getStackPaddingRight() == 10);
        REQUIRE(frame->getStackVerticalPadding() == 10);
        REQUIRE(frame->getStackPaddingBottom() == 10);
      }
    }
  }

  GIVEN("画布上有一个矩形的实例") {
    auto c1 = _.drawRectangleComponent(100, 100, 0, 0);
    auto i1 = _.duplicateNodes({c1})[0].as<InstanceNode>();

    WHEN("选中实例，添加 AutoLayout") {
      _.setSelection({i1});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("创建 f1，i1 成为 f1 的子元素，f1 添加 AutoLayout，边距为 10") {
        let selection = _.getSelection();
        let f1 = _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(f1->getChildrenHandles().size() == 1);
        REQUIRE(f1->getChildren().front().getId() == i1.getId());
        REQUIRE(f1->getStackMode() == StackMode::Horizontal);
        REQUIRE(f1->getStackHorizontalPadding() == 10);
        REQUIRE(f1->getStackVerticalPadding() == 10);
        REQUIRE(f1->getStackPaddingRight() == 10);
        REQUIRE(f1->getStackPaddingBottom() == 10);
        REQUIRE(f1->getFills().empty());
      }
    }
  }

  GIVEN("[特殊逻辑] 为一个文本图层添加 AutoLayout") {
    auto text = _.createText(60, 40, 20, 20, "abcd", TextAutoResize::NONE, 14);
    _.setSelection({text});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("frame 添加横向的 AutoLayout") {
        let selection = _.getSelection();
        let frame =
            _.documentRoot->tryGetNodeById<FrameNode>(selection.front());
        REQUIRE(frame->getStackMode() == StackMode::Horizontal);
      }
    }
  }

  GIVEN("[特殊逻辑] frame 内有一个和 frame 完全一致的矩形") {
    auto frame = _.drawFrame(100, 100, 0, 0);
    auto rect = _.drawRectangle(50, 50, 50, 50);
    rect.setStrokeWeight(3.f);
    _.adjustLayout(
        rect, {.width = 100, .height = 100, .positionX = 0, .positionY = 0});
    _.setSelection({frame});

    WHEN("为选中的图层添加 AutoLayout") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("将矩形的样式复制到 frame 上，并删除矩形") {
        REQUIRE(frame.getStackMode() == StackMode::Vertical);
        REQUIRE(frame.getChildrenHandles().empty());
        REQUIRE(frame.getStrokeWeight() == 3.f);
      }
    }
  }
}

SCENARIO("AutoLayout 的智能推荐") {
  auto _ = createAppContext();

  GIVEN("空白画布") {
    WHEN("单选 frame") {
      auto frame = _.drawFrame(100, 100, 0, 0);
      _.setSelection({frame});

      THEN("展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                true);
      }
    }

    WHEN("单选 group") {
      auto rect = _.drawRectangle(100, 100, 0, 0);
      auto group = _.createGroup({rect});
      _.setSelection({group});

      THEN("展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                true);
      }
    }

    WHEN("单选 component") {
      auto rect = _.drawRectangle(100, 100, 0, 0);
      _.setSelection({rect});
      auto component = _.componentSelection();
      _.setSelection({component});

      THEN("展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                true);
      }
    }

    WHEN("单选 rect") {
      auto rect = _.drawRectangle(100, 100, 0, 0);
      _.setSelection({rect});

      THEN("不展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                false);
      }
    }

    WHEN("单选已存在 AutoLayout 的 frame") {
      auto frame = _.drawFrame(100, 100, 0, 0);
      _.setSelection({frame});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("不展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                false);
      }
    }

    WHEN("选中投影相交的两个矩形") {
      auto rect1 = _.drawRectangle(100, 100, 0, 0);
      auto rect2 = _.drawRectangle(100, 100, 200, 0);
      _.setSelection({rect1, rect2});

      THEN("展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                true);
      }
    }

    WHEN("选中投影不相交的两个矩形") {
      auto rect1 = _.drawRectangle(100, 100, 0, 0);
      auto rect2 = _.drawRectangle(100, 100, 200, 200);
      _.setSelection({rect1, rect2});

      THEN("不展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                false);
      }
    }

    WHEN("选中组成标准行且等间距的三个矩形") {
      auto rect1 = _.drawRectangle(100, 100, 0, 0);
      auto rect2 = _.drawRectangle(50, 100, 150, 0);
      auto rect3 = _.drawRectangle(100, 100, 250, 0);
      _.setSelection({rect1, rect2, rect3});

      THEN("展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                true);
      }
    }

    WHEN("选中组成标准列且等间距的三个矩形") {
      auto rect1 = _.drawRectangle(100, 100, 0, 0);
      auto rect2 = _.drawRectangle(50, 50, 0, 200);
      auto rect3 = _.drawRectangle(50, 50, 0, 350);
      _.setSelection({rect1, rect2, rect3});

      THEN("展示智能推荐") {
        REQUIRE(_.documentRoot->tryGetCurrentPage()->getRecommendAutoLayout() ==
                true);
      }
    }
  }
}

SCENARIO("AutoLayout 的共同作用") {
  auto _ = createAppContext();

  GIVEN("一个 frame(350, 200) 包含两个矩形 (100, 100, 50, 50)、(100, 100, 200, "
        "50)，对 frame 添加 AutoLayout") {
    auto frame = _.drawFrame(350, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("[容器为固定] 修改 frame 的高度") {
      _.adjustLayout(frame, {.height = 300});

      THEN("frame 高度模式为固定，子图层位置和大小不变") {
        REQUIRE(frame.getStackCounterSizing() == StackSize::StackSizeFixed);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE(rect1.getWidth() == 100);
        REQUIRE(rect1.getHeight() == 100);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE(rect2.getWidth() == 100);
        REQUIRE(rect2.getHeight() == 100);
      }
    }

    WHEN("[容器为固定] 修改 frame 的宽度模式为固定，然后删除 rect2") {
      frame.setStackPrimarySizing(StackSize::StackSizeFixed);
      _.commandInvoker->invoke(DeleteNodeCommand(rect2.getId()));

      THEN("frame 宽度不变") { REQUIRE(frame.getWidth() == 350); }
    }

    WHEN(
        "[容器为弹性] 调整 frame 宽度到 500，再修改 frame 宽度模式为包围内容") {
      _.adjustLayout(frame, {.width = 500});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimarySizing =
               StackSize::StackSizeResizeToFitWithImplicitSize}));

      THEN("frame 宽度被调整为 350") { REQUIRE(frame.getWidth() == 350); }
    }

    WHEN("[容器为弹性] 调整 rect1 的宽度为 200") {
      _.adjustLayout(rect1, {.width = 200});

      THEN("frame 宽度被调整为 450, rect2 位置被调整为 (300, 50)") {
        REQUIRE(frame.getWidth() == 450);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(300, 50));
      }
    }

    WHEN("[容器为弹性] 调整 frame 间距为 100") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(), .stackSpacing = 100}));

      THEN("frame 宽度被调整为 400，rect2 位置被调整为 (250, 50)") {
        REQUIRE(frame.getWidth() == 400);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(250, 50));
      }
    }

    WHEN("[容器为弹性] 删除 rect2") {
      _.commandInvoker->invoke(DeleteNodeCommand(rect2.getId()));
      DocumentProto::Arg_updateContainerNodeLayout arg;
      arg.set_nodeid(frame.getId());
      _.jsCommandCenter->updateContainerNodeLayout(arg);

      THEN("frame 宽度被调整为 200") { REQUIRE(frame.getWidth() == 200); }
    }

    WHEN("[子图层为弹性] 修改 rect2 的宽度模式为填充容器，调整 frame 宽度为 "
         "400") {
      rect2.setStackChildPrimaryGrow(1.f);
      _.adjustLayout(frame, {.width = 400});

      THEN("rect2 的位置不变，宽度被调整为 150") {
        REQUIRE(rect2.getWidth() == 150);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
      }
    }

    WHEN("[子图层为弹性] 修改 rect2 的宽度模式为填充容器，调整 frame 间距为 "
         "100") {
      rect2.setStackChildPrimaryGrow(1.f);
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(), .stackSpacing = 100}));

      THEN("rect2 的位置调整为 (250, 50)，宽度被调整为 50") {
        REQUIRE(rect2.getWidth() == 50);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(250, 50));
      }
    }

    WHEN("[子图层为弹性] 修改 rect1 和 rect2 的宽度模式为填充容器，调整 frame "
         "宽度为 500") {
      rect1.setStackChildPrimaryGrow(1.f);
      rect2.setStackChildPrimaryGrow(1.f);
      _.adjustLayout(frame, {.width = 500});

      THEN("rect1 被调整为 (175, 100, 50, 50), rect2 被调整为 (175, 100, 275, "
           "50)") {
        REQUIRE(rect1.getWidth() == 175);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE(rect2.getWidth() == 175);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(275, 50));
      }
    }

    WHEN("[子图层为弹性] 修改 rect1 和 rect2 的宽度模式为填充容器，调整 frame "
         "右边距为 100") {
      rect1.setStackChildPrimaryGrow(1.f);
      rect2.setStackChildPrimaryGrow(1.f);
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(), .stackPaddingRight = 100}));

      THEN("rect1 被调整为 (75, 100, 50, 50), rect2 被调整为 (75, 100, 175, "
           "50)") {
        REQUIRE(rect1.getWidth() == 75);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE(rect2.getWidth() == 75);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(175, 50));
      }
    }

    WHEN("[间距为弹性] 修改 frame 的间距为弹性，然后修改 frame 的宽度为 500") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifySpaceEvently}));
      _.adjustLayout(frame, {.width = 500});

      THEN("frame 宽度模式变为固定，rect1 位置不变，rect2 位置被调整为 (350, "
           "50)") {
        REQUIRE(frame.getStackPrimarySizing() == StackSize::StackSizeFixed);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }

    WHEN("[容器为弹性] 调整 frame 宽度到 500，再修改 frame 的间距为弹性") {
      _.adjustLayout(frame, {.width = 500});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifySpaceEvently}));

      THEN("rect2 位置被调整为 (350, 50)") {
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }

    WHEN("[删除最后一个子图层] 删除 rect2，然后删除 rect1") {
      _.commandInvoker->invoke(DeleteNodeCommand(rect2.getId()));
      DocumentProto::Arg_updateContainerNodeLayout arg;
      arg.set_nodeid(frame.getId());
      _.jsCommandCenter->updateContainerNodeLayout(arg);
      _.commandInvoker->invoke(DeleteNodeCommand(rect1.getId()));
      _.jsCommandCenter->updateContainerNodeLayout(arg);

      THEN("frame 大小和删除 rect1 之前相同") {
        REQUIRE(frame.getWidth() == 200);
        REQUIRE(frame.getHeight() == 200);
      }
    }
  }

  GIVEN("[任何子图层的填充容器，和容器的包围内容冲突] frame 的 AutoLayout "
        "横向排列 rect1, rect2, rect3，frame "
        "的宽度模式为固定，rect1 的宽度模式为填充容器") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    frame.setStackPrimarySizing(StackSize::StackSizeFixed);
    rect1.setStackChildPrimaryGrow(1.f);

    WHEN("将 frame 的宽度模式设置为包围内容") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimarySizing =
               StackSize::StackSizeResizeToFitWithImplicitSize}));

      THEN("frame 的宽度模式为包围内容， rect1 的宽度模式被调整为固定") {
        REQUIRE(frame.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(rect1.getStackChildPrimaryGrow() == 0);
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Auto);
      }

      THEN("展示出 '矩形 1 被设定为固定宽度' 的 toast") {
        REQUIRE(_.fakeToastService->getLatestMessage().value() ==
                "矩形 1 被设定为固定宽度");
      }
    }
  }

  GIVEN("[任何子图层的填充容器，和容器的包围内容冲突] frame 的 AutoLayout "
        "横向排列 rect1, rect2, rect3，frame "
        "的宽度模式为包围内容") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("将 rect1 的宽度模式设置为填充容器") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect1.getId(), .stackChildPrimaryGrow = 1.f}));

      THEN("rect1 的宽度模式为填充容器，frame 的宽度模式被调整为固定宽度") {
        REQUIRE(frame.getStackPrimarySizing() == StackSize::StackSizeFixed);
        REQUIRE(rect1.getStackChildPrimaryGrow() == 1.f);
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Auto);
      }

      THEN("展示出 'Frame 被设定为固定宽度' 的 toast") {
        REQUIRE(_.fakeToastService->getLatestMessage() ==
                "Frame 被设定为固定宽度");
      }
    }
  }

  GIVEN("[全部子图层的填充容器，和容器的包围内容冲突] frame 的 AutoLayout "
        "横向排列 rect1, rect2, rect3，frame "
        "的高度模式为固定，rect1、rect2、rect3 的高度模式为填充容器") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    frame.setStackCounterSizing(StackSize::StackSizeFixed);
    rect1.setStackChildAlignSelf(StackCounterAlign::Stretch);
    rect2.setStackChildAlignSelf(StackCounterAlign::Stretch);
    rect3.setStackChildAlignSelf(StackCounterAlign::Stretch);

    WHEN("将 frame 的高度模式设置为包围内容") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackCounterSizing =
               StackSize::StackSizeResizeToFitWithImplicitSize}));

      THEN("frame 的高度模式为包围内容， rect1、rect2、rect3 "
           "的高度模式被调整为固定") {
        REQUIRE(frame.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(rect1.getStackChildPrimaryGrow() == 0);
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Auto);
        REQUIRE(rect2.getStackChildPrimaryGrow() == 0);
        REQUIRE(rect2.getStackChildAlignSelf() == StackCounterAlign::Auto);
        REQUIRE(rect3.getStackChildPrimaryGrow() == 0);
        REQUIRE(rect3.getStackChildAlignSelf() == StackCounterAlign::Auto);
      }

      THEN("展示出 '矩形 1、矩形 2、矩形 3 被设定为固定高度' 的 toast") {
        REQUIRE(_.fakeToastService->getLatestMessage().value() ==
                "矩形 1、矩形 2、矩形 3 被设定为固定高度");
      }
    }
  }

  GIVEN("[全部子图层的填充容器，和容器的包围内容冲突] frame 的 AutoLayout "
        "横向排列 rect1, rect2, rect3，frame "
        "的高度模式为包围内容，rect1、rect2、rect3 的高度模式为固定容器") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("将 rect1、rect2、rect3 的高度模式设置为填充容器") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect1.getId(),
           .stackChildAlignSelf = StackCounterAlign::Stretch}));
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect2.getId(),
           .stackChildAlignSelf = StackCounterAlign::Stretch}));
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect3.getId(),
           .stackChildAlignSelf = StackCounterAlign::Stretch}));

      THEN("frame 的高度模式为固定，rect1、rect2、rect3 的高度模式为填充容器") {
        REQUIRE(frame.getStackCounterSizing() == StackSize::StackSizeFixed);
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE(rect2.getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE(rect3.getStackChildAlignSelf() == StackCounterAlign::Stretch);
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3，frame "
        "的宽度模式为包围内容") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("将 rect1 宽度模式为填充容器，frame 容器排列方式修改为自动间距分布") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect1.getId(), .stackChildPrimaryGrow = 1.f}));
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifySpaceEvently}));

      THEN("frame 的宽度模式为固定，rect1 的宽度模式为固定") {
        REQUIRE(frame.getStackPrimarySizing() == StackSize::StackSizeFixed);
        REQUIRE(rect1.getStackChildPrimaryGrow() == 0);
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Auto);
      }

      THEN("展示出 'Frame 被设定为固定宽度' 的 toast") {
        REQUIRE(_.fakeToastService->getLatestMessage() ==
                "Frame 被设定为固定宽度");
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3，frame "
        "的宽度模式为固定，rect1 宽度模式为填充容器") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    frame.setStackPrimarySizing(StackSize::StackSizeFixed);
    rect1.setStackChildPrimaryGrow(1.f);

    WHEN("将 frame 容器排列方式修改为自动间距分布") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifySpaceEvently}));

      THEN("frame 的宽度模式为固定，rect1 的宽度模式为固定") {
        REQUIRE(frame.getStackPrimarySizing() == StackSize::StackSizeFixed);
        REQUIRE(rect1.getStackChildPrimaryGrow() == 0);
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Auto);
      }

      THEN("展示出 '矩形 1 被设定为固定宽度' 的 toast") {
        REQUIRE(_.fakeToastService->getLatestMessage() ==
                "矩形 1 被设定为固定宽度");
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3，frame "
        "的宽度模式为固定，分布方式为自动间距") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    frame.setStackPrimarySizing(StackSize::StackSizeFixed);
    frame.setStackPrimaryAlignItems(StackJustify::StackJustifySpaceEvently);

    WHEN("将 frame 容器宽度模式修改为包围内容") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimarySizing =
               StackSize::StackSizeResizeToFitWithImplicitSize}));

      THEN("frame 的分布方式被调整为固定间距") {
        REQUIRE(frame.getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMin);
      }

      THEN("展示出 'Frame 被设定为固定间距分布' 的 toast") {
        REQUIRE(_.fakeToastService->getLatestMessage().value() ==
                "Frame 被设定为固定间距分布");
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3，frame "
        "的宽度模式为固定，分布方式为自动间距") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    frame.setStackPrimarySizing(StackSize::StackSizeFixed);
    frame.setStackPrimaryAlignItems(StackJustify::StackJustifySpaceEvently);

    WHEN("将 rect1 宽度模式修改为填充容器") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect1.getId(), .stackChildPrimaryGrow = 1.f}));

      THEN("frame 的分布方式被调整为固定间距") {
        REQUIRE(frame.getStackPrimaryAlignItems() ==
                StackJustify::StackJustifyMin);
      }
    }
  }

  GIVEN("frame 的 AutoLayout 纵向排列 rect1, rect2，分布方式为自动间距") {
    auto frame = _.drawFrame(200, 500, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 50, 200);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("[间距为弹性] 修改 frame 的间距为弹性，然后修改 frame 的高度为 500") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifySpaceEvently}));
      _.adjustLayout(frame, {.height = 500});

      THEN("frame 高度模式变为固定，rect1 位置不变，rect2 位置被调整为 (50, "
           "350)") {
        REQUIRE(frame.getStackPrimarySizing() == StackSize::StackSizeFixed);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 350));
      }

      THEN("展示出 'Frame 被设定为固定高度' 的 toast") {
        REQUIRE(_.fakeToastService->getLatestMessage() ==
                "Frame 被设定为固定高度");
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2") {
    auto frame = _.drawFrame(125, 100, 0, 0);
    auto rect1 = _.drawRectangle(25, 50, 25, 25);
    auto rect2 = _.drawRectangle(50, 25, 50, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("将 rect1 的高度设置为填充容器") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect1.getId(),
           .stackChildAlignSelf = StackCounterAlign::Stretch}));

      THEN("frame 高度被调整为 75, rect1 高度被调整为 25, 位置为 (25, 25)") {
        REQUIRE(frame.getHeight() == 75);
        REQUIRE(rect1.getHeight() == 25);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(25, 25));
      }
    }
  }

  GIVEN("AutoLayout 容器 f1 嵌套 AutoLayout 容器 f2，设置 f2 相对 f1 "
        "主排列方向为填充内容") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto f2 = _.drawFrame(80, 80, 10, 10);
    _.addAutoLayout({f1});
    _.addAutoLayout({f2});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = f2.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("设置 f2 容器的主排列方向为包围内容") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f2.getId(),
           .stackPrimarySizing =
               StackSize::StackSizeResizeToFitWithImplicitSize}));

      THEN("f2 相对 f1 的主排列方向被调整为固定") {
        REQUIRE(f2.getStackChildPrimaryGrow() == 0.f);
      }
    }

    WHEN("设置 f2 容器的次要排列方向为包围内容") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f2.getId(),
           .stackCounterSizing =
               StackSize::StackSizeResizeToFitWithImplicitSize}));

      THEN("f2 相对 f1 的次要排列方向被调整为固定") {
        REQUIRE(f2.getStackChildAlignSelf() == StackCounterAlign::Auto);
      }
    }
  }
}

SCENARIO("AutoLayout 的对齐方式") {
  auto _ = createAppContext();

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3，frame "
        "的宽度模式为固定，rect1 宽度模式为填充容器") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("修改 frame 高度为 300，设置对齐方式为 左、上") {
      _.adjustLayout(frame, {.height = 300});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyMin,
           .stackCounterAlignItems = StackAlign::StackAlignMin}));

      THEN("rect1, rect2, rect3 位置不变") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }

    WHEN("修改 frame 高度为 300，设置对齐方式为 左、中") {
      _.adjustLayout(frame, {.height = 300});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyMin,
           .stackCounterAlignItems = StackAlign::StackAlignCenter}));

      THEN("rect1, rect2, rect3 位置为左中") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 100));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(200, 100));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(350, 100));
      }
    }

    WHEN("修改 frame 高度为 300，设置对齐方式为 左、下") {
      _.adjustLayout(frame, {.height = 300});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyMin,
           .stackCounterAlignItems = StackAlign::StackAlignMax}));

      THEN("rect1, rect2, rect3 位置为左下") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 150));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(200, 150));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(350, 150));
      }
    }

    WHEN("修改 frame 宽度为 600，设置对齐方式为 中、上") {
      _.adjustLayout(frame, {.width = 600});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyCenter,
           .stackCounterAlignItems = StackAlign::StackAlignMin}));

      THEN("rect1, rect2, rect3 位置为中上") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(100, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(250, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(400, 50));
      }
    }

    WHEN("修改 frame 宽度为 600，设置对齐方式为 右、上") {
      _.adjustLayout(frame, {.width = 600});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyMax,
           .stackCounterAlignItems = StackAlign::StackAlignMin}));

      THEN("rect1, rect2, rect3 位置为右上") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(150, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(300, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(450, 50));
      }
    }

    WHEN("修改 frame 高度为 300，宽度为 600，设置对齐方式为 中、中") {
      _.adjustLayout(frame, {.width = 600, .height = 300});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyCenter,
           .stackCounterAlignItems = StackAlign::StackAlignCenter}));

      THEN("rect1, rect2, rect3 位置为居中") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(100, 100));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(250, 100));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(400, 100));
      }
    }

    WHEN("修改 frame 高度为 300，宽度为 600，设置对齐方式为 右、中") {
      _.adjustLayout(frame, {.width = 600, .height = 300});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyMax,
           .stackCounterAlignItems = StackAlign::StackAlignCenter}));

      THEN("rect1, rect2, rect3 位置为右中") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(150, 100));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(300, 100));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(450, 100));
      }
    }

    WHEN("修改 frame 高度为 300，宽度为 600，设置对齐方式为 中、下") {
      _.adjustLayout(frame, {.width = 600, .height = 300});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyCenter,
           .stackCounterAlignItems = StackAlign::StackAlignMax}));

      THEN("rect1, rect2, rect3 位置为中下") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(100, 150));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(250, 150));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(400, 150));
      }
    }

    WHEN("修改 frame 高度为 300，宽度为 600，设置对齐方式为 右、下") {
      _.adjustLayout(frame, {.width = 600, .height = 300});
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimaryAlignItems = StackJustify::StackJustifyMax,
           .stackCounterAlignItems = StackAlign::StackAlignMax}));

      THEN("rect1, rect2, rect3 位置为右下") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(150, 150));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(300, 150));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(450, 150));
      }
    }
  }
}

SCENARIO("AutoLayout 的画布操作") {
  auto _ = createAppContext();

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2，frame "
        "的宽度模式和高度模式为固定") {
    auto frame = _.drawFrame(350, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = frame.getId(),
         .stackPrimarySizing = StackSize::StackSizeFixed,
         .stackCounterSizing = StackSize::StackSizeFixed}));

    WHEN("双击 frame 左边界框") {
      _.dispatchDblClick(0, 50);

      THEN("frame 的宽度模式被调整为包围内容") {
        REQUIRE(frame.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
      }
    }

    WHEN("双击 frame 上边界框") {
      _.dispatchDblClick(50, 0);

      THEN("frame 的高度模式被调整为包围内容") {
        REQUIRE(frame.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
      }
    }

    WHEN("调整 frame 宽度为 250，使 rect2 位于边界框下，然后双击 frame "
         "右边界框") {
      _.adjustLayout(frame, {.width = 250});
      _.dispatchDblClick(250, 50);

      THEN("frame 的宽度模式被调整为包围内容") {
        REQUIRE(frame.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
      }
    }

    WHEN("选中 rect1，按住 alt 双击 rect1 的左边界框") {
      _.setSelection({rect1});
      _.dispatchDblClick(50, 100, {.altKey = true});

      THEN("rect1 的宽度模式被修改为填充容器") {
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Auto);
        REQUIRE(rect1.getStackChildPrimaryGrow() == 1);
      }
    }

    WHEN("选中 rect1，按住 alt 双击 rect1 的上边界框") {
      _.setSelection({rect1});
      _.dispatchDblClick(100, 50, {.altKey = true});

      THEN("rect1 的宽度模式被修改为填充容器") {
        REQUIRE(rect1.getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE(rect1.getStackChildPrimaryGrow() == 0);
      }
    }
  }

  GIVEN("frame 的 AutoLayout 纵向排列 rect，frame 的宽度模式和高度模式为固定") {
    auto frame = _.drawFrame(200, 200, 0, 0);
    auto rect = _.drawRectangle(100, 100, 50, 50);

    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = frame.getId(),
         .stackPrimarySizing = StackSize::StackSizeFixed,
         .stackCounterSizing = StackSize::StackSizeFixed}));

    WHEN("双击 frame 左边界框") {
      _.dispatchDblClick(0, 50);

      THEN("frame 的宽度模式被调整为包围内容") {
        REQUIRE(frame.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
      }
    }

    WHEN("双击 frame 上边界框") {
      _.dispatchDblClick(50, 0);

      THEN("frame 的高度模式被调整为包围内容") {
        REQUIRE(frame.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
      }
    }

    WHEN("选中 rect1，按住 alt 双击 rect1 的左边界框") {
      _.setSelection({rect});
      _.dispatchDblClick(50, 100, {.altKey = true});

      THEN("rect1 的宽度模式被修改为填充容器") {
        REQUIRE(rect.getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE(rect.getStackChildPrimaryGrow() == 0);
      }
    }

    WHEN("选中 rect1，按住 alt 双击 rect1 的上边界框") {
      _.setSelection({rect});
      _.dispatchDblClick(100, 50, {.altKey = true});

      THEN("rect1 的宽度模式被修改为填充容器") {
        REQUIRE(rect.getStackChildAlignSelf() == StackCounterAlign::Auto);
        REQUIRE(rect.getStackChildPrimaryGrow() == 1);
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3, rect4") {
    auto frame = _.drawFrame(650, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    auto rect4 = _.drawRectangle(100, 100, 500, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("[shift 多选] 在未选中状态下按住 shift，鼠标依次点击 rect1, rect3") {
      _.setSelection({});
      _.dispatchClick(100, 100, {.shiftKey = true});
      _.dispatchClick(400, 100, {.shiftKey = true});

      THEN("rect1 和 rect3 被选中") {
        let selection = _.getSelectionNodes();
        REQUIRE(selection.size() == 2);
        REQUIRE(selection.front() == rect1);
        REQUIRE(selection.back() == rect3);
      }
    }

    WHEN("[单选调整子图层的顺序] 鼠标拖动 rect1 到 rect2 中心") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(100, 100, 250, 100);

      THEN("rect1 和 rect2 位置互换") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
      }
    }

    WHEN("[单选调整子图层的顺序] 通过快捷键将 rect1 右移") {
      _.setSelection({rect1});
      _.commandInvoker->invoke(AutoLayoutMoveChildrenCommand(
          {.type = AutoLayoutMoveChildrenType::Right, .altKey = false}));

      THEN("rect1 和 rect2 位置互换") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
      }
    }

    WHEN("[单选调整子图层的顺序] 通过快捷键将 rect1 左移") {
      _.setSelection({rect1});
      _.commandInvoker->invoke(AutoLayoutMoveChildrenCommand(
          {.type = AutoLayoutMoveChildrenType::Left, .altKey = false}));

      THEN("rect1 位置不变") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
      }
    }

    WHEN("[多选调整子图层的顺序] 选中 rect1 和 rect3， 将 rect1 拖动到 rect2 "
         "的位置") {
      _.setSelection({rect1, rect3});
      _.dispatchDraggingEventSuite(100, 100, 250, 100);

      THEN("矩形排列顺序为 rect2，rect1，rect4，rect3") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(500, 50));
        REQUIRE_THAT(rect4.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }

    WHEN("[多选调整子图层的顺序] 选中 rect1 和 rect3，将 rect1 拖动到 rect3 "
         "的位置") {
      _.setSelection({rect1, rect3});
      _.dispatchDraggingEventSuite(100, 100, 400, 100);

      THEN("矩形排列顺序为 rect2，rect4，rect1，rect3") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(500, 50));
        REQUIRE_THAT(rect4.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
      }
    }

    WHEN("[将子图层移出容器] 选中 rect1 和 rect4，拖出容器") {
      _.setSelection({rect1, rect4});
      _.dispatchDraggingEventSuite(100, 100, 100, 300);

      THEN("容器内矩形为 rect2、rect3, 位置被调整") {
        REQUIRE(frame.getChildrenHandles().size() == 2);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
      }
    }

    WHEN("[将子图层移入容器] 创建 rect5 并拖入容器起始位置") {
      auto rect5 = _.drawRectangle(100, 100, 0, 250);
      _.dispatchDraggingEventSuite(50, 300, 50, 100);

      THEN("rect5 被插入到 rect1 之前") {
        REQUIRE(frame.getChildrenHandles().size() == 5);
        REQUIRE_THAT(rect5.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
      }
    }

    WHEN("[将子图层移入容器] 创建 rect5 并拖入容器中间位置") {
      auto rect5 = _.drawRectangle(100, 100, 0, 250);
      _.dispatchDraggingEventSuite(50, 300, 325, 100);

      THEN("rect5 被插入到 rect2 之后") {
        REQUIRE(frame.getChildrenHandles().size() == 5);
        REQUIRE_THAT(rect5.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }

    WHEN("[将子图层移入容器] 创建 rect5、rect6 并拖入容器中间位置") {
      auto rect5 = _.drawRectangle(100, 100, 0, 250);
      auto rect6 = _.drawRectangle(100, 100, 100, 250);
      _.setSelection({rect5, rect6});
      _.dispatchDraggingEventSuite(50, 300, 325, 100);

      THEN("rect5、rect6 被插入到 rect2 之后") {
        REQUIRE(frame.getChildrenHandles().size() == 6);
        REQUIRE_THAT(rect5.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
        REQUIRE_THAT(rect6.getWorldTransform(),
                     TransformMatcher::Translate(500, 50));
      }
    }
    /*
            WHEN("[旋转子图层] 将 rect1 的高度设为
       80，宽度模式设为填充容器，然后将 rect1 旋转 90 度") {
                _.adjustLayout(rect1, {.height = 80});
                // 将 rect1 的宽度模式设为填充容器
                _.rotateNode(rect1, SK_FloatPI / 2);

                THEN("rect1 的高度被调整为 100，宽度模式为填充容器") {
                    //                REQUIRE(rect1.getHeight() == 100);
                    //                REQUIRE(rect1.getLayoutGrow() == 1);
                    //                REQUIRE(rect1.getLayoutAlign() ==
       LayoutAlign::Inherit);
                }
            }
    */
    WHEN("[间距相关操作] 鼠标拖动间距区域") {
      _.dispatchMouseMove(175, 75);
      _.dispatchDraggingEventSuite(175, 75, 200, 75);

      THEN("frame 被拖动") {
        REQUIRE_THAT(frame.getWorldTransform(),
                     TransformMatcher::Translate(25, 0));
      }
    }

    WHEN("[间距相关操作] 鼠标拖动 rect1 和 rect2 之间的间距手柄向右 25") {
      _.dispatchMouseMove(175, 100);
      _.dispatchDraggingEventSuite(175, 100, 200, 100);

      THEN("frame 间距被调整为 100") {
        REQUIRE(frame.getStackSpacing() == 100);
      }
    }

    WHEN("[间距相关操作] 按住 shift，鼠标拖动 rect1 和 rect2 "
         "之间的间距手柄向右 5") {
      _.dispatchMouseMove(175, 100);
      _.dispatchDraggingEventSuite(175, 100, 180, 100, {.shiftKey = true});

      THEN("frame 间距被调整为 60") { REQUIRE(frame.getStackSpacing() == 60); }
    }

    WHEN("[间距相关操作] 鼠标拖动 rect1 和 rect2 之间的间距手柄向左 200") {
      _.dispatchMouseMove(175, 100);
      _.dispatchDraggingEventSuite(175, 100, -25, 100);

      THEN("frame 间距被调整为 -350") {
        REQUIRE(frame.getStackSpacing() == -350);
      }
    }

    WHEN("[间距相关操作] 鼠标拖动 rect2 和 rect3 之间的间距手柄向右 30") {
      _.dispatchMouseMove(325, 100);
      _.dispatchDraggingEventSuite(325, 100, 355, 100);

      THEN("frame 间距被调整为 70") { REQUIRE(frame.getStackSpacing() == 70); }
    }

    WHEN("[边距相关操作] 鼠标拖动上边距手柄向上 25") {
      _.dispatchMouseMove(325, 25);
      _.dispatchDraggingEventSuite(325, 25, 325, 0);

      THEN("frame 上边距被调整为 25") {
        REQUIRE(frame.getStackVerticalPadding() == 25);
      }
    }

    WHEN("[边距相关操作] 鼠标拖动左边距手柄向右 25") {
      _.dispatchMouseMove(25, 100);
      _.dispatchDraggingEventSuite(25, 100, 50, 100);

      THEN("frame 左边距被调整为 75") {
        REQUIRE(frame.getStackHorizontalPadding() == 75);
      }
    }

    WHEN("[边距相关操作] 鼠标拖动下边距手柄向下 25") {
      _.dispatchMouseMove(325, 175);
      _.dispatchDraggingEventSuite(325, 175, 325, 200);

      THEN("frame 下边距被调整为 75") {
        REQUIRE(frame.getStackPaddingBottom() == 75);
      }
    }

    WHEN("[边距相关操作] 鼠标拖动右边距手柄向右 25") {
      _.dispatchMouseMove(625, 100);
      _.dispatchDraggingEventSuite(625, 100, 650, 100);

      THEN("frame 右边距被调整为 75") {
        REQUIRE(frame.getStackPaddingRight() == 75);
      }
    }

    WHEN("[边距相关操作] 按住 shift, 鼠标拖动上边距手柄向上 6") {
      _.dispatchMouseMove(325, 25);
      _.dispatchDraggingEventSuite(325, 25, 325, 19, {.shiftKey = true});

      THEN("frame 上边距被调整为 40") {
        REQUIRE(frame.getStackVerticalPadding() == 40);
      }
    }

    WHEN("[边距相关操作] 按住 alt, 鼠标拖动上边距手柄向上 25") {
      _.dispatchMouseMove(325, 25);
      _.dispatchDraggingEventSuite(325, 25, 325, 0, {.altKey = true});

      THEN("frame 上边距和下边距被调整为 25") {
        REQUIRE(frame.getStackVerticalPadding() == 25);
        REQUIRE(frame.getStackPaddingBottom() == 25);
      }
    }

    WHEN("[边距相关操作] 按住 alt 和 shift, 鼠标拖动上边距手柄向上 25") {
      _.dispatchMouseMove(325, 25);
      _.dispatchDraggingEventSuite(325, 25, 325, 0,
                                   {.shiftKey = true, .altKey = true});

      THEN("frame 上、下、左、右边距被调整为 25") {
        REQUIRE(frame.getStackVerticalPadding() == 25);
        REQUIRE(frame.getStackPaddingBottom() == 25);
        REQUIRE(frame.getStackHorizontalPadding() == 25);
        REQUIRE(frame.getStackPaddingRight() == 25);
      }
    }
  }
}

SCENARIO("画布区操作补充") {
  auto _ = createAppContext();

  GIVEN("frame 的 AutoLayout 纵向排列 rect1, rect2") {
    auto frame = _.drawFrame(200, 350, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(50, 100, 100, 200);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("鼠标拖动 rect1 到 rect2 中心") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(100, 100, 100, 250);

      THEN("rect1 和 rect2 位置互换") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 200));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(100, 50));
      }
    }

    WHEN("创建 rect3(100, 100, 0, 400)，将 rect3 拖到 frame 中间") {
      auto rect3 = _.drawRectangle(100, 100, 0, 400);
      _.dispatchDraggingEventSuite(50, 450, 100, 175);

      THEN("rect3 被插入到 rect1 和 rect2 中间") {
        REQUIRE(frame.getChildrenHandles().size() == 3);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(100, 350));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(50, 200));
      }
    }

    WHEN("创建 rect3(100, 100, 0, 400)，将 rect3 拖到 frame 末尾") {
      auto rect3 = _.drawRectangle(100, 100, 0, 400);
      _.dispatchDraggingEventSuite(50, 450, 100, 325);

      THEN("rect3 被插入到 rect2 后") {
        REQUIRE(frame.getChildrenHandles().size() == 3);
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(50, 350));
      }
    }

    WHEN("创建 rect3(100, 100, 0, 400)，同时选中 rect2 和 rect3，从 rect3 "
         "拖动到 frame 中央") {
      auto rect3 = _.drawRectangle(100, 100, 0, 400);
      _.setSelection({rect2, rect3});
      _.dispatchDraggingEventSuite(50, 450, 100, 175);

      THEN("rect2 和 rect3 被插入到 rect1 后") {
        REQUIRE(frame.getChildrenHandles().size() == 3);
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(100, 200));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(50, 350));
      }
    }
  }

  GIVEN("一个空的 frame 添加 AutoLayout，rect1 悬浮在 frame 上，rect2 在 frame "
        "外") {
    auto frame = _.drawFrame(100, 100, 0, 0);
    auto rect1 = _.drawRectangle(100, 50, -50, 0);
    auto rect2 = _.drawRectangle(100, 100, 200, 0);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("选中 rect1 和 rect2，从 frame 外拖动到 frame 内") {
      _.setSelection({rect1, rect2});
      _.dispatchDraggingEventSuite(-25, 25, 25, 25);

      THEN("rect1 和 rect2 被添加到 frame，并应用自动布局") {
        REQUIRE(frame.getChildrenHandles().size() == 2);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(10, 10));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(10, 70));
      }
    }

    WHEN("选中 rect1 和 rect2，从 frame 内拖动到 frame 外不松手，再回到 frame "
         "内") {
      _.setSelection({rect1, rect2});
      _.dispatchHalfDraggingEventSuite(25, 25, -25, 25);
      _.dispatchMouseMove(25, 25);
      _.dispatchMouseUp(25, 25);

      THEN("rect1 和 rect2 保持原位置") {
        REQUIRE(frame.getChildrenHandles().empty());
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(-50, 0));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(200, 0));
      }
    }
  }

  GIVEN("frame 包含 group { rect1, rect2 }，添加 AutoLayout，frame 外存在 "
        "rect3") {
    auto frame = _.drawFrame(200, 100, 0, 0);
    auto rect1 = _.drawRectangle(50, 50, 50, 25);
    auto rect2 = _.drawRectangle(50, 50, 100, 25);
    _.setSelection({rect1, rect2});
    auto group = _.groupSelection();
    auto rect3 = _.drawRectangle(50, 50, 300, 25);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("选中 rect2 和 rect3，从 rect2 拖动一段距离") {
      _.setSelection({rect2, rect3});
      _.dispatchDraggingEventSuite(125, 50, 150, 50);

      THEN("rect2 和 rect3 被移动，frame 应用自动布局") {
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(125, 25));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(325, 25));
        REQUIRE(frame.getWidth() == 225);
      }
    }
  }

  GIVEN("frame 横向排列 rect1, rect2， 添加 AutoLayout") {
    auto frame = _.drawFrame(250, 100, 0, 0);
    auto rect1 = _.drawRectangle(50, 50, 50, 25);
    auto rect2 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("在 rect1 和 rect2 中间创建 rect3") {
      auto rect3 = _.drawRectangle(50, 50, 100, 25);

      THEN("rect3 被插入到 rect1 和 rect2 中间，frame 应用自动布局") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 25));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(250, 25));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(150, 25));
        REQUIRE(frame.getWidth() == 350);
      }
    }

    WHEN("在 rect2 后面单击鼠标创建 rect3") {
      _.changeEditorMode(EditorMode::EM_Rectangle);
      _.dispatchClick(225, 25);

      THEN("rect3 被插入到 rect2 后面，frame 应用自动布局") {
        let rect3 = _.getSelectionNodes().front();
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(250, 25));
        REQUIRE(frame.getWidth() == 400);
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3，将 frame "
        "设置为自动间距") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = frame.getId(),
         .stackPrimaryAlignItems = StackJustify::StackJustifySpaceEvently}));

    WHEN("调整 rect2 的宽度为 150") {
      _.adjustLayout(rect2, {.width = 150});

      THEN("rect2 的位于 (175, 50)，frame 的间距为 25") {
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(175, 50));
        REQUIRE(frame.getStackSpacing() == 25);
      }
    }

    WHEN("单击鼠标在 rect1, rect2 中间创建 rect4") {
      _.changeEditorMode(EditorMode::EM_Rectangle);
      _.dispatchDblClick(175, 50);

      THEN("rect4 的位于 (150, 50)，frame 的间距为 0") {
        let rect4 = _.getSelectionNodes().front();
        REQUIRE_THAT(rect4.getWorldTransform(),
                     TransformMatcher::Translate(150, 50));
        REQUIRE(frame.getStackSpacing() == 0);
      }
    }

    WHEN("鼠标在 rect1, rect2 之间拖动创建 rect4") {
      let rect4 = _.drawRectangle(100, 100, 175, 50);

      THEN("rect4 的位于 (150, 50)，frame 的间距为 0") {
        REQUIRE_THAT(rect4.getWorldTransform(),
                     TransformMatcher::Translate(150, 50));
        REQUIRE(frame.getStackSpacing() == 0);
      }
    }

    WHEN("[导入的状态] 在 frame 间距值为 0 时，应用自动布局") {
      frame.setStackSpacing(0);
      _.commandInvoker->invoke(
          ApplyAutoLayoutCommand({.nodeId = frame.getId()}));

      THEN("rect1、rect2、rect3 位置不变") {
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }
  }

  GIVEN("frame 横向排列 rect1, rect2， 添加 AutoLayout") {
    auto frame = _.drawFrame(250, 100, 0, 0);
    auto rect1 = _.drawRectangle(50, 50, 50, 25);
    auto rect2 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("将 rect1 顺时针旋转 30°") {
      _.rotateNode(rect1, SK_FloatPI / 6);

      THEN("frame 宽高被调整为 268.3f, 118.3f, rect2 位置被调整为 (168.3f, "
           "25.f)") {
        REQUIRE(frame.getWidth() == Approx(268.3f));
        REQUIRE(frame.getHeight() == Approx(118.3f));
        REQUIRE(rect2.getWorldTransform().getTranslateX() == Approx(168.3f));
        REQUIRE(rect2.getWorldTransform().getTranslateY() == Approx(25.f));
      }
    }

    WHEN("选中 rect1 和 rect2, 鼠标拖动旋转一定角度") {
      _.setSelection({rect1, rect2});
      _.dispatchDraggingEventSuite(210, 15, 210, -10);

      THEN("frame 宽高被调整为 269.719f, 109.86f, rect1 位置被调整为 (50.f, "
           "36.109f), rect2 位置被调整为 "
           "(159.86f, 36.109f)") {
        REQUIRE(frame.getWidth() == Approx(269.719f));
        REQUIRE(frame.getHeight() == Approx(109.86f));
        REQUIRE(rect1.getWorldTransform().getTranslateX() == Approx(50.f));
        REQUIRE(rect1.getWorldTransform().getTranslateY() == Approx(36.109f));
        REQUIRE(rect2.getWorldTransform().getTranslateX() == Approx(159.86f));
        REQUIRE(rect2.getWorldTransform().getTranslateY() == Approx(36.109f));
      }
    }

    WHEN("拖动 rect1 左边到 x = 150 处") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(50, 50, 50, 150);

      THEN("rect1 宽度为 50，位置不变") {
        REQUIRE(rect1.getWidth() == Approx(50));
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 25));
      }
    }
  }

  GIVEN("f1 横向排列 f2, r1， 为 f2 添加 AutoLayout") {
    auto f1 = _.drawFrame(250, 100, 0, 0);
    auto f2 = _.drawFrame(80, 80, 10, 10);
    auto r1 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({f2});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("为 f1 添加 AutoLayout，将 r1 拖动到 f2 中不松手") {
      _.setSelection({f1});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      _.setSelection({r1});
      _.dispatchHalfDraggingEventSuite(175, 50, 50, 50);

      THEN("f1 应用自动布局，宽度为 100") { REQUIRE(f1.getWidth() == 100); }
      _.dispatchMouseUp(50, 50);
    }

    WHEN("将 r1 拖动到 f2 中不松手，再移出 f2") {
      _.setSelection({r1});
      _.dispatchHalfDraggingEventSuite(175, 50, 50, 50);
      _.dispatchMouseMove(150, 50);

      THEN("r1 跟随光标移动") {
        REQUIRE_THAT(r1.getWorldTransform(),
                     TransformMatcher::Translate(125, 25));
      }
      _.dispatchMouseUp(150, 50);
    }
  }
}

SCENARIO("AutoLayout 弹出悬浮框") {
  auto _ = createAppContext();

  GIVEN("frame 的 AutoLayout 纵向排列 rect1, rect2") {
    auto frame = _.drawFrame(200, 350, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(50, 100, 100, 200);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("鼠标点击 frame 上边距区域") {
      _.dispatchMouseMove(20, 20);
      _.dispatchClick(20, 20);

      THEN("设置弹出窗类型为 Top") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::Top);
      }
    }

    WHEN("鼠标点击 frame 右边距区域") {
      _.dispatchMouseMove(180, 60);
      _.dispatchClick(180, 60);

      THEN("设置弹出窗类型为 Right") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::Right);
      }
    }

    WHEN("鼠标点击 frame 下边距区域") {
      _.dispatchMouseMove(20, 330);
      _.dispatchClick(20, 330);

      THEN("设置弹出窗类型为 Bottom") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::Bottom);
      }
    }

    WHEN("鼠标点击 frame 左边距区域") {
      _.dispatchMouseMove(20, 100);
      _.dispatchClick(20, 100);

      THEN("设置弹出窗类型为 Left") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::Left);
      }
    }

    WHEN("按住 alt，鼠标点击 frame 左边距区域") {
      _.dispatchMouseMove(20, 100);
      _.dispatchClick(20, 100, {.altKey = true});

      THEN("设置弹出窗类型为 Horizontal") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::Horizontal);
      }
    }

    WHEN("按住 alt，鼠标点击 frame 上边距区域") {
      _.dispatchMouseMove(20, 20);
      _.dispatchClick(20, 20, {.altKey = true});

      THEN("设置弹出窗类型为 Vertical") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::Vertical);
      }
    }

    WHEN("按住 alt 和 shift，鼠标点击 frame 左边距区域") {
      _.dispatchMouseMove(20, 100);
      _.dispatchClick(20, 100, {.shiftKey = true, .altKey = true});

      THEN("设置弹出窗类型为 All") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::All);
      }
    }

    WHEN("鼠标点击 frame 间距区域") {
      _.dispatchMouseMove(100, 175);
      _.dispatchClick(100, 175);

      THEN("设置弹出窗类型为 Space") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::Space);
      }
    }

    WHEN("鼠标点击 frame 非边距/间距的空白处") {
      _.dispatchMouseMove(75, 210);
      _.dispatchClick(75, 210);

      THEN("设置弹出窗类型为 None") {
        REQUIRE(_.documentRoot->currentPage().getAutoLayoutSpaceInput() ==
                FloatSpaceInputType::None);
      }
    }
  }
}

SCENARIO("AutoLayout 的重叠方式") {
  auto _ = createAppContext();

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3，frame "
        "容器的重叠方式改为后遮前") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = frame.getId(), .stackReverseZIndex = true}));

    WHEN("打开画布") {
      THEN("frame 子图层顺序保持不变") {
        let &childrenHandles = frame.getChildrenHandles();
        REQUIRE(childrenHandles.size() == 3);
        REQUIRE(childrenHandles[0] == rect1.getHandle());
        REQUIRE(childrenHandles[1] == rect2.getHandle());
        REQUIRE(childrenHandles[2] == rect3.getHandle());
      }
    }

    WHEN("鼠标悬浮在间距处") {
      _.dispatchMouseMove(175, 50);

      THEN("正确展示间距区域") {
        let region = _.renderTree->getRenderItemById(ElId::AutoLayoutRegion);
        let sprite = region->getSprite<AutoLayoutRegionSprite>();
        REQUIRE(region != nullptr);
        REQUIRE(!sprite->getFillGeometry().contains(250, 100));
      }
    }

    WHEN("在 rect1 和 rect2 之间创建 rect4") {
      _.changeEditorMode(EditorMode::EM_Rectangle);
      _.dispatchClick(175, 50);
      let rect4 = _.getSelectionNodes().front();

      THEN("子元素排列变为 rect1, rect4, rect2, rect3") {
        let &childrenHandles = frame.getChildrenHandles();
        REQUIRE(childrenHandles.size() == 4);
        REQUIRE(childrenHandles[0] == rect1.getHandle());
        REQUIRE(childrenHandles[1] == rect4.getHandle());
        REQUIRE(childrenHandles[2] == rect2.getHandle());
        REQUIRE(childrenHandles[3] == rect3.getHandle());
      }
    }

    WHEN("选中 rect1，使用方向键向右移动一格") {
      _.setSelection({rect1});
      _.commandInvoker->invoke(AutoLayoutMoveChildrenCommand(
          {.type = AutoLayoutMoveChildrenType::Right, .altKey = false}));

      THEN("子元素排列变为 rect2, rect1, rect3") {
        let &childrenHandles = frame.getChildrenHandles();
        REQUIRE(childrenHandles.size() == 3);
        REQUIRE(childrenHandles[0] == rect2.getHandle());
        REQUIRE(childrenHandles[1] == rect1.getHandle());
        REQUIRE(childrenHandles[2] == rect3.getHandle());
      }
    }

    WHEN("选中 rect1，鼠标拖动 rect1 到 rect2 右侧") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(100, 100, 250, 100);

      THEN("子元素排列变为 rect2, rect1, rect3") {
        let &childrenHandles = frame.getChildrenHandles();
        REQUIRE(childrenHandles.size() == 3);
        REQUIRE(childrenHandles[0] == rect2.getHandle());
        REQUIRE(childrenHandles[1] == rect1.getHandle());
        REQUIRE(childrenHandles[2] == rect3.getHandle());
      }
    }

    WHEN("在 frame 外创建 rect4，再把 rect4 拖动到 rect1 和 rect2 中间") {
      let rect4 = _.drawRectangle(100, 100, 0, 250);
      _.dispatchDraggingEventSuite(50, 300, 150, 100);

      THEN("子元素排列变为 rect1, rect4, rect2, rect3") {
        let &childrenHandles = frame.getChildrenHandles();
        REQUIRE(childrenHandles.size() == 4);
        REQUIRE(childrenHandles[0] == rect1.getHandle());
        REQUIRE(childrenHandles[1] == rect4.getHandle());
        REQUIRE(childrenHandles[2] == rect2.getHandle());
        REQUIRE(childrenHandles[3] == rect3.getHandle());
      }
    }
  }
}

SCENARIO("AutoLayout 的子图层绝对位置和隐藏") {
  auto _ = createAppContext();

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("将 rect3 设置为绝对位置，然后调整 rect3 位置为 (400, 50)") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect3.getId(),
           .stackPositioning = StackPositioning::Absolute}));
      _.adjustLayout(rect3, {.positionX = 400});

      THEN("frame 宽度被调整为 350, rect3 位置为 (400, 50)") {
        REQUIRE(frame.getWidth() == 350);
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(400, 50));
      }
    }

    WHEN("将 rect1 设置为绝对位置") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect1.getId(),
           .stackPositioning = StackPositioning::Absolute}));

      THEN("rect1 置顶显示") {
        auto lastChild = frame.getChildrenHandles().back();
        REQUIRE(lastChild == rect1.getHandle());
      }
    }

    WHEN("[绝对位置图层和容器内图层多选] 将 rect3 设置为绝对位置，从容器外侧将 "
         "rect3 和 rect2 移动到 rect1 右侧") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect3.getId(),
           .stackPositioning = StackPositioning::Absolute}));
      _.setSelection({rect2, rect3});
      _.dispatchDraggingEventSuite(400, 100, 150, 100);

      THEN("frame 宽度为 500, rect3 被设置为自动位置") {
        REQUIRE(frame.getWidth() == 500);
        REQUIRE(rect3.getStackPositioning() == StackPositioning::Auto);
      }
    }

    WHEN("[绝对位置图层和容器内图层多选] 将 rect3 设置为绝对位置，从容器内侧将 "
         "rect3 和 rect2 移动到 rect1 左侧") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect3.getId(),
           .stackPositioning = StackPositioning::Absolute}));
      _.setSelection({rect2, rect3});
      _.dispatchDraggingEventSuite(250, 100, 100, 100);

      THEN("frame 宽度为 350, rect1 和 rect2 交换，rect3 位置不变") {
        REQUIRE(frame.getWidth() == 350);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }

    WHEN("[绝对位置图层和容器外图层多选] 将 rect3 设置为绝对位置，创建 rect4 "
         "然后将 rect3 和 rect4 拖动到 rect2 "
         "右侧") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect3.getId(),
           .stackPositioning = StackPositioning::Absolute}));
      let rect4 = _.drawRectangle(100, 100, 0, 250);
      _.setSelection({rect3, rect4});
      _.dispatchDraggingEventSuite(50, 300, 150, 100);

      THEN("frame 宽度被调整为 650, rect3 被设置为自动位置") {
        REQUIRE(frame.getWidth() == 650);
        REQUIRE(rect3.getStackPositioning() == StackPositioning::Auto);
      }
    }

    WHEN("[绝对位置图层移动到其它自动布局容器] 创建 frame2 包含自动布局，将 "
         "rect3 设置为绝对位置并移动到 frame2 "
         "中") {
      let frame2 = _.drawFrame(200, 200, 0, 300);
      _.commandInvoker->invoke(AddAutoLayoutCommand());
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = rect3.getId(),
           .stackPositioning = StackPositioning::Absolute}));
      _.dispatchDraggingEventSuite(400, 100, 50, 350);

      THEN("rect3 位置为 (0, 300)，保留绝对位置属性") {
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(0, 300));
        REQUIRE(rect3.getStackPositioning() == StackPositioning::Absolute);
      }
    }
  }

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2, rect3") {
    auto frame = _.drawFrame(500, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    auto rect3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("将 rect3 设置为隐藏") {
      rect3.setVisible(false);
      _.commandInvoker->invoke(
          ApplyAutoLayoutCommand({.nodeId = frame.getId()}));

      THEN("frame 宽度被调整为 350") { REQUIRE(frame.getWidth() == 350); }
    }

    WHEN("将 rect3 设置为隐藏，创建 rect4，将 rect4 拖到 rect3 位置") {
      rect3.setVisible(false);
      _.commandInvoker->invoke(
          ApplyAutoLayoutCommand({.nodeId = frame.getId()}));
      auto rect4 = _.drawRectangle(100, 100, 0, 250);
      _.dispatchDraggingEventSuite(50, 300, 350, 50);

      THEN("frame 宽度为 500，rect4 位于 rect3 位置") {
        REQUIRE(frame.getWidth() == 500);
        REQUIRE_THAT(rect4.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }

    WHEN("将 frame 设置为固定宽度，将 rect2 设为隐藏") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimarySizing = StackSize::StackSizeFixed}));
      rect2.setVisible(false);
      _.commandInvoker->invoke(
          ApplyAutoLayoutCommand({.nodeId = frame.getId()}));

      THEN("frame 宽度为 500，rect3 位于 rect2 位置") {
        REQUIRE(frame.getWidth() == 500);
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
      }
    }
  }
}

SCENARIO("AutoLayout 和「锁定」、「约束」") {
  auto _ = createAppContext();

  GIVEN("frame 中包含矩形 rect1, rect2, 为 frame 添加横向自动布局，设置 rect2 "
        "宽度为填充容器") {
    auto frame = _.drawFrame(200, 100, 0, 0);
    auto rect1 = _.drawRectangle(50, 50, 25, 25);
    auto rect2 = _.drawRectangle(50, 50, 125, 25);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = rect2.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("将 rect2 设置为锁定，然后拉伸 frame 右边框到 300") {
      rect2.setLocked(true);
      _.dispatchDraggingEventSuite(200, 50, 300, 50);

      THEN("rect2 宽度被修改为 150") {
        REQUIRE(rect2.getWidth() == Approx(150));
      }
    }

    WHEN("将 frame 设置为锁定，通过面板修改 frame 宽度为 300") {
      frame.setLocked(true);
      _.adjustLayout(frame, {.width = 300});

      THEN("rect2 宽度被修改为 150") {
        REQUIRE(rect2.getWidth() == Approx(150));
      }
    }
  }

  GIVEN("[约束] 自动布局容器 f1 和自动布局绝对定位子元素 r1") {
    auto f1 = _.drawFrame(100, 100, 100, 100);
    auto r1 = _.drawRectangle(50, 50, 120, 120);
    REQUIRE(r1.getParentHandle() == f1.getHandle());

    _.addAutoLayout({f1});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r1.getId(),
         .stackPositioning = StackPositioning::Absolute}));
    REQUIRE(r1.getStackPositioning() == StackPositioning::Absolute);

    WHEN("为 r1 设置右上约束，并调整 f1 的右边界") {
      r1.setConstraints(Constraints(ConstraintType::Max, ConstraintType::Min));
      auto beforeBounds = r1.getBoundsInWorld();
      auto f1Bounds = f1.getBoundsInWorld();
      _.dispatchClick(f1Bounds.right() - 10.0f, f1Bounds.top() + 10.0f);
      _.dispatchDraggingEventSuite(
          f1Bounds.right(), f1Bounds.top() + 10.0f, f1Bounds.right() + 100.0f,
          f1Bounds.top() + 10.0f); // 右边界向右拖动 100px
      REQUIRE(f1.getBoundsInWorld().left() == f1Bounds.left());
      REQUIRE(f1.getBoundsInWorld().right() == f1Bounds.right() + 100.0f);

      THEN("r1 根据约束向右调整位置") {
        auto afterBounds = r1.getBoundsInWorld();
        REQUIRE(afterBounds.right() - beforeBounds.right() == 100.0f);
      }
    }
  }

  //    GIVEN("f1 一个包含 AutoLayout 的 f2, f2 的约束为 上、右，包含 2 个矩形")
  //    {
  //        auto f1 = _.drawFrame(600, 400, 0, 0);
  //        auto f2 = _.drawFrame(350, 200, 100, 100);
  //        auto rect1 = _.drawRectangle(100, 100, 150, 150);
  //        auto rect2 = _.drawRectangle(100, 100, 300, 150);
  //
  //        f2.setConstraints(Constraints(ConstraintType::Max,
  //        ConstraintType::Min));
  //        // 为 f1 添加 AutoLayout
  //
  //        WHEN("修改 rect1 的宽度为 150") {
  //            _.adjustLayout(rect1, {.width = 150});
  //
  //            THEN("f2 的位置被调整为 (50, 100)") {
  //                //                REQUIRE_THAT(f2.getWorldTransform(),
  //                TransformMatcher::Translate(50, 100));
  //            }
  //        }
  //    }
}

SCENARIO("AutoLayout 和「横向/纵向排列切换」") {
  auto _ = createAppContext();

  GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2") {
    auto frame = _.drawFrame(350, 200, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 50, 50);
    auto rect2 = _.drawRectangle(100, 100, 200, 50);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("修改 frame 的排列方向为纵向") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(), .stackMode = StackMode::Vertical}));

      THEN("frame、rect1、rect2 的大小/位置被调整") {
        REQUIRE(frame.getWidth() == 200);
        REQUIRE(frame.getHeight() == 350);
        REQUIRE_THAT(rect1.getWorldTransform(),
                     TransformMatcher::Translate(50, 50));
        REQUIRE_THAT(rect2.getWorldTransform(),
                     TransformMatcher::Translate(50, 200));
      }
    }
  }
}

SCENARIO("AutoLayout 和「复制粘贴」") {
  auto _ = createAppContext();

  GIVEN("一个空白的 frame(200, 200, 0, 0) 和一个 rect(100, 100, 300, 0), frame "
        "宽高模式为固定") {
    auto frame = _.drawFrame(200, 200, 0, 0);
    auto rect = _.drawRectangle(100, 100, 300, 0);

    WHEN("为 frame 添加 AutoLayout，修改宽高模式为固定，将 rect 复制到 frame "
         "中") {
      _.setSelection({frame});
      _.commandInvoker->invoke(AddAutoLayoutCommand());
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackPrimarySizing = StackSize::StackSizeFixed,
           .stackCounterSizing = StackSize::StackSizeFixed}));
      _.dispatchDraggingEventSuite(350, 50, 50, 50, {.altKey = true});

      THEN("frame 大小不变，矩形位置为 (10, 10)") {
        REQUIRE(frame.getWidth() == 200);
        REQUIRE(frame.getHeight() == 200);
        let copiedRect = _.getSelectionNodes().front();
        REQUIRE_THAT(copiedRect.getWorldTransform(),
                     TransformMatcher::Translate(10, 10));
      }
    }
  }

  GIVEN("一个空白的 frame(200, 200, 0, 0) 和一个 rect(100, 100, 300, 0), 设置 "
        "rect 宽高类型为填充容器") {
    auto frame = _.drawFrame(200, 200, 0, 0);
    auto rect = _.drawRectangle(100, 100, 300, 0);

    WHEN("为 frame 添加 AutoLayout，将 rect 复制到 frame 中") {
      _.setSelection({frame});
      _.commandInvoker->invoke(AddAutoLayoutCommand());
      _.dispatchDraggingEventSuite(350, 50, 50, 50, {.altKey = true});

      THEN("frame 大小被调整为 (120, 120)，矩形位置为 (10, 10)") {
        REQUIRE(frame.getWidth() == 120);
        REQUIRE(frame.getHeight() == 120);
        let copiedRect = _.getSelectionNodes().front();
        REQUIRE_THAT(copiedRect.getWorldTransform(),
                     TransformMatcher::Translate(10, 10));
      }
    }
  }

  GIVEN("frame 横向排列 rect1, rect2， 添加 AutoLayout") {
    auto frame = _.drawFrame(250, 100, 0, 0);
    auto rect1 = _.drawRectangle(50, 50, 50, 25);
    auto rect2 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("按住 alt，将 rect1 拖动到 rect2 右侧") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(75, 50, 225, 50, {.altKey = true});

      THEN("rect1 被复制，frame 应用自动布局") {
        let rect3 = _.getSelectionNodes().front();
        REQUIRE_THAT(rect3.getWorldTransform(),
                     TransformMatcher::Translate(250, 25));
        REQUIRE(frame.getWidth() == 350);
      }
    }

    WHEN("按住 alt，将 rect1 和 rect2 拖动到 frame 中间") {
      _.setSelection({rect1, rect2});
      _.dispatchDraggingEventSuite(75, 50, 125, 50, {.altKey = true});

      THEN("rect1 和 rect2 被复制到 rect1 后，frame 应用自动布局") {
        let selection = _.getSelectionNodes();
        REQUIRE_THAT(selection.front().getWorldTransform(),
                     TransformMatcher::Translate(150, 25));
        REQUIRE_THAT(selection.back().getWorldTransform(),
                     TransformMatcher::Translate(250, 25));
        REQUIRE(frame.getWidth() == 450);
      }
    }

    WHEN("将 rect1 拖动到 frame 中间（不松手），再按下 alt") {
      _.setSelection({rect1});
      _.dispatchHalfDraggingEventSuite(75, 50, 125, 50);
      _.dispatchMouseMove(125, 50, {.altKey = true});

      THEN("被复制的 rect1 挂在 pageNode 下") {
        let selection = _.getSelectionNodes().front();
        REQUIRE(selection.getParentHandle() ==
                _.documentRoot->tryGetCurrentPage()->getHandle());
      }
      _.dispatchMouseUp(125, 50);
    }

    WHEN("将 rect1 拖动到 frame 外（不松手），再按下 alt") {
      _.setSelection({rect1});
      _.dispatchHalfDraggingEventSuite(75, 50, 75, 150);
      _.dispatchMouseMove(75, 150, {.altKey = true});

      THEN("rect1 被重新加入 frame") {
        REQUIRE(frame.getChildrenHandles().size() == 2);
      }
      _.dispatchMouseUp(75, 150);
    }
  }
}

SCENARIO("AutoLayout 和「矢量图层」") {
  auto _ = createAppContext();

  GIVEN("一个空白的 frame(200, 200, 0, 0) 包含一个 vector, vector "
        "的高度模式为填充容器") {
    auto frame = _.drawFrame(200, 200, 0, 0);
    auto vector = _.drawSimpleVector(
        {{50, 50}, {150, 50}, {150, 150}, {50, 150}, {50, 50}});
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = vector.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("调整矢量网络的元素以修改宽/高") {
      _.setSelection({vector});
      _.changeEditorMode(EditorMode::EM_Move);
      _.dispatchDraggingEventSuite(50, 50, 50, 25);
      _.dispatchDblClick(50, 50);

      THEN("vector 的高度被调整为 125，高度模式被调整为固定") {
        REQUIRE(vector.getHeight() == 125);
        REQUIRE(vector.getStackChildPrimaryGrow() == 0);
      }
    }
  }
}

SCENARIO("AutoLayout 和「编组」") {
  auto _ = createAppContext();

  GIVEN("frame 横向排列 rect1, rect2，rect1 和 rect2 编组为 group，为 frame "
        "添加横向 AutoLayout") {
    auto frame = _.drawFrame(250, 100, 0, 0);
    auto rect1 = _.drawRectangle(50, 50, 50, 25);
    auto rect2 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({rect1, rect2});
    auto group = _.groupSelection();
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = frame.getId(), .stackMode = StackMode::Horizontal}));

    WHEN("将 group 宽度修改为填充容器，然后修改 frame 宽度为 400") {
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = group.getId(), .stackChildPrimaryGrow = 1.f}));
      _.adjustLayout(frame, {.width = 400});

      THEN("group 宽度为 200，rect1 和 rect2 被拉伸") {
        REQUIRE(group.getWidth() == 300);
        REQUIRE(rect1.getWidth() == 100);
        REQUIRE(rect2.getWidth() == 100);
      }
    }
  }

  GIVEN("f1 的 AutoLayout 横向排列 r1, r2, r3") {
    auto f1 = _.drawFrame(500, 200, 0, 0);
    auto r1 = _.drawRectangle(100, 100, 50, 50);
    auto r2 = _.drawRectangle(100, 100, 200, 50);
    auto r3 = _.drawRectangle(100, 100, 350, 50);
    _.setSelection({f1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("对 r2 和 r3 添加画框") {
      _.setSelection({r2, r3});
      _.jsCommandCenter->frameSelection();

      THEN("创建 f2，r2 和 r3 的位置不变") {
        let f2 = _.getSelectionNodes().front();
        REQUIRE(f2.getWidth() == 250);
        REQUIRE(f2.getHeight() == 100);
        REQUIRE_THAT(f2.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(r2.getWorldTransform(),
                     TransformMatcher::Translate(200, 50));
        REQUIRE_THAT(r3.getWorldTransform(),
                     TransformMatcher::Translate(350, 50));
      }
    }
  }

  // GIVEN("frame 的 AutoLayout 横向排列 rect1, rect2，rect1 和 rect2
  // 的高度模式均为填充容器") {
  //     auto frame = _.drawFrame(350, 200, 0, 0);
  //     auto rect1 = _.drawRectangle(100, 100, 50, 50);
  //     auto rect2 = _.drawRectangle(100, 100, 200, 50);
  //     // frame 添加 auto layout
  //     // rect1 和 rect2 的高度模式均为填充容器
  //
  //     WHEN("将 rect1 和 rect2 编组") {
  //         _.setSelection({rect1, rect2});
  //         let group = _.groupSelection();
  //
  //         THEN("group 的高度模式为固定") {
  //             //                REQUIRE(group.getLayoutAlign() ==
  //             LayoutAlign::Inherit);
  //             //                REQUIRE(group.getLayoutGrow() == 0);
  //         }
  //     }
}

/*
SCENARIO("AutoLayout 和「缩放工具」") {
    // TODO(zhaoxz)
}
*/
SCENARIO("AutoLayout 和「文本图层」") {
  auto _ = createAppContext();

  GIVEN("frame(100, 100, 0, 0) 包含一个固定宽高的文本 (60, 30, 20, 20)，为 "
        "frame 添加 AutoLayout") {
    auto frame = _.drawFrame(100, 100, 0, 0);
    auto text = _.createText(60, 60, 20, 20, "abcd", TextAutoResize::NONE, 14);
    frame.appendChild(text);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.setSelection({text});

    WHEN("鼠标双击 text 右边界") {
      _.dispatchDblClick(80, 40);

      THEN("文本被修改为自动宽高") {
        REQUIRE(text.getTextAutoResize() == TextAutoResize::WIDTH_AND_HEIGHT);
      }

      THEN("文本的 sizeTip 展示为 '包围内容 x 包围内容'") {
        let sprite = _.getElSpriteById<SizeTipSprite>(ElId::BoundsSizetip);
        REQUIRE(sprite->getDisplayWidth() == "包围内容");
        REQUIRE(sprite->getDisplayHeight() == "包围内容");
      }
    }

    WHEN("按住 alt，鼠标双击 text 右边界") {
      _.dispatchDblClick(80, 40, {.altKey = true});

      THEN("文本的宽度模式被修改为填充容器") {
        REQUIRE(text.getStackChildAlignSelf() == StackCounterAlign::Stretch);
      }
    }

    WHEN("将文本改为自动宽高，然后按住 alt，鼠标双击 text 右边界") {
      _.dispatchDblClick(80, 40);
      _.dispatchDblClick(51, 30, {.altKey = true});

      THEN("文本的宽度模式被修改为填充容器，宽高模式为自动高度") {
        REQUIRE(text.getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE(text.getTextAutoResize() == TextAutoResize::HEIGHT);
      }
    }

    WHEN("将文本改为自动宽高，然后按住 alt，鼠标双击 text 下边界") {
      _.dispatchDblClick(80, 40);
      _.dispatchDblClick(40, 36, {.altKey = true});

      THEN("文本的高度模式被修改为填充容器，宽高模式为固定") {
        REQUIRE(text.getStackChildPrimaryGrow() == 1.f);
        REQUIRE(text.getTextAutoResize() == TextAutoResize::NONE);
      }
    }
  }

  GIVEN("frame(400, 200, 0, 0) 包含两个文本 t1(100, 100, 100, 10), t2(40, 100, "
        "250, 10)，t2 字号设为 15") {
    auto frame = _.drawFrame(400, 200, 0, 0);
    let t1 =
        _.createText(100, 100, 100, 10, "123456789012", TextAutoResize::HEIGHT);
    auto t2 = _.createText(40, 100, 250, 10, "abccabaabcbacde",
                           TextAutoResize::HEIGHT);
    _.setSelection({t2});
    _.jsCommandCenter->cmdUpdateFontSize(15);
    frame.appendChild(t1);
    frame.appendChild(t2);
    _.setSelection({frame});

    WHEN("为选中的图层添加 AutoLayout，在高级布局设置中设置 frame "
         "为文字基线对齐") {
      _.commandInvoker->invoke(AddAutoLayoutCommand());
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = frame.getId(),
           .stackCounterAlignItems = StackAlign::StackAlignBaseLine}));

      THEN("t1 和 t2 按基线对齐") {
        REQUIRE_THAT(t1.getWorldTransform(),
                     TransformMatcher::Translate(100, 11));
        REQUIRE_THAT(t2.getWorldTransform(),
                     TransformMatcher::Translate(250, 10));
      }
    }
  }

  GIVEN("[WK-7957] AutoLayout 容器 f1 包含文本 t1，设置 t1 为自动高度，t1 "
        "宽度为填充容器") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto t1 = _.createText(50, 30, 20, 20, "ABC", TextAutoResize::HEIGHT);
    f1.appendChild(t1);
    _.addAutoLayout({f1});
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = f1.getId(), .stackMode = StackMode::Horizontal}));
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = t1.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("拉伸 f1 宽度") {
      _.setSelection({f1});
      _.dispatchDraggingEventSuite(90, 50, 50, 50);

      THEN("t1 的 textAutoResize 为自动高度") {
        REQUIRE(t1.getTextAutoResize() == TextAutoResize::HEIGHT);
      }
    }
  }
}

SCENARIO("[WK-6111] UpdateParent 中的 ApplyLayout 需要 prepare layout") {
  let _ = createAppContext();

  GIVEN("有 rect r1 50x50 位于 (25,25)，rect r2 50x50 位于 (100,25)，frame f1 "
        "200x100 位于 (0,0)，将 r2 "
        "拼合为矢量图形，为 f1 增加自动布局，将 v1 的宽度设置为填充") {
    let r1 = _.drawRectangle(50, 50, 25, 25);
    let r2 = _.drawRectangle(50, 50, 100, 25);
    let f1 = _.drawFrame(200, 100, 0, 0);
    _.setSelection({r2});
    _.commandInvoker->invoke(FlattenCommand());
    auto v1 = _.getSelectionNodes()[0].as<VectorNode>();
    _.addAutoLayout({f1});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = v1.getId(), .stackChildPrimaryGrow = 1.f}));
    _.flushNodeEvents();

    WHEN("调整 r1 的宽度到 60") {
      _.adjustLayout(r1, {
                             .width = 60,
                         });

      THEN("v1 宽度变为 40") {
        auto v1Width = v1.getWidth();
        REQUIRE(v1Width == Approx(40));
      }
    }
  }
}

SCENARIO("[WK-6987] 复制最外层实例需要带上 relativeTransform") {
  let _ = createAppContext();

  auto c1 = _.drawFrameComponent(50, 50, 200, 0);
  auto i1 = _.duplicateNodes({c1})[0].as<InstanceNode>();
  auto f1 = _.drawFrame(50, 50, 0, 0);
  f1.appendChild(i1);
  _.adjustLayout(i1, {
                         .positionX = 0,
                         .positionY = 0,
                     });

  _.commandInvoker->invoke(UpdateAutoLayoutCommand({
      .nodeId = f1.getId(),
      .stackVerticalPadding = 0.f,
      .stackPaddingBottom = 0.f,
      .stackHorizontalPadding = 0.f,
      .stackPaddingRight = 0.f,
  }));
  _.flushNodeEvents();

  _.disableMouseMoveInterpolation();
  _.setSelection({f1});
  _.dispatchDraggingEventSuite(25, 25, 75, 75, {.altKey = true});

  THEN("复制出来的内部节点位于 (0,0)") {
    auto node = _.getSelectionNodes()[0].as<FrameNode>();
    auto child = node.getChildren()[0].as<InstanceNode>();
    REQUIRE_THAT(child.getRelativeTransform(),
                 TransformMatcher::Translate(0, 0));
  }
}

SCENARIO(
    "[WK-7255] 自动布局次要方向为包围内容，内部图层的次要方向全部为填充容器") {
  auto _ = createAppContext();

  GIVEN("frame 横向排列 r1, r2， 添加 AutoLayout，r1 的高度为填充容器") {
    auto frame = _.drawFrame(250, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 50, 25);
    auto r2 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({frame});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r1.getId(),
         .stackChildAlignSelf = StackCounterAlign::Stretch}));

    WHEN("删除 r2") {
      _.commandInvoker->invoke(DeleteNodeCommand(r2.getId()));

      THEN("r1 高度为填充容器，位置不变；frame 高度为包围内容，宽度不变") {
        REQUIRE(r1.getStackChildAlignSelf() == StackCounterAlign::Stretch);
        REQUIRE_THAT(r1.getRelativeTransform(),
                     TransformMatcher::Translate(50, 25));
        REQUIRE(frame.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE_THAT(frame.getRelativeTransform(),
                     TransformMatcher::Translate(0, 0));
        REQUIRE(frame.getWidth() == 250);
      }
    }
  }
}

SCENARIO(
    "[WK-7397][WK-7017] 自动布局为自动间距，包含主方向为填充容器的子图层") {
  auto _ = createAppContext();

  GIVEN("f1 横向排列 r1, r2， 添加 "
        "AutoLayout，设置间距为自动；在横向自动布局容器 f2 创建 r3，设置 r3 "
        "宽度为填充容器") {
    auto f1 = _.drawFrame(250, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 50, 25);
    auto r2 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({f1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = f1.getId(),
         .stackPrimaryAlignItems = StackJustify::StackJustifySpaceEvently}));

    auto f2 = _.drawFrame(250, 100, 0, 200);
    auto r3 = _.drawRectangle(50, 50, 50, 225);
    _.setSelection({f2});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = f2.getId(), .stackMode = StackMode::Horizontal}));
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r3.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("将 r3 拖入 f1 中") {
      _.setSelection({r3});
      _.dispatchDraggingEventSuite(75, 250, 225, 50);

      THEN("f1 宽度不变，为自动间距；r3 宽度为填充容器，位置为 (250, 25)") {
        REQUIRE(f1.getWidth() == 250);
        REQUIRE(f1.getStackPrimaryAlignItems() ==
                StackJustify::StackJustifySpaceEvently);
        REQUIRE(r3.getStackChildPrimaryGrow() == 1.f);
        REQUIRE_THAT(r3.getRelativeTransform(),
                     TransformMatcher::Translate(250, 25));
      }
    }
  }
}

SCENARIO("[WK-7376] 创建自动布局时需消除图层弹性") {
  auto _ = createAppContext();

  GIVEN("画布横向排列两个矩形 r1、r2，其中 r2 的宽高均为弹性") {
    auto r1 = _.drawRectangle(100, 100, 0, 0);
    auto r2 = _.drawRectangle(100, 100, 150, 0);
    r2.setStackChildPrimaryGrow(1.f);
    r2.setStackChildAlignSelf(StackCounterAlign::Stretch);

    WHEN("对 r1 和 r2 创建自动布局") {
      _.setSelection({r1, r2});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("r2 的弹性属性消除") {
        REQUIRE(r2.getStackChildPrimaryGrow() == 0.f);
        REQUIRE(r2.getStackChildAlignSelf() == StackCounterAlign::Auto);
      }
    }
  }
}

SCENARIO("[WK-7533] 编辑实例内嵌套两层的自动布局内的文本，容器会被撑开") {
  let _ = createAppContext();

  GIVEN("嵌套两层的自动布局实例容器，包含文本 'ABC'") {
    auto t1 =
        _.createText(50, 30, 40, 40, "ABC", TextAutoResize::WIDTH_AND_HEIGHT);
    auto f2 = _.drawFrame(100, 100, 20, 20);
    _.addAutoLayout({f2});
    auto c1 = _.drawFrameComponent(200, 200, 0, 0);
    _.addAutoLayout({c1});
    auto i1 = _.duplicateNodes({c1})[0].as<InstanceNode>();

    auto i1Width = i1.getWidth();
    auto i1ChildWidth = i1.getChildren()[0].getWidth();
    auto i1InnerTextWidth = i1.getChildren()[0].getChildren()[0].getWidth();

    WHEN("编辑实例文本为 AABBCC") {
      auto innerText = i1.getChildren()[0].getChildren()[0].as<TextNode>();
      innerText.setCharacters("AABBCC");
      _.commandInvoker->invoke(TextMeasureCommand({
          .nodeId = innerText.getId(),
      }));

      THEN("i1 与 i1 内 f1 宽度增加原文本宽度") {
        REQUIRE(i1.getWidth() == Approx(i1Width + i1InnerTextWidth));
        REQUIRE(i1.getChildren()[0].getWidth() ==
                Approx(i1ChildWidth + i1InnerTextWidth));
      }
    }
  }
}

SCENARIO("[WK-7184] 【细节逻辑补充】在容器中拖拽移动子图层顺序和将内容移入 "
         "Frame 类子图层的特殊处理") {
  let _ = createAppContext();

  GIVEN("frame1 的 AutoLayout 横向排列 frame2, rect1") {
    auto frame1 = _.drawFrame(350, 200, 0, 0);
    auto frame2 = _.drawFrame(100, 100, 50, 50);
    auto rect1 = _.drawRectangle(100, 100, 200, 50);
    _.setSelection({frame1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("鼠标拖动 rect1 到 (145, 100)，距 frame2 右边界小于 10px") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(225, 100, 145, 100);

      THEN("rect1 未被拖入 frame2 中，位置不变") {
        REQUIRE(rect1.getParent() == frame1);
        REQUIRE_THAT(rect1.getRelativeTransform(),
                     TransformMatcher::Translate(200, 50));
      }
    }

    WHEN("鼠标拖动 rect1 到 (135, 100)，距 frame2 右边界大于 10px") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(225, 100, 135, 100);

      THEN("rect1 被拖入 frame2 中") { REQUIRE(rect1.getParent() == frame2); }
    }

    WHEN("将 frame2 旋转 45°，然后鼠标拖动 rect1 到 (191, 120)，距 frame2 "
         "右边界小于 10px") {
      _.adjustLayout(frame2, {.rotationInWorld = SK_FloatPI / 4});
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(250, 120, 191, 120);

      THEN("rect1 未被拖入 frame2 中") { REQUIRE(rect1.getParent() == frame1); }
    }

    WHEN("将 frame2 旋转 45°，然后鼠标拖动 rect1 到 (180, 120)，距 frame2 "
         "右边界大于 10px") {
      _.adjustLayout(frame2, {.rotationInWorld = SK_FloatPI / 4});
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(250, 120, 180, 120);

      THEN("rect1 被拖入 frame2 中") { REQUIRE(rect1.getParent() == frame2); }
    }
  }
}

SCENARIO("[WK-7009] 拖动自动布局子图层的大小时，不应隐藏自动布局容器的虚线框") {
  auto _ = createAppContext();

  GIVEN("frame1 的 AutoLayout 横向排列 rect1") {
    auto frame1 = _.drawFrame(300, 300, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 100, 100);
    _.setSelection({frame1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("鼠标拖拽 rect1") {
      _.setSelection({rect1});
      _.dispatchHalfDraggingEventSuite(150, 150, 200, 200);

      THEN("虚线框不可见") {
        auto id = std::string(ElId::TargetSkeleton) + "-" + frame1.getId();
        auto item = _.renderTree->findRenderItemById(id);
        REQUIRE(!item->isAttached());
      }

      _.dispatchMouseUp(200, 200);
    }

    WHEN("鼠标调整 rect1 大小") {
      _.setSelection({rect1});
      _.dispatchHalfDraggingEventSuite(200, 200, 150, 150);

      THEN("虚线框可见") {
        auto id = std::string(ElId::TargetSkeleton) + "-" + frame1.getId();
        auto item = _.renderTree->findRenderItemById(id);
        REQUIRE(item->isAttached());
        REQUIRE(item->isVisible());
      }

      _.dispatchMouseUp(150, 150);
    }
  }
}

SCENARIO("[WK-7015] 因宽高锁定触发属性冲突，没有触发toast") {
  auto _ = createAppContext();

  GIVEN("frame1 的 AutoLayout 横向排列 rect1，rect1 锁定宽高") {
    auto frame1 = _.drawFrame(300, 300, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 100, 100);
    rect1.setConstrainProportions(true);
    _.setSelection({frame1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    rect1.setStackChildAlignSelf(StackCounterAlign::Stretch);
    _.flushNodeEvents();

    WHEN("鼠标调整 rect1 大小") {
      _.setSelection({rect1});
      _.dispatchDraggingEventSuite(200, 200, 150, 150);

      THEN("展示出 '矩形 1 被设定为固定宽度' 的 toast") {
        auto a = _.fakeToastService->getLatestMessage();
        REQUIRE(_.fakeToastService->getLatestMessage() ==
                "矩形 1 被设定为固定宽度");
      }
    }
  }
}

SCENARIO("[WK-6424] "
         "Group作为自动布局子图层时，移动Group的子图层时没有对齐像素网格") {
  auto _ = createAppContext();

  GIVEN("frame1 的 AutoLayout 横向排列 rect1 和 group1，group1 有 rect2 和 "
        "rect3") {
    auto frame1 = _.drawFrame(700, 300, 0, 0);
    auto rect1 = _.drawRectangle(100, 100, 100, 100);
    auto rect2 = _.drawRectangle(100, 100, 300, 100);
    auto rect3 = _.drawRectangle(100, 100, 500, 100);

    _.setSelection({rect2, rect3});
    auto group1 = _.groupSelection();

    _.setSelection({frame1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    REQUIRE(rect1.getParentId() == frame1.getId());
    REQUIRE(rect2.getParentId() == group1.getId());
    REQUIRE(rect3.getParentId() == group1.getId());
    REQUIRE(group1.getParentId() == frame1.getId());

    WHEN("拖动 rect2 使之 bottom 和 rect1 对齐") {
      _.setSelection({rect2});
      _.dispatchMouseDown(350, 150);
      _.dispatchMouseMove(350, 160);
      _.dispatchMouseMove(350, 150);

      THEN("显示参考线") {
        auto item = _.renderTree->findRenderItemById(ElId::GuideLine);
        REQUIRE(item->isAttached());
        REQUIRE(item->isVisible());
      }
    }
  }
}

SCENARIO("[WK-6801] "
         "鼠标移动时，出现了居中对齐的辅助线，但是缺少吸附感，整体感觉比较难找"
         "到对齐点") {
  auto _ = createAppContext();

  GIVEN("frame1 的 AutoLayout 横向排列，rect1 盖住 frame1") {
    auto frame1 = _.drawFrame(100, 100, 100, 100);
    _.setSelection({frame1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    auto rect1 = _.drawRectangle(100, 100, 100, 300);
    _.dispatchDraggingEventSuite(150, 380, 150, 240);

    WHEN("在 frame1 和 rect1 重合区域拖动 rect1 直至 rect1 top 和 frame1 "
         "center 重合") {
      _.dispatchHalfDraggingEventSuite(150, 180, 150, 170);

      THEN("显示参考线") {
        auto a = rect1.getWorldTransform();
        auto item = _.renderTree->findRenderItemById(ElId::GuideLine);
        REQUIRE(item->isAttached());
        REQUIRE(item->isVisible());
      }
    }
  }
}

SCENARIO("[WK-7922] 实例中的子图层不能添加绝对定位") {
  auto _ = createAppContext();

  GIVEN("AutoLayout 容器 f1 包含矩形组件 c1，f2 为空的 AutoLayout 容器") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto c1 = _.drawRectangleComponent(80, 80, 10, 10);
    _.addAutoLayout({f1});
    _.commandInvoker->invoke(UpdateAutoLayoutCommand(
        {.nodeId = f1.getId(), .stackMode = StackMode::Horizontal}));
    auto f2 = _.drawFrame(100, 100, 200, 0);
    _.addAutoLayout({f2});

    WHEN("为 c1 创建实例 i1 并添加在 f2 中，然后设置 c1 为绝对位置") {
      auto i1 = _.duplicateNodes({c1})[0].as<InstanceNode>();
      _.setSelection({i1});
      _.dispatchDraggingEventSuite(50, 50, 250, 50);
      _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
          {.nodeId = c1.getId(),
           .stackPositioning = StackPositioning::Absolute}));
      EmClock::advanceTimersByTime(2000);

      THEN("实例 i1 被设置为绝对位置") {
        REQUIRE(i1.getStackPositioning() == StackPositioning::Absolute);
      }
    }
  }
}

SCENARIO("[WK-6653] "
         "【细节补充】在画布编辑图层时，若会与之产生对齐参考线的图层也在不断变"
         "化，应该如何出对齐参考线的问题") {
  auto _ = createAppContext();

  GIVEN("frame1 的 AutoLayout 横向排列，rect1 和 rect2 紧挨着") {
    auto frame1 = _.drawFrame(400, 300, 0, 0);
    auto rect2 = _.drawRectangle(100, 100, 200, 100);
    auto rect1 = _.drawRectangle(100, 100, 100, 100);
    _.setSelection({frame1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());

    WHEN("拖动 rect1 右边框至 220") {
      _.setSelection({rect1});
      _.dispatchHalfDraggingEventSuite(200, 150, 220, 150);

      THEN("不显示参考线") {
        auto a = rect1.getWorldTransform();
        auto item = _.renderTree->findRenderItemById(ElId::GuideLine);
        REQUIRE(!item->isAttached());
      }
    }

    WHEN("拖动 rect1 右边框至 250，和 rect2 中心对齐") {
      _.setSelection({rect1});
      _.dispatchHalfDraggingEventSuite(200, 150, 250, 150);

      THEN("显示参考线") {
        auto a = rect1.getWorldTransform();
        auto item = _.renderTree->findRenderItemById(ElId::GuideLine);
        REQUIRE(item->isAttached());
        REQUIRE(item->isVisible());
      }
    }
  }
}

SCENARIO(
    "[WK-8021] 将一个包含了组件的自动布局画板创建组件时，移动的组件位置不对") {
  let _ = createAppContext();

  GIVEN("f1 的 AutoLayout 横向排列，包含矩形组件 c1 和 c2") {
    auto f1 = _.drawFrame(200, 100, 0, 0);
    auto c1 = _.drawRectangleComponent(50, 50, 50, 25);
    auto c2 = _.drawRectangleComponent(50, 50, 100, 25);
    _.addAutoLayout({f1});

    WHEN("将 f1 转为组件") {
      _.setSelection({f1});
      _.componentSelection();
      _.flushNodeEvents();

      THEN("c1 和 c2 的位置为 (270, 25)、(320, 25)") {
        REQUIRE_THAT(c1.getWorldTransform(),
                     TransformMatcher::Translate(270, 25));
        REQUIRE_THAT(c2.getWorldTransform(),
                     TransformMatcher::Translate(320, 25));
      }
    }
  }
}

SCENARIO("[WK-8070] 容器的宽度方向同时为包围容器和填充内容") {
  let _ = createAppContext();

  GIVEN("f1 的 AutoLayout 纵向排列，包含容器 f2，为 f2 添加纵向自动布局，设置 "
        "f2 宽度为包围容器和填充内容") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto f2 = _.drawFrame(50, 50, 20, 20);
    _.addAutoLayout({f1});
    _.addAutoLayout({f2});
    f2.setStackCounterSizing(StackSize::StackSizeResizeToFitWithImplicitSize);
    f2.setStackChildAlignSelf(StackCounterAlign::Stretch);
    _.flushNodeEvents();

    WHEN("调整 f2 高度（触发 layout）") {
      _.setSelection({f2});
      _.dispatchDraggingEventSuite(50, 20, 50, 30);

      THEN("f2 包围容器的弹性被消除") {
        REQUIRE(f2.getStackCounterSizing() == StackSize::StackSizeFixed);
      }
    }

    WHEN("设置 f1 为横向排列") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f1.getId(), .stackMode = StackMode::Horizontal}));

      THEN("f2 宽度模式为包围容器") {
        REQUIRE(f2.getStackCounterSizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
      }
    }
  }
}

SCENARIO("[WK-8405] 自动布局容器，调整边距/排列方向后报错") {
  let _ = createAppContext();

  GIVEN("f1 的 AutoLayout 纵向排列，包含矩形 r1，设 r1 高度为填充容器") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 25, 25);
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r1.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("重置状态，在面板修改 f1 为横向排列") {
      _.commandInvoker->invoke(ClearApplyLayoutPropertiesStateCommand());
      _.setSelection({f1});
      BatchAutoLayoutUpdatedParam param;
      param.params.push_back(
          {.nodeId = f1.getId(), .stackMode = StackMode::Horizontal});
      _.commandInvoker->invoke(UpdateBatchAutoLayoutCommand(param));

      THEN("f1 宽度为横向排列") {
        REQUIRE(f1.getStackMode() == StackMode::Horizontal);
      }
    }

    WHEN("重置状态，在面板修改 f1 上边距为 10") {
      _.commandInvoker->invoke(ClearApplyLayoutPropertiesStateCommand());
      _.setSelection({f1});
      BatchAutoLayoutUpdatedParam param;
      param.params.push_back(
          {.nodeId = f1.getId(), .stackVerticalPadding = 10});
      _.commandInvoker->invoke(UpdateBatchAutoLayoutCommand(param));

      THEN("f1 上边距为 10") { REQUIRE(f1.getStackVerticalPadding() == 10); }
    }
  }
}

SCENARIO("[WK-8432] 设置绝对位置后，图层的层级和位置不应该发生变化") {
  let _ = createAppContext();

  GIVEN("f1 的 AutoLayout 纵向排列，包含矩形 r1，容器外有矩形 r2，将 r2 复制到 "
        "f1 为 r3") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 25, 25);
    _.addAutoLayout({f1});
    auto r2 = _.drawRectangle(50, 50, 150, 150);
    _.copyNodesToClipboard({r2});
    _.setSelection({f1});
    _.pasteFromClipboard();
    auto r3 = _.getSelectionNodes().front();

    WHEN("设置粘贴的 r3 为绝对位置") {
      BatchAutoLayoutChildUpdatedParam param;
      param.params.push_back({.nodeId = r3.getId(),
                              .stackPositioning = StackPositioning::Absolute});
      _.commandInvoker->invoke(UpdateBatchAutoLayoutChildCommand(param));

      THEN("r2 被设置为绝对位置，坐标不变") {
        REQUIRE(r3.getStackPositioning() == StackPositioning::Absolute);
        REQUIRE_THAT(r3.getWorldTransform(),
                     TransformMatcher::Translate(25, 85));
      }
    }
  }
}

SCENARIO("[WK-8345] 选中实例内部图层，添加自动布局会报错") {
  let _ = createAppContext();

  GIVEN("容器 f1 包含矩形 r1，修改 f1 为组件 c1，创建 c1 的实例 "
        "i1，容器外有矩形 r2") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 25, 25);
    auto r2 = _.drawRectangle(50, 50, 110, 110);
    _.setSelection({f1});
    auto c1 = _.componentSelection();
    auto i1 = _.duplicateNodes({c1})[0].as<InstanceNode>();

    WHEN("选中 i1 中的矩形和 r2，添加自动布局") {
      _.setSelection({i1.getChildren().front(), r2});
      _.commandInvoker->invoke(AddAutoLayoutCommand());

      THEN("r2 被添加自动布局") {
        auto f2 = _.getSelectionNodes().front();
        REQUIRE(f2.getChildren().size() == 1);
        REQUIRE(f2.getChildren().front() == r2);
      }
    }
  }
}

SCENARIO("[WK-8512] 撤销添加自动布局导致报错") {
  auto _ = createAppContext();

  GIVEN("f1 横向排列 r1, r2， 添加 AutoLayout，设置 r1 宽度为填充容器") {
    auto f1 = _.drawFrame(250, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 50, 25);
    auto r2 = _.drawRectangle(50, 50, 150, 25);
    _.setSelection({f1});
    _.commandInvoker->invoke(AddAutoLayoutCommand());
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r1.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("选中 r1、r2 添加自动布局，然后 undo") {
      _.setSelection({r1, r2});
      _.commandInvoker->invoke(AddAutoLayoutCommand());
      _.undo();

      THEN("新的自动布局被撤销，r1 宽度为填充容器") {
        REQUIRE(r1.getParent().value() == f1);
        REQUIRE(r1.getStackChildPrimaryGrow() == 1.f);
      }
    }
  }
}

SCENARIO("[WK-8355][WK-8547] 自动布局的子图层弹性属性冲突") {
  auto _ = createAppContext();

  GIVEN("f1 的 AutoLayout 纵向排列 f2, f2 为纵向排列，高度设置为填充容器，f2 "
        "包含 r1") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto f2 = _.drawFrame(80, 80, 10, 10);
    auto r1 = _.drawRectangle(50, 50, 20, 20);
    _.addAutoLayout({f1});
    _.addAutoLayout({f2});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = f2.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("修改 f1 的排列方向为横向") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f1.getId(), .stackMode = StackMode::Horizontal}));

      THEN("f1 排列方向修改为横向，f2 的宽度为固定，高度为填充容器") {
        REQUIRE(f1.getStackMode() == StackMode::Horizontal);
        REQUIRE(f2.getStackChildPrimaryGrow() == 0.f);
        REQUIRE(f2.getStackChildAlignSelf() == StackCounterAlign::Stretch);
      }
    }
  }

  GIVEN("f1 的 AutoLayout 纵向排列，f2 的 AutoLayout "
        "纵向排列，主方向存在填充容器的属性") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    _.addAutoLayout({f1});

    auto f2 = _.drawFrame(80, 80, 110, 110);
    _.addAutoLayout({f2});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = f2.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("将 f2 拖动到 f1 中") {
      _.dispatchDraggingEventSuite(150, 150, 50, 50);

      THEN("f2 的高度为包围内容") {
        REQUIRE(f2.getStackPrimarySizing() ==
                StackSize::StackSizeResizeToFitWithImplicitSize);
        REQUIRE(f2.getStackChildPrimaryGrow() == 0.f);
      }
    }
  }

  GIVEN("f1 的 AutoLayout 纵向排列, 包含 r1, r1 的高度为填充容器") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 20, 20);
    _.addAutoLayout({f1});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r1.getId(), .stackChildPrimaryGrow = 1.f}));

    WHEN("取消 f1 的自动布局") {
      _.commandInvoker->invoke(UpdateAutoLayoutCommand(
          {.nodeId = f1.getId(), .stackMode = StackMode::None}));

      THEN("r1 的高度修改为固定") {
        REQUIRE(r1.getStackChildPrimaryGrow() == 0);
      }
    }
  }
}

SCENARIO("[WK-8714] 子图层拖入包含填充容器子图层的自动布局时报错") {
  auto _ = createAppContext();

  GIVEN("f1 的 AutoLayout 为纵向布局，包含 r1，r1 的宽高均为填充容器") {
    auto f1 = _.drawFrame(100, 100, 0, 0);
    auto r1 = _.drawRectangle(50, 50, 20, 20);
    _.addAutoLayout({f1});
    _.commandInvoker->invoke(UpdateAutoLayoutChildCommand(
        {.nodeId = r1.getId(),
         .stackChildPrimaryGrow = 1.f,
         .stackChildAlignSelf = StackCounterAlign::Stretch}));

    WHEN("在 f1 外创建 r2，将 r2 拖入 f1 中") {
      auto r2 = _.drawRectangle(50, 50, 120, 120);
      _.dispatchDraggingEventSuite(150, 150, 20, 10);

      THEN("r2 被添加至 f1 中") { REQUIRE(r2.getParent().value() == f1); }
    }
  }
}

} // namespace Wukong