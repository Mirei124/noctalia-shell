#pragma once

#include "render/core/color.h"

#include <string_view>

struct GlassMaterial {
  // The default is intentionally restrained: wallpaper provides most of the
  // colour, while tint and the edge treatment only make its material legible.
  Color tint = rgba(1.0F, 1.0F, 1.0F, 0.06F);
  Color border = rgba(1.0F, 1.0F, 1.0F, 0.14F);
  float opacity = 0.95F;
  float radius = 16.0F;
  float refraction = 0.8F;
  // Kept inside the edge mask, but large enough to read on a normal-DPI
  // display when refraction strength is raised above the subtle baseline.
  float chromaticAberration = 0.20F;
  float fresnel = 0.06F;
  float noise = 0.001F;
  float borderWidth = 0.8F;

  [[nodiscard]] static GlassMaterial fromPreset(std::string_view preset, float refractionStrength = 1.0F) {
    GlassMaterial material;
    if (preset == "clear") {
      material.tint = rgba(1.0F, 1.0F, 1.0F, 0.02F);
      material.border = rgba(1.0F, 1.0F, 1.0F, 0.10F);
      material.refraction = 0.45F;
      material.chromaticAberration = 0.12F;
      material.fresnel = 0.035F;
      material.noise = 0.0F;
    } else if (preset == "tinted") {
      // macOS-like readability mode: retain a little wallpaper depth and an
      // optical rim, but let a white material layer dominate the interior.
      material.tint = rgba(1.0F, 1.0F, 1.0F, 0.68F);
      material.border = rgba(1.0F, 1.0F, 1.0F, 0.28F);
      material.refraction = 0.35F;
      material.chromaticAberration = 0.08F;
      material.fresnel = 0.045F;
      material.noise = 0.0F;
    } else if (preset == "frosted") {
      material.tint = rgba(1.0F, 1.0F, 1.0F, 0.11F);
      material.border = rgba(1.0F, 1.0F, 1.0F, 0.16F);
      material.refraction = 0.60F;
      material.chromaticAberration = 0.18F;
      material.fresnel = 0.05F;
      material.noise = 0.002F;
    } else if (preset == "bold") {
      material.tint = rgba(1.0F, 1.0F, 1.0F, 0.08F);
      material.border = rgba(1.0F, 1.0F, 1.0F, 0.18F);
      material.refraction = 1.40F;
      material.chromaticAberration = 0.45F;
      material.fresnel = 0.11F;
      material.noise = 0.0015F;
      material.borderWidth = 1.0F;
    }
    material.refraction *= refractionStrength;
    material.chromaticAberration *= refractionStrength;
    return material;
  }
};
