#include "render/glass/glass_background_registry.h"

#include "core/log.h"

#include <unordered_map>

namespace {

  using SnapshotMap = std::unordered_map<std::uint32_t, GlassBackgroundSnapshot>;
  constexpr Logger kLog("glass-background");

  SnapshotMap& snapshots() {
    static SnapshotMap value;
    return value;
  }

} // namespace

GlassBackgroundRegistry& GlassBackgroundRegistry::instance() noexcept {
  static GlassBackgroundRegistry registry;
  return registry;
}

void GlassBackgroundRegistry::publish(std::uint32_t outputName, GlassBackgroundSnapshot snapshot) {
  if (outputName == 0 || !snapshot.valid()) {
    remove(outputName);
    return;
  }
  snapshots().insert_or_assign(outputName, snapshot);
  kLog.info(
      "published output {} (sharp={}, blur={}, {}x{} logical, {}x{} buffer)", outputName, snapshot.sharpTexture.value(),
      snapshot.blurredTexture.value(), snapshot.logicalWidth, snapshot.logicalHeight, snapshot.bufferWidth,
      snapshot.bufferHeight
  );
}

void GlassBackgroundRegistry::remove(std::uint32_t outputName) noexcept {
  if (snapshots().erase(outputName) != 0U) {
    kLog.info("removed output {}", outputName);
  }
}

void GlassBackgroundRegistry::clear() noexcept { snapshots().clear(); }

std::optional<GlassBackgroundSnapshot> GlassBackgroundRegistry::snapshotFor(std::uint32_t outputName) const {
  const auto it = snapshots().find(outputName);
  if (it == snapshots().end()) {
    return std::nullopt;
  }
  return it->second;
}
