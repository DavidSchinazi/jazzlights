#ifndef JL_PROTOCOL_ENGINE_H
#define JL_PROTOCOL_ENGINE_H

#include <list>
#include <optional>
#include <string>

#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/config.h"
#include "jazzlights/util/time.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

// This value was intentionally selected by brute-forcing all possible values that start with rings-rainbow followed by
// flame-heat and then sp-cloud, and then picking the one that will loop after the most iterations. This one loops after
// 118284 iterations, which is more than 13 days.
inline constexpr PatternBits kStartingPattern = 0x00b3db69;
inline constexpr PatternBits kStartingSecondPattern = 0x7d000629;

inline constexpr Microseconds kInputDuration = 10 * 60 * kMicrosecondsPerSecond;  // 10min.

#if JL_IS_CONFIG(XMAS_TREE)
inline constexpr Precedence kAdminPrecedence = 6001;
#else   // XMAS_TREE
inline constexpr Precedence kAdminPrecedence = 60000;
#endif  // XMAS_TREE

inline constexpr Microseconds kOriginationTimeOverride = 6 * kMicrosecondsPerSecond;
inline constexpr Microseconds kOriginationTimeDiscard = 9 * kMicrosecondsPerSecond;

static_assert(kOriginationTimeOverride < kOriginationTimeDiscard,
              "Inverting these can lead to retracting an originator "
              "while disallowing picking a replacement.");
static_assert(kOriginationTimeDiscard < kEffectDuration,
              "Inverting these can lead to keeping an originator "
              "past the end of its intended next pattern.");

// Compares two (precedence, deviceId) pairs. Returns a negative number if left sorts before right, a positive number
// if it sorts after, and zero if they are equal.
int comparePrecedence(Precedence leftPrecedence, const NetworkDeviceId& leftDeviceId, Precedence rightPrecedence,
                      const NetworkDeviceId& rightDeviceId);

// Patterns with lowest 4 bits set to zero are reserved.
constexpr bool patternIsReserved(PatternBits pattern) { return (pattern & 0xF) == 0; }

PatternBits computeNextPattern(PatternBits pattern);
PatternBits applyPalette(PatternBits pattern, uint8_t palette);

Precedence getPrecedenceGain(OptionalMicroseconds epochTime, Microseconds currentTime, Microseconds duration,
                             Precedence maxGain);
Precedence addPrecedenceGain(Precedence startPrecedence, Precedence gain);

// ProtocolEngine implements the JazzLights synchronization protocol: leader election, the originator table, and the
// synchronized pattern rotation. It is a pure state machine that owns no transport and performs no rendering.
// Received messages are pushed in with HandleReceivedMessage() and the message to advertise is pulled out with
// GetMessageToSend(); the owner (Player) is responsible for all transport plumbing.
class ProtocolEngine {
 public:
  // Implemented by the renderer. Every call happens synchronously from within a ProtocolEngine method, and the
  // delegate must outlive the ProtocolEngine.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Human-readable name for this pattern. Used in log lines only.
    virtual std::string PatternName(PatternBits pattern) const = 0;
    // The pattern this device must display whenever it is leading, or nullopt to use the regular rotation. Used by
    // the configs that pin a single pattern.
    virtual std::optional<PatternBits> ForcedLeadingPattern() const = 0;
    // The synchronized pattern changed or restarted: the renderer must begin the effect anew and must not rate-limit
    // its next write to the LEDs.
    virtual void OnPatternRestart() = 0;
    // A network update was accepted: the renderer must not rate-limit its next write to the LEDs.
    virtual void OnAcceptedUpdate() = 0;
    // Called right after a pattern change is logged, so the renderer can log an adjacent FPS report.
    virtual void LogFpsReport() = 0;
#if JL_IS_CONFIG(CREATURE)
    virtual bool IsPartying() const = 0;
    virtual uint32_t CreatureColor() const = 0;
    virtual void OnCreatureHeard(uint32_t creatureColor, Microseconds heardTime, int rssi, bool isPartying) = 0;
    virtual void OnOrreryHeard() = 0;
#endif  // CREATURE
  };

  class OrrerySceneIdWatcher {
   public:
    virtual ~OrrerySceneIdWatcher() = default;
    virtual void OnOrrerySceneId(std::optional<OrrerySceneId> orrerySceneId) = 0;
  };

