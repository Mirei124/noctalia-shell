#include "shell/backdrop/backdrop_surface.h"

#include "render/backend/render_backend.h"
#include "render/glass/glass_background_registry.h"
#include "wayland/wayland_connection.h"

#include <algorithm>
#include <stdexcept>
#include <wayland-client-protocol.h>

BackdropSurface::~BackdropSurface() {
  GlassBackgroundRegistry::instance().remove(m_outputName);
  m_wallpaperRenderer.makeCurrent();
  m_glassBlurLayer.destroy();
  m_glassSharpLayer.destroy();
  m_layer.destroy();
}

bool BackdropSurface::createWlSurface() {
  m_surface = wl_compositor_create_surface(m_connection.compositor());
  if (m_surface == nullptr) {
    return false;
  }

  initializeSurfaceScaleProtocol();

  if (m_shared == nullptr) {
    throw std::runtime_error("BackdropSurface requires a GlSharedContext");
  }
  m_wallpaperRenderer.bind(*m_shared, m_surface);
  return true;
}

void BackdropSurface::onConfigure(std::uint32_t width, std::uint32_t height) {
  const auto bw = bufferWidthFor(width);
  const auto bh = bufferHeightFor(height);

  m_bufW = bw;
  m_bufH = bh;

  m_wallpaperRenderer.resize(bw, bh, width, height);
  m_layer.invalidate();

  Surface::onConfigure(width, height);
}

void BackdropSurface::onScaleChanged() {
  if (width() == 0 || height() == 0) {
    return;
  }
  onConfigure(width(), height());
}

void BackdropSurface::render() {
  auto* backend = m_wallpaperRenderer.backend();
  if (m_surface == nullptr || backend == nullptr) {
    return;
  }

  m_wallpaperRenderer.makeCurrent();
  m_layer.resize(*backend, m_bufW, m_bufH);

  if (!m_layer.valid()) {
    return;
  }

  static constexpr int kBlurRounds = 3;
  const auto options = BackdropPostProcessOptions{
      .blurRadius = m_blurIntensity * 40.0F,
      .blurRounds = kBlurRounds,
      .tintColor = rgba(m_tintR, m_tintG, m_tintB, 1.0F),
      .tintIntensity = m_tintIntensity,
  };
  // Glass owns the blur/tint treatment of its own sampled texture. Do not
  // post-process the full-screen backdrop as well, or the desktop outside
  // every glass surface would become softly blurred and tinted.
  const auto baseOptions = m_glassEnabled ? BackdropPostProcessOptions{} : options;

  if (!m_layer.dirty()) {
    return;
  }

  m_layer.ensure([&](RenderFramebuffer& target) {
    auto* scratch = m_layer.scratch();
    if (scratch == nullptr) {
      return;
    }
    m_wallpaperRenderer.renderBackdropContent(target, *scratch, baseOptions);
  });

  if (m_glassEnabled) {
    m_glassSharpLayer.resize(*backend, m_bufW, m_bufH);
    m_glassBlurLayer.resize(*backend, m_bufW, m_bufH);

    m_glassSharpLayer.ensure([&](RenderFramebuffer& target) {
      // No post-processing is requested, so the scratch parameter is unused.
      // Reusing target avoids allocating a second full-resolution framebuffer.
      m_wallpaperRenderer.renderBackdropContent(target, target, {});
    });
    m_glassBlurLayer.ensure([&](RenderFramebuffer& target) {
      auto* scratch = m_glassBlurLayer.scratch();
      if (scratch != nullptr) {
        m_wallpaperRenderer.renderBackdropContent(
            target, *scratch,
            BackdropPostProcessOptions{
                // Independent from [backdrop]: this radius only affects the
                // wallpaper texture sampled by glass surfaces.
                .blurRadius = m_glassBlurIntensity * 20.0F,
                .blurRounds = options.blurRounds,
            }
        );
      }
    });

    GlassBackgroundRegistry::instance().publish(
        m_outputName,
        GlassBackgroundSnapshot{
            .sharpTexture = m_glassSharpLayer.texture(),
            .blurredTexture = m_glassBlurLayer.texture(),
            .bufferWidth = m_bufW,
            .bufferHeight = m_bufH,
            .logicalWidth = width(),
            .logicalHeight = height(),
            .flipY = true,
        }
    );
  } else {
    GlassBackgroundRegistry::instance().remove(m_outputName);
  }

  requestFrame();
  m_wallpaperRenderer.presentTexture(m_layer.texture());
}

