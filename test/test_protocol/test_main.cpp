#include <unity.h>

#include <optional>
#include <string>

#include "jazzlights/protocol/engine.h"

namespace jazzlights {
namespace {

class FakeDelegate : public ProtocolEngine::Delegate {
 public:
  std::string PatternName(PatternBits pattern) const override { return std::to_string(pattern); }
  std::optional<PatternBits> ForcedLeadingPattern() const override { return forcedLeadingPattern; }
  void OnPatternRestart() override { patternRestarts++; }
  void OnAcceptedUpdate() override { acceptedUpdates++; }
  void LogFpsReport() override { fpsReports++; }

  std::optional<PatternBits> forcedLeadingPattern;
  int patternRestarts = 0;
  int acceptedUpdates = 0;
  int fpsReports = 0;
};

class FakeSceneWatcher : public ProtocolEngine::OrrerySceneIdWatcher {
 public:
  void OnOrrerySceneId(std::optional<OrrerySceneId> orrerySceneId) override {
    lastScene = orrerySceneId;
    numCalls++;
  }
  std::optional<OrrerySceneId> lastScene;
  int numCalls = 0;
};

NetworkDeviceId MakeDeviceId(uint8_t lastByte) {
  uint8_t bytes[6] = {0, 0, 0, 0, 0, lastByte};
  return NetworkDeviceId(bytes);
}

// A non-reserved pattern (lowest four bits are non-zero).
constexpr PatternBits kPatternA = 0x12345671;
constexpr PatternBits kPatternB = 0x76543212;

ProtocolMessage MakeMessage(NetworkDeviceId originator, NetworkDeviceId sender, Precedence precedence,
                            Microseconds time) {
  ProtocolMessage message;
  message.originator = originator;
  message.sender = sender;
  message.precedence = precedence;
  message.currentPattern = kPatternA;
  message.nextPattern = kPatternB;
  message.currentPatternStartTime = time;
  message.lastOriginationTime = time;
  message.numHops = 0;
  message.receiptNetworkId = 1;
  message.receiptNetworkType = NetworkType::kWiFi;
  return message;
}

// Starts an engine that is leading, and reports the time its pattern rotation started.
Microseconds StartEngine(ProtocolEngine* engine, uint8_t localDeviceIdLastByte = 0x10) {
  engine->SetHasNetworks(true);
  engine->SetupDeviceId(MakeDeviceId(localDeviceIdLastByte));
  return engine->currentPatternStartTime();
}

}  // namespace

void test_compute_next_pattern() {
  // Make sure kStartingSecondPattern is correct.
  TEST_ASSERT_EQUAL_UINT32(computeNextPattern(kStartingPattern), kStartingSecondPattern);
  // The rotation never lands on a reserved pattern and never returns zero.
  PatternBits pattern = kStartingPattern;
  for (int i = 0; i < 1000; i++) {
    pattern = computeNextPattern(pattern);
    TEST_ASSERT_FALSE(patternIsReserved(pattern));
    TEST_ASSERT_NOT_EQUAL(0u, pattern);
  }
  // It is deterministic.
  TEST_ASSERT_EQUAL_UINT32(computeNextPattern(kStartingPattern), computeNextPattern(kStartingPattern));
}

void test_apply_palette() {
  for (uint8_t palette = 0; palette < 8; palette++) {
    const PatternBits pattern = applyPalette(kPatternA, palette);
    TEST_ASSERT_EQUAL_UINT8(palette, (pattern >> 13) & 0x7);
    TEST_ASSERT_FALSE(patternIsReserved(pattern));
    // Everything outside the palette bits is preserved.
    TEST_ASSERT_EQUAL_UINT32(kPatternA & 0xFFFF1FFF, pattern & 0xFFFF1FFF);
  }
}

void test_compare_precedence() {
  const NetworkDeviceId low = MakeDeviceId(0x01);
  const NetworkDeviceId high = MakeDeviceId(0x02);
  // Precedence dominates the device ID.
  TEST_ASSERT_GREATER_THAN(0, comparePrecedence(200, low, 100, high));
  TEST_ASSERT_LESS_THAN(0, comparePrecedence(100, high, 200, low));
  // Equal precedence falls back to the device ID.
  TEST_ASSERT_GREATER_THAN(0, comparePrecedence(100, high, 100, low));
  TEST_ASSERT_LESS_THAN(0, comparePrecedence(100, low, 100, high));
  TEST_ASSERT_EQUAL_INT(0, comparePrecedence(100, low, 100, low));
}

void test_precedence_gain() {
  constexpr Microseconds kDuration = 1000;
  // No epoch means no gain at all.
  TEST_ASSERT_EQUAL_UINT16(0, getPrecedenceGain(std::nullopt, 500, kDuration, 100));
  // Before the epoch, and within the first tenth of the duration, we get the full gain.
  TEST_ASSERT_EQUAL_UINT16(100, getPrecedenceGain(1000, 500, kDuration, 100));
  TEST_ASSERT_EQUAL_UINT16(100, getPrecedenceGain(500, 550, kDuration, 100));
  // Past the duration the gain is gone.
  TEST_ASSERT_EQUAL_UINT16(0, getPrecedenceGain(500, 500 + kDuration + 1, kDuration, 100));
  // Halfway through it has decayed to about half.
  TEST_ASSERT_EQUAL_UINT16(50, getPrecedenceGain(500, 1000, kDuration, 100));

  // Adding gain saturates rather than wrapping around.
  TEST_ASSERT_EQUAL_UINT16(150, addPrecedenceGain(100, 50));
  TEST_ASSERT_EQUAL_UINT16(65535, addPrecedenceGain(65500, 100));
}

void test_standalone_leading() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  engine.SetupDeviceId(MakeDeviceId(0x10));
  const Microseconds t0 = engine.currentPatternStartTime();