#if JL_IS_CONFIG(ORRERY_LEADER)
  class OverriddenPatternWatcher {
   public:
    virtual ~OverriddenPatternWatcher() = default;
    virtual void OnOverriddenPattern(std::optional<PatternBits> pattern) = 0;
  };
#endif  // ORRERY_LEADER

  explicit ProtocolEngine(Delegate* delegate) : delegate_(delegate) {}

  // Disallow copy.
  ProtocolEngine(const ProtocolEngine&) = delete;
  ProtocolEngine& operator=(const ProtocolEngine&) = delete;

  // Sets up our local device ID. `localDeviceIdFromNetworks` is the device ID reported by the transports, or a
  // default-constructed NetworkDeviceId if none of them has one, in which case we generate one randomly.
  void SetupDeviceId(NetworkDeviceId localDeviceIdFromNetworks);
  // Whether any transport is connected. While false we do not advertise at all.
  void SetHasNetworks(bool hasNetworks) { hasNetworks_ = hasNetworks; }

  void HandleReceivedMessage(NetworkMessage message, OptionalMicroseconds currentTimeOpt = std::nullopt);

  // Ages out originators, elects a leader, advances the synchronized pattern, and recomputes the message to
  // advertise. Called once per frame and after every user command.
  void CheckLeaderAndPattern(OptionalMicroseconds currentTimeOpt = std::nullopt);

  // Returns the message that should currently be advertised on every transport, or false if there is nothing to
  // advertise. This does not consume the message: it returns whatever the last CheckLeaderAndPattern() computed.
  bool GetMessageToSend(NetworkMessage* messageToSend) const;

  // Synchronized state, read by the renderer.
  PatternBits GetCurrentPattern() const { return currentPattern_; }
  PatternBits GetNextPattern() const { return nextPattern_; }
  Microseconds currentPatternStartTime() const { return currentPatternStartTime_; }
  bool isLooping() const { return loop_; }
  std::optional<uint8_t> forcedPalette() const { return forcedPalette_; }
  NetworkDeviceId localDeviceId() const { return localDeviceId_; }
  NetworkDeviceId currentLeader() const { return currentLeader_; }
  NetworkType following() const { return followedNextHopNetworkType_; }
  NetworkId followedNextHopNetworkId() const { return followedNextHopNetworkId_; }
  NumHops currentNumHops() const { return currentNumHops_; }
  Precedence basePrecedence() const { return basePrecedence_; }
  Precedence precedenceGain() const { return precedenceGain_; }
#if JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(ORRERY_PLANET)
  bool creatureIsFollowingNonCreature() const { return creatureIsFollowingNonCreature_; }
#endif  // CREATURE || ORRERY_PLANET

  // Play the next pattern in the rotation. Counts as user input, and runs CheckLeaderAndPattern() internally.
  void GoToNextPattern();
  // Jump to this pattern. Counts as user input, and runs CheckLeaderAndPattern() internally.
  void SetPattern(PatternBits pattern);
  void LoopOne();
  void StopLooping();
  // Sets the loop flag without touching the patterns. Only intended for startup configuration.
  void SetLooping(bool loop) { loop_ = loop; }
  // Pins both the current and next pattern and enables looping. Does not count as user input and does not run
  // CheckLeaderAndPattern(). Used for special modes and for startup pattern seeding.
  void SetPatternAndLoop(PatternBits pattern);
  // Leaves a pinned pattern and resumes the regular rotation.
  void ResumeRotation();
  void ForcePalette(uint8_t palette);
  void StopForcePalette();

  void SetBasePrecedence(Precedence basePrecedence) { basePrecedence_ = basePrecedence; }
  void SetPrecedenceGain(Precedence precedenceGain) { precedenceGain_ = precedenceGain; }
  // Returns whether anything actually changed, in which case the caller should re-advertise.
  bool UpdatePrecedence(Precedence basePrecedence, Precedence precedenceGain);
  void SetRandomizeLocalDeviceId(bool val) { randomizeLocalDeviceId_ = val; }

  void SetOrrerySceneIdToSend(std::optional<OrrerySceneId> orrerySceneIdToSend);
  void SetOrrerySceneIdWatcher(OrrerySceneIdWatcher* orrerySceneIdWatcher) {
    orrerySceneIdWatcher_ = orrerySceneIdWatcher;
  }
