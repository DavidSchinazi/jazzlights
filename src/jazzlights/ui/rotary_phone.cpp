#include "jazzlights/ui/rotary_phone.h"

#ifdef ESP32
#if JL_IS_CONFIG(PHONE)

#include <driver/gpio.h>

#include <cstdint>

#include "jazzlights/util/log.h"

#define JL_PHONE_TIME_FMT "%03llds%03lldms%03lldus"
#define JL_PHONE_TIME_VAL(x) ((x) / 1000000), (((x) / 1000) % 1000), ((x) % 1000)

#define JL_PHONE_DEBUG_ENABLED 0

#if JL_PHONE_DEBUG_ENABLED
#define JL_PHONE_DEBUG(...) jll_info(__VA_ARGS__)
#else  // JL_PHONE_DEBUG_ENABLED
#define JL_PHONE_DEBUG(...) jll_debug(__VA_ARGS__)
#endif  // JL_PHONE_DEBUG_ENABLED

namespace jazzlights {
namespace {

#if JL_IS_CONTROLLER(ATOM_MATRIX)
static constexpr uint8_t kPhoneDigitPin = 32;
static constexpr uint8_t kPhoneDialPin = 26;
#elif JL_IS_CONTROLLER(ATOM_S3) || JL_IS_CONTROLLER(ATOM_S3_LITE)
static constexpr uint8_t kPhoneDigitPin = 1;
static constexpr uint8_t kPhoneDialPin = 2;
#else
#error "unsupported controller for phone"
#endif

static constexpr int64_t kPhoneDebounceDuration = 2000;  // 2ms.
static constexpr int64_t kNumberTime = 3000000;          // 3s.
static constexpr int64_t kDigitMinTime = 30000;          // 30ms.
static constexpr int64_t kDigitMaxTime = 120000;         // 120ms.
}  // namespace

PhonePinHandler::PhonePinHandler()
    : digitPin_(kPhoneDigitPin, *this, kPhoneDebounceDuration),
      dialPin_(kPhoneDialPin, *this, kPhoneDebounceDuration) {}

// static
PhonePinHandler* PhonePinHandler::Get() {
  static PhonePinHandler pinHandler;
  return &pinHandler;
}

void PhonePinHandler::RunLoop() {
  digitPin_.RunLoop();
  dialPin_.RunLoop();
}

void PhonePinHandler::HandleChange(uint8_t pin, bool isClosed, int64_t timeOfChange) {
  JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " PhonePinHandler: pin %u %s", JL_PHONE_TIME_VAL(timeOfChange), pin,
                 (isClosed ? "closed" : "opened"));
  if (pin == kPhoneDialPin) {
    dialing_ = isClosed;
    if (dialing_) {
      JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Started dialing", JL_PHONE_TIME_VAL(timeOfChange));
    } else {
      JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Dialed %u lastKnownDigitIsClosed_=%s", JL_PHONE_TIME_VAL(timeOfChange),
                     digitCount_, (lastKnownDigitIsClosed_ ? "closed" : "open"));
      if (!lastKnownDigitIsClosed_) {
        const int64_t timeListLastDigitEvent = timeOfChange - lastDigitEvent_;
        JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Dialing ended with digit pin still open after " JL_PHONE_TIME_FMT,
                       JL_PHONE_TIME_VAL(timeOfChange), JL_PHONE_TIME_VAL(timeListLastDigitEvent));
        if (timeListLastDigitEvent >= kDigitMinTime && timeListLastDigitEvent <= kDigitMaxTime) {
          JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Incrementing count from %u to %u", JL_PHONE_TIME_VAL(timeOfChange),
                         digitCount_, digitCount_ + 1);
          digitCount_++;
        }
      }
      if (0 < digitCount_ && digitCount_ <= 10) {
        if (digitCount_ == 10) { digitCount_ = 0; }
        if (lastNumberEvent_ < 0 || timeOfChange - lastNumberEvent_ > kNumberTime) { fullNumber_ = 0; }
        fullNumber_ = fullNumber_ * 10 + digitCount_;
        jll_info(JL_PHONE_TIME_FMT " Dialed %llu", JL_PHONE_TIME_VAL(timeOfChange), fullNumber_);
        lastNumberEvent_ = timeOfChange;
      }
      digitCount_ = 0;
      lastDigitEvent_ = -1;
    }
  } else if (pin == kPhoneDigitPin) {
    if (lastKnownDigitIsClosed_ == isClosed) {
      JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " STRANGE unexpected repeat digit %s", JL_PHONE_TIME_VAL(timeOfChange),
                     (isClosed ? "open" : "closed"));
    } else {
      lastKnownDigitIsClosed_ = isClosed;
    }
    if (dialing_) {
      if (lastDigitEvent_ >= 0) {
        const int64_t timeListLastDigitEvent = timeOfChange - lastDigitEvent_;
        JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Digit pin was %s for " JL_PHONE_TIME_FMT, JL_PHONE_TIME_VAL(timeOfChange),
                       (isClosed ? "open" : "closed"), JL_PHONE_TIME_VAL(timeListLastDigitEvent));
        if (isClosed && timeListLastDigitEvent >= kDigitMinTime && timeListLastDigitEvent <= kDigitMaxTime) {
          JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Incrementing count from %u to %u", JL_PHONE_TIME_VAL(timeOfChange),
                         digitCount_, digitCount_ + 1);
          digitCount_++;
        }
      } else {
        JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " %s digit pin", JL_PHONE_TIME_VAL(timeOfChange),
                       (isClosed ? "Closing" : "Opening"));
      }
      lastDigitEvent_ = timeOfChange;
    } else {
      const bool dialIsClosed = gpio_get_level(static_cast<gpio_num_t>(kPhoneDialPin)) == 0;
      JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Ignoring digit pin %s when not dialing - dial is %s",
                     JL_PHONE_TIME_VAL(timeOfChange), (isClosed ? "closed" : "opened"),
                     (dialIsClosed ? "UNEXPECTEDLY CLOSED" : "expectedly open"));
    }
  }
}

}  // namespace jazzlights
#endif  // PHONE
#endif  // ESP32
