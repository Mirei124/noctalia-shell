#include "render/programs/glass_program.h"

#include <stdexcept>

namespace {
  constexpr char kVertex[] = R"(
precision highp float;
attribute vec2 a_position;
uniform vec2 u_surface_size;
uniform vec2 u_rect_size;
uniform mat3 u_transform;
varying vec2 v_uv;
void main() {
  v_uv = a_position;
  vec3 pixel = u_transform * vec3(a_position * u_rect_size, 1.0);
  vec2 ndc = pixel.xy / u_surface_size * 2.0 - 1.0;
  gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)";
  constexpr char kFragment[] = R"(
precision highp float;
uniform sampler2D u_sharp;
uniform sampler2D u_blurred;
uniform vec2 u_rect_size;
uniform vec2 u_output_size;
uniform vec2 u_output_origin;
uniform float u_flip_y;
uniform vec4 u_tint;
uniform vec4 u_border;
uniform vec4 u_corner_shapes;
uniform vec4 u_logical_inset;
uniform vec4 u_radii;
uniform float u_noise;
uniform float u_opacity;
uniform float u_blur_mix;
// radius, refraction in logical pixels, chromatic aberration in logical pixels,
// and Fresnel edge intensity. Opacity, noise and border width are separate.
uniform vec4 u_material;
varying vec2 v_uv;

float roundedSdf(vec2 point, vec2 size, vec4 radii) {
  vec2 halfSize = size * .5;
  vec2 centered = point - halfSize;
  float radius = centered.x < 0.0 ? (centered.y < 0.0 ? radii.x : radii.w)
                                        : (centered.y < 0.0 ? radii.y : radii.z);
  vec2 q = abs(centered) - (halfSize - vec2(radius));
  return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}
float circleExtent(float radius, float delta) { return sqrt(max(0.0, radius * radius - delta * delta)); }
float shapeDistance(vec2 point, vec2 size, vec4 radii, vec4 cornerShapes, vec4 logicalInset) {
  vec4 inset = max(logicalInset, vec4(0.0));
  vec2 bodyMin = min(inset.xy, size);
  vec2 bodyMax = max(bodyMin, size - inset.zw);
  vec2 bodySize = max(bodyMax - bodyMin, vec2(0.0));
  float maxRadius = max(min(bodySize.x, bodySize.y) * .5, 0.0);
  vec4 r = clamp(radii, vec4(0.0), vec4(maxRadius));
  bool tl = cornerShapes.x > .5, tr = cornerShapes.y > .5, br = cornerShapes.z > .5, bl = cornerShapes.w > .5;
  if (!(tl || tr || br || bl)) return roundedSdf(point - bodyMin, bodySize, r);
  float x = point.x, y = point.y, left = bodyMin.x, right = bodyMax.x, top = bodyMin.y, bottom = bodyMax.y;
  float radius = r.x;
  if (radius > 0.0 && y < bodyMin.y + radius) { float extent = circleExtent(radius, clamp(y, bodyMin.y, bodyMin.y + radius) - (bodyMin.y + radius)); left = tl ? min(left, bodyMin.x - radius + extent) : max(left, bodyMin.x + radius - extent); }
  if (radius > 0.0 && x < bodyMin.x + radius) { float extent = circleExtent(radius, clamp(x, bodyMin.x, bodyMin.x + radius) - (bodyMin.x + radius)); top = tl ? min(top, bodyMin.y - radius + extent) : max(top, bodyMin.y + radius - extent); }
  radius = r.y;
  if (radius > 0.0 && y < bodyMin.y + radius) { float extent = circleExtent(radius, clamp(y, bodyMin.y, bodyMin.y + radius) - (bodyMin.y + radius)); right = tr ? max(right, bodyMax.x + radius - extent) : min(right, bodyMax.x - radius + extent); }
  if (radius > 0.0 && x > bodyMax.x - radius) { float extent = circleExtent(radius, clamp(x, bodyMax.x - radius, bodyMax.x) - (bodyMax.x - radius)); top = tr ? min(top, bodyMin.y - radius + extent) : max(top, bodyMin.y + radius - extent); }
  radius = r.z;
  if (radius > 0.0 && y > bodyMax.y - radius) { float extent = circleExtent(radius, clamp(y, bodyMax.y - radius, bodyMax.y) - (bodyMax.y - radius)); right = br ? max(right, bodyMax.x + radius - extent) : min(right, bodyMax.x - radius + extent); }
  if (radius > 0.0 && x > bodyMax.x - radius) { float extent = circleExtent(radius, clamp(x, bodyMax.x - radius, bodyMax.x) - (bodyMax.x - radius)); bottom = br ? max(bottom, bodyMax.y + radius - extent) : min(bottom, bodyMax.y - radius + extent); }
  radius = r.w;
  if (radius > 0.0 && y > bodyMax.y - radius) { float extent = circleExtent(radius, clamp(y, bodyMax.y - radius, bodyMax.y) - (bodyMax.y - radius)); left = bl ? min(left, bodyMin.x - radius + extent) : max(left, bodyMin.x + radius - extent); }
  if (radius > 0.0 && x < bodyMin.x + radius) { float extent = circleExtent(radius, clamp(x, bodyMin.x, bodyMin.x + radius) - (bodyMin.x + radius)); bottom = bl ? max(bottom, bodyMax.y + radius - extent) : min(bottom, bodyMax.y - radius + extent); }
  return max(max(left - x, x - right), max(top - y, y - bottom));
}
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
  vec2 local = v_uv * u_rect_size;
  float dist = shapeDistance(local, u_rect_size, u_radii, u_corner_shapes, u_logical_inset);
  float coverage = 1.0 - smoothstep(-1.0, 1.0, dist);
  // A finite SDF gradient follows both rounded and concave corners. The old
  // centre-to-pixel vector made the refraction look like a uniform soft-focus
  // layer instead of a thin, shape-aware glass edge.
  vec2 gradient = vec2(
      shapeDistance(local + vec2(1.0, 0.0), u_rect_size, u_radii, u_corner_shapes, u_logical_inset)
          - shapeDistance(local - vec2(1.0, 0.0), u_rect_size, u_radii, u_corner_shapes, u_logical_inset),
      shapeDistance(local + vec2(0.0, 1.0), u_rect_size, u_radii, u_corner_shapes, u_logical_inset)
          - shapeDistance(local - vec2(0.0, 1.0), u_rect_size, u_radii, u_corner_shapes, u_logical_inset)
  );
  vec2 normal = length(gradient) > 0.0001 ? normalize(gradient) : vec2(0.0);
  // Both masks must rise toward the perimeter. Reversing either one makes
  // the whole interior behave as a highlight/border, which turns the panel
  // into an opaque white rectangle.
  // The glass rim is independent of corner radius. Scale it from the short
  // side so compact bars and docks keep most of their area unwarped, while
  // large panels retain a readable edge (3–8 logical pixels).
  float edgeWidth = clamp(min(u_rect_size.x, u_rect_size.y) * 0.16, 3.0, 8.0);
  float edge = smoothstep(-edgeWidth, 0.0, dist);
  vec2 uv = (u_output_origin + local) / u_output_size;
  if (u_flip_y > 0.5) uv.y = 1.0 - uv.y;
  vec2 shift = normal * edge * u_material.y / u_output_size;
  vec2 chroma = normal * edge * u_material.z / u_output_size;
  // All channels remain in the same shared FBO. This keeps the Wayfire/Mesa
  // path reliable while giving the rim the subtle spectral split of glass.
  vec3 blurred = vec3(
      texture2D(u_blurred, uv + shift + chroma).r,
      texture2D(u_blurred, uv + shift).g,
      texture2D(u_blurred, uv + shift - chroma).b
  );
  // The blurred source alone hides sub-pixel channel separation. Take the
  // rim's detail from the matching shared sharp texture, with the same
  // in-texture RGB split. This remains safe on the Wayfire/Mesa path because
  // every channel comes from one texture for each sample.
  vec3 sharp = vec3(
      texture2D(u_sharp, uv + shift + chroma).r,
      texture2D(u_sharp, uv + shift).g,
      texture2D(u_sharp, uv + shift - chroma).b
  );
  // Blend continuously from the sharp wallpaper source to the pre-blurred
  // source. At zero, glass is genuinely sharp; at full strength, retain the
  // previous sharp rim so refraction and RGB separation stay visible.
  float blurMix = clamp(u_blur_mix, 0.0, 1.0);
  vec3 sampled = mix(sharp, blurred, blurMix * (1.0 - edge * 0.78));
  float rim = pow(clamp(edge, 0.0, 1.0), 2.0) * u_material.w;
  // Tint alpha controls only the material tint. Grain has its own much
  // smaller parameter; coupling it to tint alpha turns the panel into visible
  // per-pixel white noise.
  float grain = (hash(floor(local)) - 0.5) * u_noise;
  vec3 color = mix(sampled, u_tint.rgb, u_tint.a) + rim + grain;
  float border = smoothstep(-u_border.a, 0.0, dist) * u_border.a;
  color = mix(color, u_border.rgb, border);
  float alpha = coverage * clamp(u_opacity, 0.0, 1.0);
  gl_FragColor = vec4(color * alpha, alpha);
}
)";
} // namespace