#if JL_IS_CONFIG(ORRERY_LEADER)
  void SetOverriddenPatternWatcher(OverriddenPatternWatcher* overriddenPatternWatcher) {
    overriddenPatternWatcher_ = overriddenPatternWatcher;
  }
#endif  // ORRERY_LEADER

#if JL_IS_CONFIG(CLOUDS)
  void ClearUserInputTime() { lastUserInputTime_.reset(); }
  // Re-applies the forced palette to the current pattern and recomputes the next one.
  void ReapplyForcedPalette();
  // Advances the rotation. `extraAdvance` skips one extra pattern, used when leaving forced-clouds mode. Counts as
  // user input, and runs CheckLeaderAndPattern() internally.
  void CloudNext(bool extraAdvance);
#endif  // CLOUDS

 private:
  struct OriginatorEntry {
    NetworkDeviceId originator = NetworkDeviceId();
    Precedence precedence = 0;
    PatternBits currentPattern = 0;
    PatternBits nextPattern = 0;
    Microseconds currentPatternStartTime = 0;
    Microseconds lastOriginationTime = 0;
    NetworkDeviceId nextHopDevice = NetworkDeviceId();
    NetworkId nextHopNetworkId = 0;
    NetworkType nextHopNetworkType = NetworkType::kLeading;
    NumHops numHops = 0;
    bool retracted = false;
    int8_t patternStartTimeMovementCounter = 0;
  };

  Precedence GetLocalPrecedence(Microseconds currentTime);
  OriginatorEntry* GetOriginatorEntry(NetworkDeviceId originator);
  void UpdateOverriddenPatternWatcher(Precedence precedence);
  PatternBits EnforceForcedPalette(PatternBits pattern);
  void ComputeMessageToSend(NetworkDeviceId originator, Precedence precedence, Microseconds lastOriginationTime,
                            Microseconds currentTime);

  Delegate* const delegate_;  // Unowned.

  std::list<OriginatorEntry> originatorEntries_;

  bool loop_ = false;
  std::optional<uint8_t> forcedPalette_;

  Microseconds currentPatternStartTime_ = timeMicros();
  PatternBits currentPattern_ = EnforceForcedPalette(kStartingPattern);
  PatternBits nextPattern_ = EnforceForcedPalette(computeNextPattern(currentPattern_));

  bool hasNetworks_ = false;
  bool hasMessageToSend_ = false;
  NetworkMessage messageToSend_;

  OptionalMicroseconds lastUserInputTime_;
  Precedence basePrecedence_ = 0;
  Precedence precedenceGain_ = 0;
  bool randomizeLocalDeviceId_ = false;
  NetworkDeviceId localDeviceId_ = NetworkDeviceId();
  NetworkDeviceId currentLeader_ = NetworkDeviceId();
  NetworkId followedNextHopNetworkId_ = 0;
  NetworkType followedNextHopNetworkType_ = NetworkType::kLeading;
  NumHops currentNumHops_ = 0;

  std::optional<OrrerySceneId> orrerySceneIdToSend_;
  OptionalMicroseconds lastOrrerySceneIdSetTime_;
  OrrerySceneIdWatcher* orrerySceneIdWatcher_ = nullptr;  // Unowned.

#if JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(ORRERY_PLANET)
  bool creatureIsFollowingNonCreature_ = false;
#endif  // CREATURE || ORRERY_PLANET
#if JL_IS_CONFIG(ORRERY_LEADER)
  OverriddenPatternWatcher* overriddenPatternWatcher_ = nullptr;  // Unowned.
#endif                                                            // ORRERY_LEADER
};

}  // namespace jazzlights
#endif  // JL_PROTOCOL_ENGINE_H
