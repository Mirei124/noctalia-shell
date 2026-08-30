#pragma once

#include "render/core/texture_handle.h"

#include <cstdint>
#include <optional>

// Output-local wallpaper-derived texture metadata shared by glass consumers.
// All Noctalia render contexts belong to the same EGL share group, so the
// texture is safe to sample from another shell surface while its publisher is
// alive. The texture is intentionally a wallpaper render target, never a
// compositor or screen capture.
struct GlassBackgroundSnapshot {
  TextureId sharpTexture;
  TextureId blurredTexture;
  std::uint32_t bufferWidth = 0;
  std::uint32_t bufferHeight = 0;
  std::uint32_t logicalWidth = 0;
  std::uint32_t logicalHeight = 0;
  bool flipY = true;

  [[nodiscard]] bool valid() const noexcept {
    return sharpTexture != TextureId{}
    && blurredTexture != TextureId{}
    && bufferWidth != 0
        && bufferHeight != 0
        && logicalWidth != 0
        && logicalHeight != 0;
  }
};

// This registry carries handles only; it neither owns nor creates GPU
// resources. BackdropSurface remains responsible for rendering and destroying
// the cached wallpaper texture. Shell rendering runs on the UI thread, so the
// registry deliberately has no locking or cross-thread ownership semantics.
class GlassBackgroundRegistry {
public:
  static GlassBackgroundRegistry& instance() noexcept;

  void publish(std::uint32_t outputName, GlassBackgroundSnapshot snapshot);
  void remove(std::uint32_t outputName) noexcept;
  void clear() noexcept;

  [[nodiscard]] std::optional<GlassBackgroundSnapshot> snapshotFor(std::uint32_t outputName) const;

private:
  GlassBackgroundRegistry() = default;
};