  // Without networks we have nothing to advertise.
  engine.SetHasNetworks(false);
  engine.CheckLeaderAndPattern(t0);
  std::optional<ProtocolMessage> message = engine.GetMessageToSend();
  TEST_ASSERT_FALSE(message);

  // With networks we advertise ourselves as the originator.
  engine.SetHasNetworks(true);
  engine.SetBasePrecedence(1000);
  engine.CheckLeaderAndPattern(t0);
  message = engine.GetMessageToSend();
  TEST_ASSERT(message);
  TEST_ASSERT(message->originator == engine.localDeviceId());
  TEST_ASSERT(message->sender == engine.localDeviceId());
  TEST_ASSERT_EQUAL_UINT8(0, message->numHops);
  TEST_ASSERT(message->receiptNetworkType == NetworkType::kLeading);
  TEST_ASSERT_EQUAL_UINT16(1000, message->precedence);
  TEST_ASSERT_EQUAL_UINT32(engine.GetCurrentPattern(), message->currentPattern);
  TEST_ASSERT(engine.currentLeader() == engine.localDeviceId());
}

void test_follow_higher_precedence() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  const NetworkDeviceId other = MakeDeviceId(0x20);

  engine.HandleReceivedMessage(MakeMessage(other, other, 100, t0), t0);
  TEST_ASSERT_EQUAL_INT(1, delegate.acceptedUpdates);
  engine.CheckLeaderAndPattern(t0);

  TEST_ASSERT(engine.currentLeader() == other);
  TEST_ASSERT_EQUAL_UINT32(kPatternA, engine.GetCurrentPattern());
  TEST_ASSERT_EQUAL_UINT32(kPatternB, engine.GetNextPattern());
  TEST_ASSERT(engine.following() == NetworkType::kWiFi);
  TEST_ASSERT_EQUAL_UINT8(1, engine.currentNumHops());
  TEST_ASSERT_EQUAL_INT(1, delegate.patternRestarts);
  TEST_ASSERT_EQUAL_INT(1, delegate.fpsReports);

  // We relay the leader's pattern onwards, attributed to the original originator.
  std::optional<ProtocolMessage> message = engine.GetMessageToSend();
  TEST_ASSERT(message);
  TEST_ASSERT(message->originator == other);
  TEST_ASSERT(message->sender == engine.localDeviceId());
  TEST_ASSERT_EQUAL_UINT8(1, message->numHops);
}