void GlassProgram::ensureInitialized() {
  if (m_program.isValid())
    return;
  m_program.create(kVertex, kFragment);
  const auto id = m_program.id();
  m_position = glGetAttribLocation(id, "a_position");
  m_surfaceSize = glGetUniformLocation(id, "u_surface_size");
  m_rectSize = glGetUniformLocation(id, "u_rect_size");
  m_outputSize = glGetUniformLocation(id, "u_output_size");
  m_outputOrigin = glGetUniformLocation(id, "u_output_origin");
  m_flipY = glGetUniformLocation(id, "u_flip_y");
  m_sharp = glGetUniformLocation(id, "u_sharp");
  m_blurred = glGetUniformLocation(id, "u_blurred");
  m_tint = glGetUniformLocation(id, "u_tint");
  m_border = glGetUniformLocation(id, "u_border");
  m_material = glGetUniformLocation(id, "u_material");
  m_transform = glGetUniformLocation(id, "u_transform");
  m_cornerShapes = glGetUniformLocation(id, "u_corner_shapes");
  m_logicalInset = glGetUniformLocation(id, "u_logical_inset");
  m_radii = glGetUniformLocation(id, "u_radii");
  m_noise = glGetUniformLocation(id, "u_noise");
  m_opacity = glGetUniformLocation(id, "u_opacity");
  m_blurMix = glGetUniformLocation(id, "u_blur_mix");
  // GLSL compilers may eliminate a material uniform when an effect is disabled.
  // Geometry and wallpaper samplers are the required baseline for a Glass draw.
  if (m_position < 0
      || m_surfaceSize < 0
      || m_rectSize < 0
      || m_outputSize < 0
      || m_outputOrigin < 0
      || m_flipY < 0
      || m_sharp < 0
      || m_blurred < 0
      || m_transform < 0
      || m_cornerShapes < 0
      || m_logicalInset < 0
      || m_radii < 0
      || m_noise < 0
      || m_opacity < 0
      || m_blurMix < 0)
    throw std::runtime_error("failed to query required glass shader locations");
}
void GlassProgram::destroy() { m_program.destroy(); }
void GlassProgram::abandon() noexcept { m_program.abandon(); }
void GlassProgram::draw(
    TextureId sharp, TextureId blurred, float sw, float sh, float w, float h, float ow, float oh, float ox, float oy,
    bool flipY, const GlassMaterial& m, const CornerShapes& corners, const RectInsets& inset, const Radii& radii,
    const Mat3& transform
) const {
  if (!m_program.isValid() || sharp == TextureId{} || blurred == TextureId{} || w <= 0 || h <= 0 || ow <= 0 || oh <= 0)
    return;
  static constexpr float quad[] = {0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1};
  glUseProgram(m_program.id());
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(sharp.value()));
  glUniform1i(m_sharp, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(blurred.value()));
  glUniform1i(m_blurred, 1);
  glUniform2f(m_surfaceSize, sw, sh);
  glUniform2f(m_rectSize, w, h);
  glUniform2f(m_outputSize, ow, oh);
  glUniform2f(m_outputOrigin, ox, oy);
  glUniform1f(m_flipY, flipY ? 1.F : 0.F);
  glUniform4f(m_tint, m.tint.r, m.tint.g, m.tint.b, m.tint.a);
  glUniform4f(m_border, m.border.r, m.border.g, m.border.b, m.borderWidth);
  glUniform1f(m_noise, m.noise);
  glUniform1f(m_opacity, m.opacity);
  glUniform1f(m_blurMix, m.blurMix);
  glUniform4f(m_material, m.radius, m.refraction, m.chromaticAberration, m.fresnel);
  glUniformMatrix3fv(m_transform, 1, GL_FALSE, transform.m.data());
  const auto concave = [](CornerShape shape) { return shape == CornerShape::Concave ? 1.0F : 0.0F; };
  glUniform4f(m_cornerShapes, concave(corners.tl), concave(corners.tr), concave(corners.br), concave(corners.bl));
  glUniform4f(m_logicalInset, inset.left, inset.top, inset.right, inset.bottom);
  glUniform4f(m_radii, radii.tl, radii.tr, radii.br, radii.bl);
  glVertexAttribPointer(static_cast<GLuint>(m_position), 2, GL_FLOAT, GL_FALSE, 0, quad);
  glEnableVertexAttribArray(static_cast<GLuint>(m_position));
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisableVertexAttribArray(static_cast<GLuint>(m_position));
  glActiveTexture(GL_TEXTURE0);
}
