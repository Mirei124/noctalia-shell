#pragma once

#include "wayland/layer_surface.h"

#include <algorithm>
#include <cstdint>

namespace shell::surface {

  // A rectangle expressed in one output's logical coordinate space. Glass
  // sampling deliberately never uses a desktop-global coordinate: every
  // wallpaper backdrop is stored per output.
  struct OutputLocalRect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
  };

  struct LayerSurfacePlacement {
    std::uint32_t anchor = 0;
    std::int32_t marginTop = 0;
    std::int32_t marginRight = 0;
    std::int32_t marginBottom = 0;
    std::int32_t marginLeft = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
  };

  [[nodiscard]] inline LayerSurfacePlacement placementFor(const LayerSurface& surface) noexcept {
    return LayerSurfacePlacement{
        .anchor = surface.anchor(),
        .marginTop = surface.marginTop(),
        .marginRight = surface.marginRight(),
        .marginBottom = surface.marginBottom(),
        .marginLeft = surface.marginLeft(),
        .width = surface.width(),
        .height = surface.height(),
    };
  }

  // Resolve layer-shell placement to output-local logical coordinates. A
  // layer surface anchored to neither side of an axis is centered there.
  [[nodiscard]] inline OutputLocalRect
  resolveOutputLocalRect(const LayerSurfacePlacement& placement, float outputWidth, float outputHeight) noexcept {
    const bool anchoredTop = (placement.anchor & LayerShellAnchor::Top) != 0;
    const bool anchoredBottom = (placement.anchor & LayerShellAnchor::Bottom) != 0;
    const bool anchoredLeft = (placement.anchor & LayerShellAnchor::Left) != 0;
    const bool anchoredRight = (placement.anchor & LayerShellAnchor::Right) != 0;
    const float width = static_cast<float>(placement.width);
    const float height = static_cast<float>(placement.height);

    float x = (outputWidth - width) * 0.5F;
    if (anchoredLeft) {
      x = static_cast<float>(placement.marginLeft);
    } else if (anchoredRight) {
      x = std::max(0.0F, outputWidth - static_cast<float>(placement.marginRight) - width);
    }

    float y = (outputHeight - height) * 0.5F;
    if (anchoredTop) {
      y = static_cast<float>(placement.marginTop);
    } else if (anchoredBottom) {
      y = std::max(0.0F, outputHeight - static_cast<float>(placement.marginBottom) - height);
    }

    return OutputLocalRect{.x = x, .y = y, .width = width, .height = height};
  }

  [[nodiscard]] inline OutputLocalRect
  resolveOutputLocalRect(const LayerSurface& surface, float outputWidth, float outputHeight) noexcept {
    return resolveOutputLocalRect(placementFor(surface), outputWidth, outputHeight);
  }

  [[nodiscard]] inline OutputLocalRect
  insetOutputLocalRect(OutputLocalRect rect, float left, float top, float right, float bottom) noexcept {
    rect.x += left;
    rect.y += top;
    rect.width = std::max(0.0F, rect.width - left - right);
    rect.height = std::max(0.0F, rect.height - top - bottom);
    return rect;
  }

} // namespace shell::surface