void test_ignore_lower_precedence() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  engine.SetBasePrecedence(1000);
  const NetworkDeviceId other = MakeDeviceId(0x20);
  const PatternBits ourPattern = engine.GetCurrentPattern();

  engine.HandleReceivedMessage(MakeMessage(other, other, 100, t0), t0);
  engine.CheckLeaderAndPattern(t0);

  TEST_ASSERT(engine.currentLeader() == engine.localDeviceId());
  TEST_ASSERT_EQUAL_UINT32(ourPattern, engine.GetCurrentPattern());
  TEST_ASSERT_EQUAL_INT(0, delegate.patternRestarts);
}

void test_equal_precedence_uses_device_id() {
  // A higher device ID wins a precedence tie.
  {
    FakeDelegate delegate;
    ProtocolEngine engine(&delegate);
    const Microseconds t0 = StartEngine(&engine, 0x10);
    const NetworkDeviceId higher = MakeDeviceId(0x20);
    engine.HandleReceivedMessage(MakeMessage(higher, higher, 0, t0), t0);
    engine.CheckLeaderAndPattern(t0);
    TEST_ASSERT(engine.currentLeader() == higher);
  }
  // A lower device ID loses it.
  {
    FakeDelegate delegate;
    ProtocolEngine engine(&delegate);
    const Microseconds t0 = StartEngine(&engine, 0x30);
    const NetworkDeviceId lower = MakeDeviceId(0x20);
    engine.HandleReceivedMessage(MakeMessage(lower, lower, 0, t0), t0);
    engine.CheckLeaderAndPattern(t0);
    TEST_ASSERT(engine.currentLeader() == engine.localDeviceId());
  }
}

void test_dropped_messages() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  const NetworkDeviceId us = engine.localDeviceId();
  const NetworkDeviceId other = MakeDeviceId(0x20);

  // A message we sent ourselves.
  engine.HandleReceivedMessage(MakeMessage(other, us, 100, t0), t0);
  // A message we originated ourselves.
  engine.HandleReceivedMessage(MakeMessage(us, other, 100, t0), t0);
  // A message that already went through the maximum number of hops.
  ProtocolMessage tooManyHops = MakeMessage(other, other, 100, t0);
  tooManyHops.numHops = 255;
  engine.HandleReceivedMessage(tooManyHops, t0);
  // A message whose origination time is too old.
  engine.HandleReceivedMessage(MakeMessage(other, other, 100, t0), t0 + kOriginationTimeDiscard + 1);
  // A message whose pattern started too long ago.
  ProtocolMessage staleStart = MakeMessage(other, other, 100, t0);
  staleStart.lastOriginationTime = t0 + 2 * kEffectDuration + 1;
  engine.HandleReceivedMessage(staleStart, t0 + 2 * kEffectDuration + 1);

  TEST_ASSERT_EQUAL_INT(0, delegate.acceptedUpdates);
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT(engine.currentLeader() == us);
}

void test_originator_ages_out() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  const NetworkDeviceId other = MakeDeviceId(0x20);

  engine.HandleReceivedMessage(MakeMessage(other, other, 100, t0), t0);
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT(engine.currentLeader() == other);

  // Once nothing refreshes it, the entry is discarded and we lead again.
  engine.CheckLeaderAndPattern(t0 + kOriginationTimeDiscard + 1);
  TEST_ASSERT(engine.currentLeader() == engine.localDeviceId());
  TEST_ASSERT(engine.following() == NetworkType::kLeading);
  TEST_ASSERT_EQUAL_UINT8(0, engine.currentNumHops());
}

void test_retraction() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  const NetworkDeviceId sender = MakeDeviceId(0x20);
  const NetworkDeviceId originatorA = MakeDeviceId(0x30);
  const NetworkDeviceId originatorB = MakeDeviceId(0x40);

  // Our single neighbor first tells us about a high-precedence originator.
  engine.HandleReceivedMessage(MakeMessage(originatorA, sender, 200, t0), t0);
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT(engine.currentLeader() == originatorA);

  // Then it switches to a lower-precedence one, which retracts the first.
  engine.HandleReceivedMessage(MakeMessage(originatorB, sender, 100, t0), t0);
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT(engine.currentLeader() == originatorB);
}