void BackdropSurface::setBlurIntensity(float v) noexcept {
  if (m_blurIntensity == v) {
    return;
  }
  m_blurIntensity = v;
  m_layer.invalidate();
  m_glassBlurLayer.invalidate();
}

void BackdropSurface::setGlassBlurIntensity(float v) noexcept {
  if (m_glassBlurIntensity == v) {
    return;
  }
  m_glassBlurIntensity = v;
  // render() returns early when the presentation layer is clean, so mark it
  // too; it remains an unprocessed wallpaper when glass is active.
  m_layer.invalidate();
  m_glassBlurLayer.invalidate();
}

void BackdropSurface::setTintIntensity(float v) noexcept {
  if (m_tintIntensity == v) {
    return;
  }
  m_tintIntensity = v;
  m_layer.invalidate();
}

void BackdropSurface::setTintColor(float r, float g, float b) noexcept {
  if (m_tintR == r && m_tintG == g && m_tintB == b) {
    return;
  }
  m_tintR = r;
  m_tintG = g;
  m_tintB = b;
  m_layer.invalidate();
}

void BackdropSurface::setWallpaperState(TextureId tex, float imgW, float imgH, WallpaperFillMode fillMode) {
  m_wallpaperRenderer.setTransitionState(
      tex, {}, imgW, imgH, 0.0F, 0.0F, 0.0F, WallpaperTransition::Fade, fillMode, TransitionParams{}
  );
  m_layer.invalidate();
  m_glassSharpLayer.invalidate();
  m_glassBlurLayer.invalidate();
}

void BackdropSurface::setGlassEnabled(bool enabled) noexcept {
  if (m_glassEnabled == enabled) {
    return;
  }
  m_glassEnabled = enabled;
  if (!enabled) {
    GlassBackgroundRegistry::instance().remove(m_outputName);
    m_wallpaperRenderer.makeCurrent();
    m_glassSharpLayer.destroy();
    m_glassBlurLayer.destroy();
  } else {
    m_glassSharpLayer.invalidate();
    m_glassBlurLayer.invalidate();
  }
  m_layer.invalidate();
}

void BackdropSurface::onGpuResourcesInvalidated() {
  GlassBackgroundRegistry::instance().remove(m_outputName);
  m_wallpaperRenderer.invalidateGpuResources();
  m_glassBlurLayer.destroy();
  m_glassSharpLayer.destroy();
  m_layer.destroy();
  requestRedraw();
}

void BackdropSurface::prepareForGraphicsReset() noexcept {
  GlassBackgroundRegistry::instance().remove(m_outputName);
  m_glassBlurLayer.abandon();
  m_glassSharpLayer.abandon();
  m_layer.abandon();
  m_wallpaperRenderer.prepareForGraphicsReset();
}

void BackdropSurface::restoreAfterGraphicsReset() {
  if (m_shared == nullptr) {
    throw std::runtime_error("BackdropSurface requires a GlSharedContext");
  }
  m_wallpaperRenderer.restoreAfterGraphicsReset(*m_shared);
  m_layer.invalidate();
  m_glassSharpLayer.invalidate();
  m_glassBlurLayer.invalidate();
}

void BackdropSurface::finishGraphicsResetRecovery() noexcept { m_wallpaperRenderer.finishGraphicsResetRecovery(); }
