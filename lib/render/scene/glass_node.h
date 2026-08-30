#pragma once

#include "render/core/render_styles.h"
#include "render/glass/glass_material.h"
#include "render/scene/node.h"

#include <cstdint>

class GlassNode final : public Node {
public:
  GlassNode() : Node(NodeType::Glass) {}
  [[nodiscard]] std::uint32_t outputName() const noexcept { return m_outputName; }
  [[nodiscard]] float outputX() const noexcept { return m_outputX; }
  [[nodiscard]] float outputY() const noexcept { return m_outputY; }
  [[nodiscard]] const GlassMaterial& material() const noexcept { return m_material; }
  [[nodiscard]] const CornerShapes& cornerShapes() const noexcept { return m_cornerShapes; }
  [[nodiscard]] const RectInsets& logicalInset() const noexcept { return m_logicalInset; }
  [[nodiscard]] const Radii& radii() const noexcept { return m_radii; }
  void setOutput(std::uint32_t name, float x, float y) {
    m_outputName = name;
    m_outputX = x;
    m_outputY = y;
    markPaintDirty();
  }
  void setMaterial(GlassMaterial material) {
    m_material = material;
    markPaintDirty();
  }
  void setShape(CornerShapes corners, RectInsets inset, Radii radii) {
    m_cornerShapes = corners;
    m_logicalInset = inset;
    m_radii = radii;
    markPaintDirty();
  }

private:
  std::uint32_t m_outputName = 0;
  float m_outputX = 0.0F;
  float m_outputY = 0.0F;
  GlassMaterial m_material{};
  CornerShapes m_cornerShapes{};
  RectInsets m_logicalInset{};
  Radii m_radii{16.0F};
};