void test_next_hop_selection() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  const NetworkDeviceId originator = MakeDeviceId(0x30);
  const NetworkDeviceId nearSender = MakeDeviceId(0x20);
  const NetworkDeviceId farSender = MakeDeviceId(0x21);

  // Start out two hops away from the originator.
  ProtocolMessage far = MakeMessage(originator, farSender, 200, t0);
  far.numHops = 1;
  far.receiptNetworkId = 1;
  engine.HandleReceivedMessage(far, t0);
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT_EQUAL_UINT8(2, engine.currentNumHops());

  // A closer path is adopted.
  ProtocolMessage near = MakeMessage(originator, nearSender, 200, t0);
  near.numHops = 0;
  near.receiptNetworkId = 2;
  engine.HandleReceivedMessage(near, t0);
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT_EQUAL_UINT8(1, engine.currentNumHops());

  // A longer path is not, since it would risk a routing loop.
  engine.HandleReceivedMessage(far, t0);
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT_EQUAL_UINT8(1, engine.currentNumHops());

  // Unless it is substantially more recent, which lets us recover when the originator moves.
  ProtocolMessage fresherFar = MakeMessage(originator, farSender, 200, t0);
  fresherFar.numHops = 1;
  fresherFar.receiptNetworkId = 1;
  fresherFar.lastOriginationTime = t0 + kOriginationTimeOverride + 1;
  engine.HandleReceivedMessage(fresherFar, t0 + kOriginationTimeOverride + 1);
  engine.CheckLeaderAndPattern(t0 + kOriginationTimeOverride + 1);
  TEST_ASSERT_EQUAL_UINT8(2, engine.currentNumHops());
}

void test_pattern_start_time_debounce() {
  constexpr Microseconds kMs = kMicrosecondsPerMillisecond;
  const NetworkDeviceId other = MakeDeviceId(0x20);

  // A small delta is ignored outright.
  {
    FakeDelegate delegate;
    ProtocolEngine engine(&delegate);
    const Microseconds t0 = StartEngine(&engine);
    engine.HandleReceivedMessage(MakeMessage(other, other, 100, t0), t0);
    engine.CheckLeaderAndPattern(t0);
    for (int i = 0; i < 10; i++) {
      ProtocolMessage message = MakeMessage(other, other, 100, t0);
      message.currentPatternStartTime = t0 + 50 * kMs;
      engine.HandleReceivedMessage(message, t0);
      engine.CheckLeaderAndPattern(t0);
    }
    TEST_ASSERT_EQUAL_INT64(t0, engine.currentPatternStartTime());
  }

  // A large delta is applied immediately.
  {
    FakeDelegate delegate;
    ProtocolEngine engine(&delegate);
    const Microseconds t0 = StartEngine(&engine);
    engine.HandleReceivedMessage(MakeMessage(other, other, 100, t0), t0);
    engine.CheckLeaderAndPattern(t0);
    ProtocolMessage message = MakeMessage(other, other, 100, t0);
    message.currentPatternStartTime = t0 + 600 * kMs;
    engine.HandleReceivedMessage(message, t0);
    engine.CheckLeaderAndPattern(t0);
    TEST_ASSERT_EQUAL_INT64(t0 + 600 * kMs, engine.currentPatternStartTime());
  }

  // A middling delta needs to be seen five times in a row before we move.
  {
    FakeDelegate delegate;
    ProtocolEngine engine(&delegate);
    const Microseconds t0 = StartEngine(&engine);
    engine.HandleReceivedMessage(MakeMessage(other, other, 100, t0), t0);
    engine.CheckLeaderAndPattern(t0);
    for (int i = 0; i < 4; i++) {
      ProtocolMessage message = MakeMessage(other, other, 100, t0);
      message.currentPatternStartTime = t0 + 200 * kMs;
      engine.HandleReceivedMessage(message, t0);
      engine.CheckLeaderAndPattern(t0);
      TEST_ASSERT_EQUAL_INT64(t0, engine.currentPatternStartTime());
    }
    ProtocolMessage message = MakeMessage(other, other, 100, t0);
    message.currentPatternStartTime = t0 + 200 * kMs;
    engine.HandleReceivedMessage(message, t0);
    engine.CheckLeaderAndPattern(t0);
    TEST_ASSERT_EQUAL_INT64(t0 + 200 * kMs, engine.currentPatternStartTime());
  }
}

