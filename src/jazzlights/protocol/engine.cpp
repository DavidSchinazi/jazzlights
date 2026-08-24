#include "jazzlights/protocol/engine.h"

#include <limits>
#include <sstream>

#include "jazzlights/util/instrumentation.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/pseudorandom.h"

#ifndef JL_PLAYER_LOG_MESSAGES
#define JL_PLAYER_LOG_MESSAGES 0
#endif  // JL_PLAYER_LOG_MESSAGES

#if JL_PLAYER_LOG_MESSAGES
#define jll_player_message(...) jll_info(__VA_ARGS__)
#else  // JL_PLAYER_LOG_MESSAGES
#define jll_player_message(...) jll_debug(__VA_ARGS__)
#endif  // JL_PLAYER_LOG_MESSAGES

namespace jazzlights {

int comparePrecedence(Precedence leftPrecedence, const NetworkDeviceId& leftDeviceId, Precedence rightPrecedence,
                      const NetworkDeviceId& rightDeviceId) {
  if (leftPrecedence < rightPrecedence) {
    return -1;
  } else if (leftPrecedence > rightPrecedence) {
    return 1;
  }
  return leftDeviceId.compare(rightDeviceId);
}

PatternBits computeNextPattern(PatternBits pattern) {
  static_assert(sizeof(PatternBits) == 4, "32bits");
  // This code is inspired by xorshift, amended to only require 32 bits of
  // state. This algorithm was informed by 10 minutes of Googling and a half
  // bottle of Malbec. It is guaranteed to produce numbers.
  pattern ^= pattern << 13;
  pattern ^= pattern >> 17;
  pattern ^= pattern << 5;
  pattern += 0x1337;
  // Apparently xorshift doesn't have great entropy in the lower bits, so let's
  // move those around just because we can.
  const uint8_t shift_offset = (pattern / 16384) % 32;
  pattern = (pattern << shift_offset) | (pattern >> (32 - shift_offset));
  if (pattern == 0) { pattern = kStartingPattern; }
  while (patternIsReserved(pattern)) {
    // Skip reserved patterns.
    pattern = computeNextPattern(pattern);
  }
  return pattern;
}

PatternBits applyPalette(PatternBits pattern, uint8_t palette) {
  // Avoid any reserved patterns.
  while (patternIsReserved(pattern)) { pattern = computeNextPattern(pattern); }
  // Clear palette.
  pattern &= 0xFFFF1FFF;
  // Set palette.
  pattern |= palette << 13;
  return pattern;
}

Precedence getPrecedenceGain(OptionalMicroseconds epochTime, Microseconds currentTime, Microseconds duration,
                             Precedence maxGain) {
  if (!epochTime) {
    return 0;
  } else if (currentTime < *epochTime) {
    return maxGain;
  } else if (currentTime - *epochTime > duration) {
    return 0;
  }
  const Microseconds timeDelta = currentTime - *epochTime;
  if (timeDelta < duration / 10) { return maxGain; }
  return static_cast<uint64_t>(duration - timeDelta) * maxGain / duration;
}

Precedence addPrecedenceGain(Precedence startPrecedence, Precedence gain) {
  if (startPrecedence >= std::numeric_limits<Precedence>::max() - gain) {
    return std::numeric_limits<Precedence>::max();
  }
  return startPrecedence + gain;
}

void ProtocolEngine::SetupDeviceId(NetworkDeviceId localDeviceIdFromNetworks) {
  if (!randomizeLocalDeviceId_ && localDeviceIdFromNetworks != NetworkDeviceId()) {
    localDeviceId_ = localDeviceIdFromNetworks;
  }
  while (localDeviceId_ == NetworkDeviceId()) {
    // If no interfaces have a localDeviceId, generate one randomly.
    uint8_t deviceIdBytes[6] = {};
    UnpredictableRandom::GetBytes(&deviceIdBytes[0], sizeof(deviceIdBytes));
    localDeviceId_ = NetworkDeviceId(deviceIdBytes);
  }
  currentLeader_ = localDeviceId_;
}

bool ProtocolEngine::GetMessageToSend(NetworkMessage* messageToSend) const {
  if (!hasMessageToSend_) { return false; }
  *messageToSend = messageToSend_;
  return true;
}

bool ProtocolEngine::UpdatePrecedence(Precedence basePrecedence, Precedence precedenceGain) {
  if (basePrecedence == basePrecedence_ && precedenceGain == precedenceGain_) { return false; }
  basePrecedence_ = basePrecedence;
  precedenceGain_ = precedenceGain;
  jll_info("updating precedence to base %u gain %u", basePrecedence, precedenceGain);
  return true;
}

void ProtocolEngine::GoToNextPattern() {
  jll_info("next command received: switching from %s (%08x) to %s (%08x), currentLeader=" DEVICE_ID_FMT,
           delegate_->PatternName(currentPattern_).c_str(), currentPattern_,
           delegate_->PatternName(nextPattern_).c_str(), nextPattern_, DEVICE_ID_HEX(currentLeader_));
  Microseconds currentTime = timeMicros();
  lastUserInputTime_ = currentTime;
  currentPatternStartTime_ = currentTime;
  if (loop_ && currentPattern_ == nextPattern_) {
    currentPattern_ = EnforceForcedPalette(computeNextPattern(currentPattern_));
    nextPattern_ = currentPattern_;
  } else {
    currentPattern_ = nextPattern_;
    nextPattern_ = EnforceForcedPalette(computeNextPattern(nextPattern_));
  }
  RunLoop(currentTime);
  jll_info("next command processed: now current %s (%08x) next %s (%08x), currentLeader=" DEVICE_ID_FMT,
           delegate_->PatternName(currentPattern_).c_str(), currentPattern_,
           delegate_->PatternName(nextPattern_).c_str(), nextPattern_, DEVICE_ID_HEX(currentLeader_));
}

void ProtocolEngine::SetPattern(PatternBits pattern) {
  jll_info("set pattern command received: switching from %s (%08x) to %s (%08x), currentLeader=" DEVICE_ID_FMT,
           delegate_->PatternName(currentPattern_).c_str(), currentPattern_, delegate_->PatternName(pattern).c_str(),
           pattern, DEVICE_ID_HEX(currentLeader_));
  Microseconds currentTime = timeMicros();
  lastUserInputTime_ = currentTime;
  currentPatternStartTime_ = currentTime;
  currentPattern_ = pattern;
  if (loop_ && currentPattern_ == nextPattern_) {
    nextPattern_ = currentPattern_;
  } else {
    nextPattern_ = EnforceForcedPalette(computeNextPattern(pattern));
  }
  RunLoop(currentTime);
  jll_info("set pattern command processed: now current %s (%08x) next %s (%08x), currentLeader=" DEVICE_ID_FMT,
           delegate_->PatternName(currentPattern_).c_str(), currentPattern_,
           delegate_->PatternName(nextPattern_).c_str(), nextPattern_, DEVICE_ID_HEX(currentLeader_));
}

void ProtocolEngine::LoopOne() {
  if (loop_) { return; }
  jll_info("Looping");
  loop_ = true;
  nextPattern_ = currentPattern_;
}

void ProtocolEngine::StopLooping() {
  if (!loop_) { return; }
  jll_info("Stopping loop");
  loop_ = false;
  nextPattern_ = EnforceForcedPalette(computeNextPattern(currentPattern_));
}

void ProtocolEngine::SetPatternAndLoop(PatternBits pattern) {
  currentPattern_ = pattern;
  nextPattern_ = currentPattern_;
  loop_ = true;
}

void ProtocolEngine::ResumeRotation() {
  currentPattern_ = EnforceForcedPalette(computeNextPattern(currentPattern_));
  nextPattern_ = EnforceForcedPalette(computeNextPattern(currentPattern_));
}

void ProtocolEngine::ForcePalette(uint8_t palette) {
  jll_info("Forcing palette %u", palette);
  paletteIsForced_ = true;
  forcedPalette_ = palette;
  SetPattern(EnforceForcedPalette(currentPattern_));
}

void ProtocolEngine::StopForcePalette() {
  if (!paletteIsForced_) { return; }
  jll_info("Stop forcing palette %u", forcedPalette_);
  paletteIsForced_ = false;
  forcedPalette_ = 0;
}

PatternBits ProtocolEngine::EnforceForcedPalette(PatternBits pattern) {
  if (paletteIsForced_) { pattern = applyPalette(pattern, forcedPalette_); }
  return pattern;
}

#if JL_IS_CONFIG(CLOUDS)
void ProtocolEngine::ReapplyForcedPalette() {
  currentPattern_ = EnforceForcedPalette(currentPattern_);
  nextPattern_ = EnforceForcedPalette(computeNextPattern(currentPattern_));
}

void ProtocolEngine::CloudNext(bool extraAdvance) {
  Microseconds currentTime = timeMicros();
  lastUserInputTime_ = currentTime;
  if (extraAdvance) {
    currentPattern_ = nextPattern_;
    nextPattern_ = EnforceForcedPalette(computeNextPattern(nextPattern_));
  }
  currentPattern_ = nextPattern_;
  nextPattern_ = EnforceForcedPalette(computeNextPattern(nextPattern_));
  RunLoop(currentTime);
  jll_info("next command processed: now current %s (%08x) next %s (%08x), currentLeader=" DEVICE_ID_FMT,
           delegate_->PatternName(currentPattern_).c_str(), currentPattern_,
           delegate_->PatternName(nextPattern_).c_str(), nextPattern_, DEVICE_ID_HEX(currentLeader_));
}
#endif  // CLOUDS

void ProtocolEngine::UpdateOverriddenPatternWatcher(Precedence precedence) {
#if JL_IS_CONFIG(ORRERY_LEADER)
  if (overriddenPatternWatcher_ != nullptr) {
    if (precedence >= kDefaultOverridePrecedence) {
      overriddenPatternWatcher_->OnOverriddenPattern(currentPattern_);
    } else {
      overriddenPatternWatcher_->OnOverriddenPattern(std::nullopt);
    }
  }
#else   // JL_IS_CONFIG(ORRERY_LEADER)
  (void)precedence;
#endif  // JL_IS_CONFIG(ORRERY_LEADER)
}

Precedence ProtocolEngine::GetLocalPrecedence(Microseconds currentTime) {
  return addPrecedenceGain(basePrecedence_,
                           getPrecedenceGain(lastUserInputTime_, currentTime, kInputDuration, precedenceGain_));
}

ProtocolEngine::OriginatorEntry* ProtocolEngine::GetOriginatorEntry(NetworkDeviceId originator) {
  OriginatorEntry* entry = nullptr;
  for (OriginatorEntry& e : originatorEntries_) {
    if (e.originator == originator) { return &e; }
  }
  return entry;
}

void ProtocolEngine::SetOrrerySceneIdToSend(std::optional<OrrerySceneId> orrerySceneIdToSend) {
  orrerySceneIdToSend_ = orrerySceneIdToSend;
  if (orrerySceneIdToSend_) {
    lastOrrerySceneIdSetTime_ = timeMicros();
    jll_info("Start sending orrery scene ID %d", static_cast<int>(*orrerySceneIdToSend_));
  } else {
    lastOrrerySceneIdSetTime_.reset();
  }
}

void ProtocolEngine::RunLoop(Microseconds currentTime) {
  // Remove elements that have aged out.
  originatorEntries_.remove_if([currentTime](const OriginatorEntry& e) {
    if (currentTime > e.lastOriginationTime + kOriginationTimeDiscard) {
      jll_info("Removing " DEVICE_ID_FMT ".p%u entry due to origination time", DEVICE_ID_HEX(e.originator),
               e.precedence);
      return true;
    }
    if (currentTime > e.currentPatternStartTime + 2 * kEffectDuration) {
      jll_info("Removing " DEVICE_ID_FMT ".p%u entry due to effect duration", DEVICE_ID_HEX(e.originator),
               e.precedence);
      return true;
    }
    return false;
  });
  Precedence precedence = GetLocalPrecedence(currentTime);
  NetworkDeviceId originator = localDeviceId_;
  const OriginatorEntry* entry = nullptr;
  const bool hadRecentUserInput =
      (lastUserInputTime_ && *lastUserInputTime_ <= currentTime && currentTime - *lastUserInputTime_ < kInputDuration);
  for (const OriginatorEntry& e : originatorEntries_) {
#if !JL_IS_CONFIG(CREATURE) && !JL_IS_CONFIG(ORRERY_PLANET)
    // Keep ourselves as leader if there was recent user button input or if we are looping, unless the originator has
    // admin-level precedence.
    if ((hadRecentUserInput || loop_) && e.precedence < kAdminPrecedence) { continue; }
#endif  // CREATURE
    if (e.retracted) {
      jll_debug("ignoring " DEVICE_ID_FMT " due to retracted", DEVICE_ID_HEX(e.originator));
      continue;
    }
    if (currentTime > e.lastOriginationTime + kOriginationTimeDiscard) {
      jll_debug("ignoring " DEVICE_ID_FMT " due to origination time", DEVICE_ID_HEX(e.originator));
      continue;
    }
    if (currentTime > e.currentPatternStartTime + 2 * kEffectDuration) {
      jll_debug("ignoring " DEVICE_ID_FMT " due to effect duration", DEVICE_ID_HEX(e.originator));
      continue;
    }
    if (comparePrecedence(e.precedence, e.originator, precedence, originator) <= 0) {
      jll_debug("ignoring " DEVICE_ID_FMT ".p%u due to better " DEVICE_ID_FMT ".p%u", DEVICE_ID_HEX(e.originator),
                e.precedence, DEVICE_ID_HEX(originator), precedence);
      continue;
    }
    precedence = e.precedence;
    originator = e.originator;
    entry = &e;
  }
#if JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(ORRERY_PLANET)
  (void)hadRecentUserInput;
#endif  // CREATURE || ORRERY_PLANET

  if (currentLeader_ != originator) {
    jll_protocol_info("Switching leader from " DEVICE_ID_FMT " to " DEVICE_ID_FMT, DEVICE_ID_HEX(currentLeader_),
                      DEVICE_ID_HEX(originator));
    currentLeader_ = originator;
    UpdateOverriddenPatternWatcher(precedence);
  }

  Microseconds lastOriginationTime;
  if (entry != nullptr) {
    // Update our state based on entry from leader.
#if JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(ORRERY_PLANET)
    // Creatures only follow non-creatures if they have override enabled.
    const bool newCreatureIsFollowingNonCreature = precedence >= OverridePrecedence();
    if (creatureIsFollowingNonCreature_ != newCreatureIsFollowingNonCreature) {
      jll_info("now %s because " DEVICE_ID_FMT " has precedence %u %s override limit %u",
               (creatureIsFollowingNonCreature_ ? "creatureFollowing" : "creatureIgnoring"), DEVICE_ID_HEX(originator),
               precedence, (creatureIsFollowingNonCreature_ ? "below" : "above"), OverridePrecedence());
    }
    creatureIsFollowingNonCreature_ = newCreatureIsFollowingNonCreature;
#endif  // CREATURE
    nextPattern_ = entry->nextPattern;
    currentPatternStartTime_ = entry->currentPatternStartTime;
    followedNextHopNetworkId_ = entry->nextHopNetworkId;
    followedNextHopNetworkType_ = entry->nextHopNetworkType;
    currentNumHops_ = entry->numHops;
    lastOriginationTime = entry->lastOriginationTime;
    if (currentPattern_ != entry->currentPattern) {
      currentPattern_ = entry->currentPattern;
      jll_protocol_info("Following " DEVICE_ID_FMT ".p%u nh=%u %s new currentPattern %s (%08x)%s",
                        DEVICE_ID_HEX(originator), precedence, currentNumHops_,
                        NetworkTypeToString(followedNextHopNetworkType_),
                        delegate_->PatternName(currentPattern_).c_str(), currentPattern_,
#if JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(ORRERY_PLANET)
                        (creatureIsFollowingNonCreature_ ? " creatureFollowing" : " creatureIgnoring")
#else   // CREATURE
                        ""
#endif  // CREATURE
      );
      delegate_->LogFpsReport();
      delegate_->OnPatternRestart();
      UpdateOverriddenPatternWatcher(precedence);
    }
  } else {
    // We are currently leading.
#if JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(ORRERY_PLANET)
    if (creatureIsFollowingNonCreature_) { jll_info("now creatureIgnoring because we are leading"); }
    creatureIsFollowingNonCreature_ = false;
#endif  // CREATURE || ORRERY_PLANET
    const std::optional<PatternBits> forcedLeadingPattern = delegate_->ForcedLeadingPattern();
    if (forcedLeadingPattern) {
      currentPattern_ = *forcedLeadingPattern;
      nextPattern_ = currentPattern_;
      loop_ = true;
    }
    followedNextHopNetworkId_ = 0;
    followedNextHopNetworkType_ = NetworkType::kLeading;
    currentNumHops_ = 0;
    lastOriginationTime = currentTime;
    while (currentTime - currentPatternStartTime_ > kEffectDuration) {
      currentPatternStartTime_ += kEffectDuration;
      if (loop_) {
        nextPattern_ = currentPattern_;
      } else {
        currentPattern_ = nextPattern_;
        nextPattern_ = EnforceForcedPalette(computeNextPattern(nextPattern_));
      }
      jll_protocol_info("We (" DEVICE_ID_FMT ".p%u) are leading, new currentPattern %s (%08x)",
                        DEVICE_ID_HEX(localDeviceId_), precedence, delegate_->PatternName(currentPattern_).c_str(),
                        currentPattern_);
      delegate_->LogFpsReport();
      delegate_->OnPatternRestart();
    }
  }

  if (!hasNetworks_) {
    jll_debug("not setting messageToSend without networks");
    hasMessageToSend_ = false;
    return;
  }
  ComputeMessageToSend(originator, precedence, lastOriginationTime, currentTime);
}

void ProtocolEngine::ComputeMessageToSend(NetworkDeviceId originator, Precedence precedence,
                                          Microseconds lastOriginationTime, Microseconds currentTime) {
  messageToSend_ = NetworkMessage();
  messageToSend_.originator = originator;
  messageToSend_.sender = localDeviceId_;
  messageToSend_.currentPattern = currentPattern_;
  messageToSend_.nextPattern = nextPattern_;
  messageToSend_.currentPatternStartTime = currentPatternStartTime_;
  messageToSend_.precedence = precedence;
  messageToSend_.lastOriginationTime = lastOriginationTime;
  messageToSend_.numHops = currentNumHops_;
  messageToSend_.receiptNetworkId = followedNextHopNetworkId_;
  messageToSend_.receiptNetworkType = followedNextHopNetworkType_;
#if JL_IS_CONFIG(CREATURE)
  messageToSend_.isCreature = true;
  messageToSend_.isPartying = delegate_->IsPartying();
  messageToSend_.creatureColor = delegate_->CreatureColor();
#endif  // CREATURE
  if (orrerySceneIdToSend_) {
#if JL_IS_CONFIG(ORRERY_LEADER)
    messageToSend_.orrerySceneId = orrerySceneIdToSend_;
#else   // ORRERY_LEADER
    static constexpr Microseconds kOrrerySceneMaxSendDuration = 59 * kMicrosecondsPerSecond;
    if (!lastOrrerySceneIdSetTime_ || currentTime - *lastOrrerySceneIdSetTime_ > kOrrerySceneMaxSendDuration) {
      jll_info("No longer sending orrery scene ID %d", static_cast<int>(*orrerySceneIdToSend_));
      orrerySceneIdToSend_ = std::nullopt;
    } else {
      jll_info("Sending orrery scene ID %d", static_cast<int>(*orrerySceneIdToSend_));
      messageToSend_.orrerySceneId = orrerySceneIdToSend_;
    }
#endif  // ORRERY_LEADER
  }
  (void)currentTime;
  hasMessageToSend_ = true;
}

void ProtocolEngine::HandleReceivedMessage(NetworkMessage message, Microseconds currentTime) {
#if JL_IS_CONFIG(CREATURE)
  if (message.isCreature) {
    jll_info("creature recv %s", networkMessageToString(message).c_str());
    delegate_->OnCreatureHeard(message.creatureColor, message.receiptTime.value_or(currentTime), message.receiptRssi,
                               message.isPartying);
  }
  if (message.orrerySceneId) { delegate_->OnOrreryHeard(); }
#endif  // CREATURE
  jll_player_message("handleReceivedMessage %s", networkMessageToString(message).c_str());
  if (message.sender == localDeviceId_) {
    jll_debug("Ignoring received message that we sent %s", networkMessageToString(message).c_str());
    return;
  }
  if (message.originator == localDeviceId_) {
    jll_debug("Ignoring received message that we originated %s", networkMessageToString(message).c_str());
    return;
  }
  if (orrerySceneIdWatcher_ != nullptr) { orrerySceneIdWatcher_->OnOrrerySceneId(message.orrerySceneId); }
  if (message.numHops == std::numeric_limits<NumHops>::max()) {
    // This avoids overflow when incrementing below.
    jll_protocol_info("Ignoring received message with high numHops %s", networkMessageToString(message).c_str());
    return;
  }
  NumHops receiptNumHops = message.numHops + 1;
  if (currentTime > message.lastOriginationTime + kOriginationTimeDiscard) {
    jll_protocol_info("Ignoring received message due to origination time %s", networkMessageToString(message).c_str());
    return;
  }
  if (currentTime > message.currentPatternStartTime + 2 * kEffectDuration) {
    jll_protocol_info("Ignoring received message due to effect duration %s", networkMessageToString(message).c_str());
    return;
  }
  OriginatorEntry* entry = GetOriginatorEntry(message.originator);
  if (entry == nullptr) {
    originatorEntries_.push_back(OriginatorEntry());
    entry = &originatorEntries_.back();
    entry->originator = message.originator;
    entry->precedence = message.precedence;
    entry->currentPattern = message.currentPattern;
    entry->nextPattern = message.nextPattern;
    entry->currentPatternStartTime = message.currentPatternStartTime;
    entry->lastOriginationTime = message.lastOriginationTime;
    entry->nextHopDevice = message.sender;
    entry->nextHopNetworkId = message.receiptNetworkId;
    entry->nextHopNetworkType = message.receiptNetworkType;
    entry->numHops = receiptNumHops;
    entry->retracted = false;
    entry->patternStartTimeMovementCounter = 0;
    jll_protocol_info("Adding " DEVICE_ID_FMT ".p%u entry via " DEVICE_ID_FMT
                      ".%s"
                      " nh %u ot %lldms current %s (%08x) next %s (%08x) elapsed %lldms",
                      DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
                      NetworkTypeToString(entry->nextHopNetworkType), entry->numHops,
                      MsSinceForLogs(entry->lastOriginationTime, currentTime),
                      delegate_->PatternName(entry->currentPattern).c_str(), entry->currentPattern,
                      delegate_->PatternName(entry->nextPattern).c_str(), entry->nextPattern,
                      MsSinceForLogs(entry->currentPatternStartTime, currentTime));
  } else {
    // The concept behind this is that we build a tree rooted at each originator
    // using a variant of the Bellman-Ford algorithm. We then only ever listen
    // to our next hop on the way to the originator to avoid oscillating between
    // neighbors. To avoid loops in this tree, we ignore any update that has same
    // or more hops than our currently saved one. To allow us to recover from
    // situations where the originator has moved further away in the network, we
    // accept those updates if they're more recent by kOriginationTimeOverride
    // than what we've seen so far. This is based on the theoretical points made
    // in Section 2 of RFC 8966 - we can say that while much simpler and less
    // powerful, this is inspired by the Babel Routing Protocol.
    if (entry->nextHopDevice != message.sender || entry->nextHopNetworkId != message.receiptNetworkId) {
      bool changeNextHop = false;
      if (receiptNumHops < entry->numHops) {
        jll_protocol_info("Switching " DEVICE_ID_FMT ".p%u entry via " DEVICE_ID_FMT
                          ".%s "
                          "nh %u ot %lldms to better nextHop " DEVICE_ID_FMT ".%s nh %u ot %lldms due to nextHops",
                          DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
                          NetworkTypeToString(entry->nextHopNetworkType), entry->numHops,
                          MsSinceForLogs(entry->lastOriginationTime, currentTime), DEVICE_ID_HEX(message.sender),
                          NetworkTypeToString(message.receiptNetworkType), receiptNumHops,
                          MsSinceForLogs(message.lastOriginationTime, currentTime));
        changeNextHop = true;
      } else if (message.lastOriginationTime > entry->lastOriginationTime + kOriginationTimeOverride) {
        jll_protocol_info("Switching " DEVICE_ID_FMT ".p%u entry via " DEVICE_ID_FMT
                          ".%s "
                          "nh %u ot %lldms to better nextHop " DEVICE_ID_FMT
                          ".%s nh %u ot %lldms due to originationTime",
                          DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
                          NetworkTypeToString(entry->nextHopNetworkType), entry->numHops,
                          MsSinceForLogs(entry->lastOriginationTime, currentTime), DEVICE_ID_HEX(message.sender),
                          NetworkTypeToString(message.receiptNetworkType), receiptNumHops,
                          MsSinceForLogs(message.lastOriginationTime, currentTime));
        changeNextHop = true;
      }
      if (changeNextHop) {
        entry->nextHopDevice = message.sender;
        entry->nextHopNetworkId = message.receiptNetworkId;
        entry->nextHopNetworkType = message.receiptNetworkType;
        entry->numHops = receiptNumHops;
      }
    }

    if (entry->nextHopDevice == message.sender && entry->nextHopNetworkId == message.receiptNetworkId) {
      bool shouldUpdateStartTime = false;
      std::ostringstream changes;
      if (entry->precedence != message.precedence) {
        changes << ", precedence " << entry->precedence << " to " << message.precedence;
      }
      if (entry->currentPattern != message.currentPattern) {
        shouldUpdateStartTime = true;
        changes << ", currentPattern " << delegate_->PatternName(entry->currentPattern) << " to "
                << delegate_->PatternName(message.currentPattern);
      }
      if (entry->nextPattern != message.nextPattern) {
        shouldUpdateStartTime = true;
        changes << ", nextPattern " << delegate_->PatternName(entry->nextPattern) << " to "
                << delegate_->PatternName(message.nextPattern);
      }
      // Debounce incoming updates to currentPatternStartTime to avoid visual jitter in the presence
      // of network jitter.
      static constexpr Microseconds kPatternStartTimeDeltaMin = 100 * kMicrosecondsPerMillisecond;
      static constexpr Microseconds kPatternStartTimeDeltaMax = 500 * kMicrosecondsPerMillisecond;
      static constexpr int8_t kPatternStartTimeMovementThreshold = 5;
      if (entry->currentPatternStartTime > message.currentPatternStartTime) {
        const Microseconds timeDelta = entry->currentPatternStartTime - message.currentPatternStartTime;
        const long long timeDeltaMs = MsForLogs(timeDelta);
        if (shouldUpdateStartTime || timeDelta >= kPatternStartTimeDeltaMax) {
          changes << ", elapsedTime -= " << timeDeltaMs;
          shouldUpdateStartTime = true;
        } else if (timeDelta < kPatternStartTimeDeltaMin) {
          if (is_debug_logging_enabled()) { changes << ", elapsedTime !-= " << timeDeltaMs; }
          entry->patternStartTimeMovementCounter = 0;
        } else {
          if (entry->patternStartTimeMovementCounter <= 1) {
            entry->patternStartTimeMovementCounter--;
            if (entry->patternStartTimeMovementCounter <= -kPatternStartTimeMovementThreshold) {
              changes << ", elapsedTime -= " << timeDeltaMs;
              shouldUpdateStartTime = true;
            } else {
              if (is_debug_logging_enabled()) {
                changes << ", elapsedTime ~-= " << timeDeltaMs << " (movement "
                        << static_cast<int>(-entry->patternStartTimeMovementCounter) << ")";
              }
            }
          } else {
            entry->patternStartTimeMovementCounter = 0;
            if (is_debug_logging_enabled()) { changes << ", elapsedTime ~-= " << timeDeltaMs << " (flip)"; }
          }
        }
      } else if (entry->currentPatternStartTime < message.currentPatternStartTime) {
        const Microseconds timeDelta = message.currentPatternStartTime - entry->currentPatternStartTime;
        const long long timeDeltaMs = MsForLogs(timeDelta);
        if (timeDelta > kEffectDuration - kEffectDuration / 10 && entry->originator == currentLeader_) {
          delegate_->OnPatternRestart();
        }
        if (shouldUpdateStartTime || timeDelta >= kPatternStartTimeDeltaMax) {
          changes << ", elapsedTime += " << timeDeltaMs;
          if (entry->currentPattern == message.currentPattern && timeDelta >= kEffectDuration / 2) {
            changes << " (keeping currentPattern " << delegate_->PatternName(entry->currentPattern) << ")";
          }
          shouldUpdateStartTime = true;
        } else if (timeDelta < kPatternStartTimeDeltaMin) {
          if (is_debug_logging_enabled()) { changes << ", elapsedTime !+= " << timeDeltaMs; }
          entry->patternStartTimeMovementCounter = 0;
        } else {
          if (entry->patternStartTimeMovementCounter >= -1) {
            entry->patternStartTimeMovementCounter++;
            if (entry->patternStartTimeMovementCounter >= kPatternStartTimeMovementThreshold) {
              changes << ", elapsedTime += " << timeDeltaMs;
              shouldUpdateStartTime = true;
            } else {
              if (is_debug_logging_enabled()) {
                changes << ", elapsedTime ~+= " << timeDeltaMs << " (movement "
                        << static_cast<int>(entry->patternStartTimeMovementCounter) << ")";
              }
            }
          } else {
            entry->patternStartTimeMovementCounter = 0;
            if (is_debug_logging_enabled()) { changes << ", elapsedTime ~+= " << timeDeltaMs << " (flip)"; }
          }
        }
      }
      if (entry->lastOriginationTime > message.lastOriginationTime) {
        changes << ", originationTime -= " << MsSinceForLogs(message.lastOriginationTime, entry->lastOriginationTime);
      }  // Do not log increases to origination time since all originated messages cause it.
      if (entry->retracted) { changes << ", unretracted"; }
      entry->precedence = message.precedence;
      entry->currentPattern = message.currentPattern;
      entry->nextPattern = message.nextPattern;
      entry->lastOriginationTime = message.lastOriginationTime;
      entry->retracted = false;
      if (shouldUpdateStartTime) {
        entry->currentPatternStartTime = message.currentPatternStartTime;
        entry->patternStartTimeMovementCounter = 0;
      }
      std::string changesStr = changes.str();
      if (!changesStr.empty()) {
        const bool followedUpdate = entry->originator == currentLeader_;
        jll_protocol_info("Accepting %s update from " DEVICE_ID_FMT ".p%u via " DEVICE_ID_FMT ".%s%s%s",
                          (followedUpdate ? "followed" : "ignored"), DEVICE_ID_HEX(entry->originator),
                          entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
                          NetworkTypeToString(entry->nextHopNetworkType), changesStr.c_str(),
                          message.receiptDetails.c_str());
        if (followedUpdate) { printInstrumentationInfo(); }
      }
      UpdateOverriddenPatternWatcher(entry->precedence);
    } else {
      jll_debug("Rejecting %s update from " DEVICE_ID_FMT ".p%u via " DEVICE_ID_FMT
                ".%s because we are following " DEVICE_ID_FMT ".%s",
                (entry->originator == currentLeader_ ? "followed" : "ignored"), DEVICE_ID_HEX(entry->originator),
                entry->precedence, DEVICE_ID_HEX(message.sender), NetworkTypeToString(message.receiptNetworkType),
                DEVICE_ID_HEX(entry->nextHopDevice), NetworkTypeToString(entry->nextHopNetworkType));
    }
  }
  // If this sender is following another originator from what we previously heard,
  // retract any previous entries from them.
  for (OriginatorEntry& e : originatorEntries_) {
    if (e.nextHopDevice == message.sender && e.nextHopNetworkId == message.receiptNetworkId &&
        e.originator != message.originator && !e.retracted) {
      e.retracted = true;
      jll_protocol_info("Retracting entry for originator " DEVICE_ID_FMT
                        ".p%u"
                        " due to abandonment from " DEVICE_ID_FMT
                        ".%s"
                        " in favor of " DEVICE_ID_FMT ".p%u",
                        DEVICE_ID_HEX(e.originator), e.precedence, DEVICE_ID_HEX(message.sender),
                        NetworkTypeToString(message.receiptNetworkType), DEVICE_ID_HEX(message.originator),
                        message.precedence);
    }
  }

  delegate_->OnAcceptedUpdate();
}

}  // namespace jazzlights
