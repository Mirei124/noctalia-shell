#pragma once

#include "render/core/mat3.h"
#include "render/core/render_styles.h"
#include "render/core/shader_program.h"
#include "render/core/texture_handle.h"
#include "render/glass/glass_material.h"

#include <GLES2/gl2.h>

class GlassProgram {
public:
  void ensureInitialized();
  void destroy();
  void abandon() noexcept;
  void draw(
      TextureId sharp, TextureId blurred, float surfaceWidth, float surfaceHeight, float width, float height,
      float outputWidth, float outputHeight, float outputX, float outputY, bool flipY, const GlassMaterial& material,
      const CornerShapes& cornerShapes, const RectInsets& logicalInset, const Radii& radii,
      const Mat3& transform = Mat3::identity()
  ) const;

private:
  ShaderProgram m_program;
  GLint m_position = -1;
  GLint m_surfaceSize = -1;
  GLint m_rectSize = -1;
  GLint m_outputSize = -1;
  GLint m_outputOrigin = -1;
  GLint m_flipY = -1;
  GLint m_sharp = -1;
  GLint m_blurred = -1;
  GLint m_tint = -1;
  GLint m_border = -1;
  GLint m_material = -1;
  GLint m_blurMix = -1;
  GLint m_cornerShapes = -1;
  GLint m_logicalInset = -1;
  GLint m_radii = -1;
  GLint m_noise = -1;
  GLint m_opacity = -1;
  GLint m_transform = -1;
};