void test_leading_pattern_advance() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  const PatternBits firstPattern = engine.GetCurrentPattern();
  const PatternBits secondPattern = engine.GetNextPattern();

  // Not yet time to advance.
  engine.CheckLeaderAndPattern(t0 + kEffectDuration);
  TEST_ASSERT_EQUAL_UINT32(firstPattern, engine.GetCurrentPattern());
  TEST_ASSERT_EQUAL_INT(0, delegate.patternRestarts);

  // One effect duration later the rotation advances by exactly one step.
  engine.CheckLeaderAndPattern(t0 + kEffectDuration + 1);
  TEST_ASSERT_EQUAL_UINT32(secondPattern, engine.GetCurrentPattern());
  TEST_ASSERT_EQUAL_INT64(t0 + kEffectDuration, engine.currentPatternStartTime());
  TEST_ASSERT_EQUAL_INT(1, delegate.patternRestarts);
  TEST_ASSERT_EQUAL_INT(1, delegate.fpsReports);

  // Skipping ahead catches up one step at a time.
  engine.CheckLeaderAndPattern(t0 + 4 * kEffectDuration + 1);
  TEST_ASSERT_EQUAL_INT(4, delegate.patternRestarts);
  TEST_ASSERT_EQUAL_INT64(t0 + 4 * kEffectDuration, engine.currentPatternStartTime());
}

void test_looping_pins_the_pattern() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);

  engine.LoopOne();
  TEST_ASSERT(engine.isLooping());
  const PatternBits pinned = engine.GetCurrentPattern();
  TEST_ASSERT_EQUAL_UINT32(pinned, engine.GetNextPattern());

  engine.CheckLeaderAndPattern(t0 + 3 * kEffectDuration + 1);
  TEST_ASSERT_EQUAL_UINT32(pinned, engine.GetCurrentPattern());
  TEST_ASSERT_EQUAL_UINT32(pinned, engine.GetNextPattern());

  engine.StopLooping();
  TEST_ASSERT_FALSE(engine.isLooping());
  TEST_ASSERT_NOT_EQUAL(pinned, engine.GetNextPattern());
}

void test_forced_palette_survives_rotation() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  StartEngine(&engine);

  constexpr uint8_t kPalette = 3;
  engine.ForcePalette(kPalette);
  TEST_ASSERT(engine.forcedPalette());
  TEST_ASSERT_EQUAL_UINT8(kPalette, *engine.forcedPalette());
  TEST_ASSERT_EQUAL_UINT8(kPalette, (engine.GetCurrentPattern() >> 13) & 0x7);
  TEST_ASSERT_EQUAL_UINT8(kPalette, (engine.GetNextPattern() >> 13) & 0x7);

  // Every pattern the rotation produces keeps the forced palette.
  const Microseconds t1 = engine.currentPatternStartTime();
  for (int i = 1; i <= 5; i++) {
    engine.CheckLeaderAndPattern(t1 + i * kEffectDuration + 1);
    TEST_ASSERT_EQUAL_UINT8(kPalette, (engine.GetCurrentPattern() >> 13) & 0x7);
  }

  engine.StopForcePalette();
  TEST_ASSERT_FALSE(engine.forcedPalette());
}

void test_forced_leading_pattern() {
  FakeDelegate delegate;
  delegate.forcedLeadingPattern = kPatternA;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);

  // While leading, the delegate's pinned pattern wins and looping is forced on.
  engine.CheckLeaderAndPattern(t0);
  TEST_ASSERT_EQUAL_UINT32(kPatternA, engine.GetCurrentPattern());
  TEST_ASSERT_EQUAL_UINT32(kPatternA, engine.GetNextPattern());
  TEST_ASSERT(engine.isLooping());
}

