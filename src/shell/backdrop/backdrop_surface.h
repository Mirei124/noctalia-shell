#pragma once

#include "render/core/cached_layer.h"
#include "render/wallpaper_renderer.h"
#include "wayland/layer_surface.h"

#include <cstdint>

class GlSharedContext;

class BackdropSurface : public LayerSurface {
public:
  using LayerSurface::LayerSurface;
  ~BackdropSurface() override;

  void setSharedGl(GlSharedContext* shared) noexcept { m_shared = shared; }
  void setOutputName(std::uint32_t outputName) noexcept { m_outputName = outputName; }
  void setGlassEnabled(bool enabled) noexcept;
  void setBlurIntensity(float v) noexcept;
  void setTintIntensity(float v) noexcept;
  void setTintColor(float r, float g, float b) noexcept;
  void setWallpaperState(TextureId tex, float imgW, float imgH, WallpaperFillMode fillMode);
  void onGpuResourcesInvalidated();
  void prepareForGraphicsReset() noexcept;
  void restoreAfterGraphicsReset();
  void finishGraphicsResetRecovery() noexcept;

  [[nodiscard]] WallpaperRenderer* wallpaperRenderer() noexcept { return &m_wallpaperRenderer; }

protected:
  bool createWlSurface() override;
  void onConfigure(std::uint32_t width, std::uint32_t height) override;
  void render() override;
  void onScaleChanged() override;

private:
  WallpaperRenderer m_wallpaperRenderer;
  CachedLayer m_layer;
  CachedLayer m_glassSharpLayer;
  CachedLayer m_glassBlurLayer;

  std::uint32_t m_bufW = 0;
  std::uint32_t m_bufH = 0;

  GlSharedContext* m_shared = nullptr;
  std::uint32_t m_outputName = 0;
  bool m_glassEnabled = false;
  float m_blurIntensity = 0.5F;
  float m_tintIntensity = 0.3F;
  float m_tintR = 0.0F;
  float m_tintG = 0.0F;
  float m_tintB = 0.0F;
};
