#include "support/progression_ui.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
using namespace progression_ui;
namespace {
class SlowStore : public MemoryStore {
 public:
  uint32_t delayMs = 0;
  CommitStatus commit(const Save& save) override {
    const auto result = MemoryStore::commit(save);
    host_time_ms += delayMs;
    return result;
  }
};
void fixture(MemoryStore& store, uint16_t from = 1) {
  legacyOwned(store, {from}, from);
  Engine engine(store); check(engine.begin() == CommandResult::Applied, "evolution fixture migrates");
  const auto view = engine.evolutionView(from);
  check(view.count > 0, "fixture has an evolution");
  step(engine, from, view.options[0].cost + 500); engine.checkpoint();
}
void confirmPage(Runtime& runtime, unsigned partnerRow = 0, unsigned branch = 1) {
  openMenu(runtime); select(runtime, 2); select(runtime, partnerRow); select(runtime, branch);
  check(runtime.screen().page == Page::EvolutionConfirm, "real keys reach confirmation");
}
uint32_t confirm(Runtime& runtime) {
  buttons(true, true); frame(runtime);
  check(runtime.screen().page == Page::Evolution, "saved confirmation starts animation");
  return runtime.screen().evolutionAnim.startMs;
}
void release(Runtime& runtime) { buttons(false, false); frame(runtime); }
void at(Runtime& runtime, uint32_t time) { frame(runtime, time - host_time_ms); }
void snapshotAt(Runtime& runtime, uint32_t time, const char* name) {
  screenshot(runtime, name, time - host_time_ms);
}
void actor(uint16_t id, uint32_t origin, uint16_t silhouette = 0) {
  const auto& sprite = data::kSprites[id - 1];
  uint32_t time = (host_time_ms - origin) % sprite.duration_ms;
  unsigned frameIndex = 0;
  while (time >= sprite.frames[frameIndex].duration_ms) time -= sprite.frames[frameIndex++].duration_ms;
  unsigned scale = displayScale(sprite);
  if (scale > 3) scale = 3;
  while (scale > 1 && sprite.height * scale > 250) --scale;
  const unsigned width = sprite.width * scale, height = sprite.height * scale;
  std::vector<uint16_t> pixels(width * height);
  check(decodeFrame(sprite, frameIndex, scale, pixels.data(), pixels.size()), "source GIF frame decodes");
  unsigned ink = 0, mismatch = 0;
  for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
    const uint16_t pixel = pixels[y * width + x];
    if (pixel == kTransparentKey) continue;
    ++ink;
    if (M5.Display.readPixel(233 - int(width) / 2 + x, 233 - int(height) / 2 + y) !=
        (silhouette ? silhouette : pixel)) ++mismatch;
  }
  check(ink > 20 && mismatch == 0, "evolution actor preserves source timing and integer pixels across form changes");
}
void burstNameContrast() {
  lgfx::LGFX_Sprite name;
  name.setColorDepth(16);
  check(name.createSprite(120, 28), "name contrast reference allocates");
  name.fillScreen(TFT_BLACK); name.setTextDatum(middle_center);
  name.setFont(&fonts::efontCN_24); name.setTextColor(theme::kText);
  name.drawString("妙蛙种子", 60, 14);
  unsigned lostContrast = 0;
  for (int y = 0; y < 28; ++y) for (int x = 0; x < 120; ++x) {
    const bool text = name.readPixel(x, y) == theme::kText;
    const bool visible = M5.Display.readPixel(173 + x, 365 + y) == theme::kText;
    if (text != visible) ++lostContrast;
  }
  check(lostContrast == 0, "burst preserves the name glyphs and contrasting background");
}
void timelineAndHeldConfirm() {
  SlowStore store; fixture(store);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); confirmPage(runtime);
  const unsigned commits = store.commits;
  const uint64_t revision = runtime.engine().view().rosterRevision;
  store.delayMs = 650;
  const uint32_t before = host_time_ms, origin = confirm(runtime);
  check(origin == before + 20 + 650 && host_time_ms == origin, "slow save finishes before animation time starts");
  store.delayMs = 0;
  check(store.commits == commits + 1 && runtime.engine().view().rosterRevision == revision + 1,
        "one evolution commits before first animation frame");
  Save saved; check(store.load(saved).status == LoadStatus::Loaded && saved.partner == 2 &&
                    saved.progress[0].spent == 7500, "animation starts only after durable evolution");
  snapshotAt(runtime, origin + 200, "evolution-gather"); actor(1, origin);
  snapshotAt(runtime, origin + 600, "evolution-glow"); actor(1, origin, theme::kText);
  snapshotAt(runtime, origin + 800, "evolution-source"); actor(1, origin, theme::kText);
  snapshotAt(runtime, origin + 1000, "evolution-target"); actor(2, origin, theme::kText);
  snapshotAt(runtime, origin + 1200, "evolution-source-again"); actor(1, origin, theme::kText);
  snapshotAt(runtime, origin + 3000, "evolution-burst"); burstNameContrast();
  snapshotAt(runtime, origin + 3200, "evolution-reveal"); actor(2, origin);
  check(glyph("妙蛙草", 379, theme::kMint, 233, false, true), "reveal names the saved target");
  check(runtime.screen().evolutionAnim.startMs == origin && !runtime.dimmed(), "held confirm neither skips nor sleeps animation");
  at(runtime, origin + 4799); check(runtime.screen().page == Page::Evolution, "animation remains until final millisecond");
  at(runtime, origin + 4800);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2 && runtime.screen().evolved,
        "natural completion opens target detail");
  idle(runtime, 1000);
  check(runtime.screen().selected == 0 && store.commits == commits + 1, "held confirm cannot activate target detail");
  release(runtime); edge(runtime, false, true);
  check(runtime.screen().selected == 1, "fresh key works after completion release");
}
void naturalReturnFreshInput() {
  MemoryStore store; fixture(store);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); confirmPage(runtime);
  const uint32_t origin = confirm(runtime); release(runtime);
  at(runtime, origin + kEvolutionTotalMs);
  check(runtime.screen().page == Page::PartnerDetail, "released animation naturally returns to target detail");
  edge(runtime, false, true);
  check(runtime.screen().selected == 1, "first fresh key immediately after natural return changes detail selection");
}
void skipAndTouch() {
  MemoryStore store; fixture(store, 133);
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); confirmPage(runtime, 1, 2);
  tap(runtime, 233, 238);
  check(runtime.screen().page == Page::Evolution && runtime.screen().evolutionAnim.to == 135 &&
        evolutionFrameAt(runtime.screen().evolutionAnim, host_time_ms).stage == EvolutionStage::Gather,
        "confirmation touch release does not skip chosen Eevee evolution");
  const uint32_t origin = runtime.screen().evolutionAnim.startMs;
  frame(runtime); tap(runtime, 233, 233);
  check(runtime.screen().page == Page::Evolution &&
        evolutionFrameAt(runtime.screen().evolutionAnim, host_time_ms).stage == EvolutionStage::Reveal,
        "fresh touch skips to target reveal");
  actor(135, origin);
  screenshot(runtime, "evolution-eevee-skip", 40); actor(135, origin);
  frame(runtime); buttons(true, true); frame(runtime);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 135,
        "fresh reveal input continues to target detail");
  const unsigned commits = store.commits; idle(runtime, 1000);
  check(runtime.screen().selected == 0 && store.commits == commits, "held skip cannot select or spend again");
  release(runtime); edge(runtime, false, true);
  check(runtime.screen().selected == 1, "detail accepts input after skip release");
}
void saveFailureAndRestart() {
  for (bool written : {false, true}) {
    MemoryStore store; fixture(store);
    HostPlatform platform; Runtime runtime(store, platform); start(runtime); confirmPage(runtime);
    store.fail = true; store.writeThenFail = written;
    buttons(true, true); frame(runtime);
    check(runtime.screen().page == Page::Error && runtime.engine().view().partner == 1 &&
          runtime.engine().view().progress[0].spent == 0, "failed evolution never publishes animation or charge");
    const unsigned failedCommits = store.commits;
    idle(runtime, 1000); check(store.commits == failedCommits, "held confirmation never retries storage");
    release(runtime); select(runtime, 0);
    check(runtime.screen().page == Page::Error && store.commits == failedCommits + 1,
          "failed retry remains an error without animation");
    store.fail = store.writeThenFail = false;
    buttons(true, true); frame(runtime);
    check(runtime.screen().page == Page::Evolution && runtime.engine().view().partner == 2 &&
          runtime.engine().view().progress[0].spent == 7500, "successful retry starts original evolution exactly once");
    const uint32_t origin = runtime.screen().evolutionAnim.startMs;
    idle(runtime, 1000);
    check(runtime.screen().evolutionAnim.startMs == origin && runtime.screen().page == Page::Evolution,
          "held retry does not skip or restart animation");
    Runtime restarted(store, platform); start(restarted);
    check(restarted.screen().page == Page::Home && restarted.engine().view().partner == 2 &&
          restarted.engine().view().progress[0].spent == 7500, "restart during animation retains evolution without replay or charge");
  }
}
void stepSaveBoundaries() {
  for (bool duringAnimation : {false, true}) {
    MemoryStore store; fixture(store);
    { Engine engine(store); engine.begin(); step(engine, 1, 997); engine.checkpoint(); }
    HostPlatform platform; Runtime runtime(store, platform); start(runtime); confirmPage(runtime);
    if (duringAnimation) { confirm(runtime); release(runtime); }
    walk(runtime, platform, 2);
    check(runtime.engine().view().totalSteps == 8997, "two step peaks remain pending before threshold");
    store.fail = true;
    for (unsigned t = 0; t < 500; t += 20) {
      platform.z = 1 - 0.30f * std::cos(6.28318530718f * t / 500);
      // 确认键必须与第三个步峰的确认批同帧：观察步先失败入队、确认
      // 命令随后记录，才构成「步重试与进化收据失配」场景。EMA 系数
      // 决定谷判定的帧位（0.8 时为 t=400），调整滤波参数需同步对齐。
      if (!duringAnimation && t >= 400) buttons(true, true);
      frame(runtime);
    }
    platform.z = 1;
    check(runtime.screen().page == Page::Error && runtime.engine().savePending(),
          "mandatory step save failure takes precedence over evolution presentation");
    release(runtime); store.fail = false; select(runtime, 0);
    const auto& save = runtime.engine().view();
    check(save.totalSteps == 9000, "retry keeps all three step peaks");
    if (duringAnimation) {
      check(runtime.screen().page == Page::Home && save.partner == 2 && save.progress[0].spent == 7500 &&
            save.progress[1].walked == 3, "background retry never replays or spends saved evolution");
    } else {
      check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 1 &&
            !runtime.screen().evolved && save.partner == 1 && save.progress[0].spent == 0,
            "step retry without matching evolution receipt returns to unchanged source");
      select(runtime, 1); confirm(runtime); release(runtime);
      check(runtime.engine().view().partner == 2 && runtime.engine().view().progress[0].spent == 7500,
            "fresh confirmation after step retry can evolve normally");
    }
  }
}
void samplingAndRollover() {
  MemoryStore store; fixture(store);
  host_time_ms = UINT32_MAX - 1600;
  HostPlatform platform; Runtime runtime(store, platform); start(runtime); confirmPage(runtime);
  const uint32_t origin = confirm(runtime); release(runtime);
  const uint32_t samples = runtime.metrics().samples;
  const uint64_t total = runtime.engine().view().totalSteps;
  walk(runtime, platform, 6);
  check(host_time_ms < origin && runtime.screen().page == Page::Evolution, "runtime animation crosses millis rollover");
  check(runtime.metrics().samples >= samples + 150 && runtime.engine().view().totalSteps == total + 6 &&
        runtime.engine().view().progress[1].walked == 6, "sampling and target cultivation continue throughout animation");
  const auto& anim = runtime.screen().evolutionAnim;
  check(anim.startMs == origin, "walking does not restart evolution");
  at(runtime, origin + kEvolutionTotalMs);
  check(runtime.screen().page == Page::PartnerDetail && runtime.screen().species == 2, "rollover completes at original deadline");
}
void largeForms() {
  for (const uint16_t from : {uint16_t(17), uint16_t(129)}) {
    MemoryStore store; fixture(store, from);
    HostPlatform platform; Runtime runtime(store, platform); start(runtime); confirmPage(runtime, 1);
    const uint32_t origin = confirm(runtime); release(runtime);
    snapshotAt(runtime, origin + 200, from == 17 ? "evolution-tall-source" : "evolution-wide-source"); actor(from, origin);
    snapshotAt(runtime, origin + 3400, from == 17 ? "evolution-tall-reveal" : "evolution-wide-reveal");
    actor(runtime.engine().view().partner, origin);
    check(!runtime.metrics().displayErrors, "large form renders without decode or display failures");
  }
}
}
int main() {
  c152::begin("EVOLUTION TEST");
  timelineAndHeldConfirm(); naturalReturnFreshInput(); skipAndTouch(); saveFailureAndRestart(); stepSaveBoundaries();
  samplingAndRollover(); largeForms();
  std::printf("%u evolution Runtime/Button_Class/M5GFX checks, 0 failures\n", checks);
}