void test_recent_user_input_keeps_us_leading() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  StartEngine(&engine);
  const NetworkDeviceId other = MakeDeviceId(0x20);

  // Pressing the button makes us sticky against ordinary originators.
  engine.GoToNextPattern();
  const Microseconds t1 = engine.currentPatternStartTime();
  engine.HandleReceivedMessage(MakeMessage(other, other, kAdminPrecedence - 1, t1), t1);
  engine.CheckLeaderAndPattern(t1);
  TEST_ASSERT(engine.currentLeader() == engine.localDeviceId());

  // But not against an admin-level one.
  engine.HandleReceivedMessage(MakeMessage(other, other, kAdminPrecedence, t1), t1);
  engine.CheckLeaderAndPattern(t1);
  TEST_ASSERT(engine.currentLeader() == other);
}

void test_orrery_scene_id() {
  FakeDelegate delegate;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);

  engine.CheckLeaderAndPattern(t0);
  std::optional<ProtocolMessage> message = engine.GetMessageToSend();
  TEST_ASSERT(message);
  TEST_ASSERT_FALSE(message->orrerySceneId.has_value());

  // Once set, the scene ID rides along on what we advertise.
  engine.SetOrrerySceneIdToSend(static_cast<OrrerySceneId>(7));
  engine.CheckLeaderAndPattern(t0);
  message = engine.GetMessageToSend();
  TEST_ASSERT(message);
  TEST_ASSERT(message->orrerySceneId.has_value());
  TEST_ASSERT_EQUAL_UINT8(7, *message->orrerySceneId);

  // Clearing it takes it back off.
  engine.SetOrrerySceneIdToSend(std::nullopt);
  engine.CheckLeaderAndPattern(t0);
  message = engine.GetMessageToSend();
  TEST_ASSERT(message);
  TEST_ASSERT_FALSE(message->orrerySceneId.has_value());
}

void test_orrery_scene_id_watcher() {
  FakeDelegate delegate;
  FakeSceneWatcher watcher;
  ProtocolEngine engine(&delegate);
  const Microseconds t0 = StartEngine(&engine);
  engine.SetOrrerySceneIdWatcher(&watcher);
  const NetworkDeviceId other = MakeDeviceId(0x20);

  ProtocolMessage message = MakeMessage(other, other, 100, t0);
  message.orrerySceneId = static_cast<OrrerySceneId>(4);
  engine.HandleReceivedMessage(message, t0);

  TEST_ASSERT_EQUAL_INT(1, watcher.numCalls);
  TEST_ASSERT(watcher.lastScene.has_value());
  TEST_ASSERT_EQUAL_UINT8(4, *watcher.lastScene);
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_compute_next_pattern);
  RUN_TEST(test_apply_palette);
  RUN_TEST(test_compare_precedence);
  RUN_TEST(test_precedence_gain);
  RUN_TEST(test_standalone_leading);
  RUN_TEST(test_follow_higher_precedence);
  RUN_TEST(test_ignore_lower_precedence);
  RUN_TEST(test_equal_precedence_uses_device_id);
  RUN_TEST(test_dropped_messages);
  RUN_TEST(test_originator_ages_out);
  RUN_TEST(test_retraction);
  RUN_TEST(test_next_hop_selection);
  RUN_TEST(test_pattern_start_time_debounce);
  RUN_TEST(test_leading_pattern_advance);
  RUN_TEST(test_looping_pins_the_pattern);
  RUN_TEST(test_forced_palette_survives_rotation);
  RUN_TEST(test_forced_leading_pattern);
  RUN_TEST(test_recent_user_input_keeps_us_leading);
  RUN_TEST(test_orrery_scene_id);
  RUN_TEST(test_orrery_scene_id_watcher);
  UNITY_END();
}

}  // namespace jazzlights

void setUp() {}

void tearDown() {}

#ifdef ESP32

void setup() { jazzlights::run_unity_tests(); }

void loop() {}

#else  // ESP32

int main(int /*argc*/, char** /*argv*/) {
  jazzlights::run_unity_tests();
  return 0;
}

#endif  // ESP32
