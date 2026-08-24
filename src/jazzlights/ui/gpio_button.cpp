#include "jazzlights/ui/gpio_button.h"

#ifdef ESP32

#include <driver/gpio.h>
#include <esp_ipc.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>

#include "jazzlights/util/esp32_shared.h"
#include "jazzlights/util/log.h"

#define JL_GPIO_DEBUG_ENABLED 0

#if JL_GPIO_DEBUG_ENABLED
#define JL_GPIO_DEBUG(...) jll_info(__VA_ARGS__)
#else  // JL_GPIO_DEBUG_ENABLED
#define JL_GPIO_DEBUG(...) jll_debug(__VA_ARGS__)
#endif  // JL_GPIO_DEBUG_ENABLED

#define JL_GPIO_DEBUG_ISR(format, ...)                                              \
  do {                                                                              \
    if (JL_GPIO_DEBUG_ENABLED) { ets_printf("INFO: " format "\n", ##__VA_ARGS__); } \
  } while (false)

#define JL_GPIO_TIME_FMT "%03llds%03lldms%03lldus"
#define JL_GPIO_TIME_VAL(x) ((x) / 1000000), (((x) / 1000) % 1000), ((x) % 1000)

namespace jazzlights {
namespace {
static constexpr Microseconds kButtonDebounceDuration = 20000;  // 20ms.
static constexpr Microseconds kLongPressTime = 1000000;         // 1s.
static constexpr uint64_t kBitMaskTime = 0x7FFFFFFFFFFFFFFFULL;
static constexpr uint64_t kBitMaskClosed = 0x8000000000000000ULL;
static_assert((kBitMaskTime & kBitMaskClosed) == 0ULL, "bad bitmasks");
static_assert((kBitMaskTime | kBitMaskClosed) == ~0ULL, "bad bitmasks");
}  // namespace

// GPIO reads can return spurious results sometimes. In particular when
// transitioning from not-pressed to pressed, it might rapidly alternate
// between the two a few times before settling on the correct value. To avoid
// reacting to these, we debounce the digital reads and only react after the
// value has settled for kDebounceTime.

GpioPin::GpioPin(uint8_t pin, PinInterface& pinInterface, Microseconds debounceDuration, bool closedIsHigh)
    : pinInterface_(pinInterface),
      queue_(xQueueCreate(/*num_queue_items=*/16, /*queue_item_size=*/sizeof(uint64_t))),
      pin_(pin),
      debounceDuration_(debounceDuration),
      closedIsHigh_(closedIsHigh) {
  if (queue_ == nullptr) { jll_fatal("Failed to create GpioPin queue"); }
  InstallGpioIsrService();
#if JL_ESP32C6 || defined(CONFIG_FREERTOS_UNICORE)
  ConfigurePin(this);
#else
  // GPIO interrupt handlers are run on the core where the config calls were made, so we use esp_ipc_call to
  // ensure that all that happens on core 0. This prevents it from interfering with LED writes on core 1.
  ESP_ERROR_CHECK(esp_ipc_call(/*coreID=*/0, ConfigurePin, /*arg=*/this));
#endif
}

GpioButton::GpioButton(uint8_t pin, ButtonInterface& buttonInterface)
    : gpioPin_(pin, *this, kButtonDebounceDuration, /*closedIsHigh=*/false), buttonInterface_(buttonInterface) {}

template <bool closedIsHigh>
GpioSwitch<closedIsHigh>::GpioSwitch(uint8_t pin, SwitchInterface& switchInterface)
    : gpioPin_(pin, *this, kButtonDebounceDuration, closedIsHigh), switchInterface_(switchInterface) {}

// static
void GpioPin::ConfigurePin(void* arg) {
  GpioPin* gpioPin = reinterpret_cast<GpioPin*>(arg);
  const gpio_config_t config = {
      .pin_bit_mask = (1ULL << gpioPin->pin_),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = gpioPin->closedIsHigh_ ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE,
      .pull_down_en = gpioPin->closedIsHigh_ ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE,
  };
  ESP_ERROR_CHECK(gpio_config(&config));
  ESP_ERROR_CHECK(gpio_isr_handler_add(static_cast<gpio_num_t>(gpioPin->pin_), &GpioPin::InterruptHandler, gpioPin));
}

GpioPin::~GpioPin() {
  jll_fatal("Destructing GpioPin is not currently supported");
  // This destruction is unsafe since a race condition could cause the interrupt handler to fire after the destructor is
  // called. If we ever have a need to destroy these, we'll need to make this safe first.
  // ESP_ERROR_CHECK(gpio_isr_handler_remove(static_cast<gpio_num_t>(pin_)));
}

GpioButton::~GpioButton() {}

template <bool closedIsHigh>
GpioSwitch<closedIsHigh>::~GpioSwitch() {}

void GpioButton::HandleChange(uint8_t changedPin, bool isClosed, Microseconds timeOfChange) {
  if (changedPin != pin()) { jll_fatal("Unexpected pin %u != %u", changedPin, pin()); }
  JL_GPIO_DEBUG("Pin %u at " JL_GPIO_TIME_FMT " the btn was %s", pin(), JL_GPIO_TIME_VAL(timeOfChange),
                (isClosed ? "closed" : "open"));
  if (isClosed) {
    // GpioButton was just pressed, record time.
    lastEvent_ = timeOfChange;
  } else {
    // GpioButton was just released.
    if (lastEvent_ && !isHeld_ && timeOfChange - *lastEvent_ < kLongPressTime) {
      // GpioButton was released after a short duration.
      lastEvent_ = timeOfChange;
      buttonInterface_.ShortPress(pin());
    }
    isHeld_ = false;
  }
}

template <bool closedIsHigh>
void GpioSwitch<closedIsHigh>::HandleChange(uint8_t changedPin, bool isClosed, Microseconds /*timeOfChange*/) {
  if (changedPin != pin()) { jll_fatal("Unexpected pin %u != %u", changedPin, pin()); }
  switchInterface_.StateChanged(pin(), isClosed);
}

void GpioPin::RunLoop() {
  uint64_t state;
  while (xQueueReceive(queue_, &state, /*xTicksToWait=*/0)) {
    const bool isClosed = (state & kBitMaskClosed) != 0;
    const Microseconds eventTime = static_cast<Microseconds>(state & kBitMaskTime);
    if (isClosedRawRunloop_ == isClosed || (lastRunloopQueueEventTime_ && eventTime <= *lastRunloopQueueEventTime_)) {
      continue;
    }
    lastRunloopQueueEventTime_ = eventTime;
    isClosedRawRunloop_ = isClosed;
    if (isClosedRawRunloop_ != isClosedDebouncedRunloop_) {  // Moving away from debounced state.
      lastChangeAwayFromDebounced_ = eventTime;
    } else {  // Going back to the debounced state.
      if (lastChangeAwayFromDebounced_ && (eventTime - *lastChangeAwayFromDebounced_) > debounceDuration_) {
        // We spent more than kDebounceTime away from the debounce time and then went back.
        JL_GPIO_DEBUG(
            JL_GPIO_TIME_FMT
            " Informing client that pin %u is %s while parsing events, lastChangeAwayFromDebounced = " JL_GPIO_TIME_FMT,
            JL_GPIO_TIME_VAL(eventTime), pin_, (!isClosedRawRunloop_ ? "closed" : "open"),
            JL_GPIO_TIME_VAL(*lastChangeAwayFromDebounced_));
        pinInterface_.HandleChange(pin_, !isClosedRawRunloop_, *lastChangeAwayFromDebounced_);
        isClosedDebouncedRunloop_ = !isClosedRawRunloop_;
        lastChangeAwayFromDebounced_ = eventTime;
      }
    }
  }
  // In theory this call to gpio_get_level() should not be necessary, as we get an interrupt any time the value changes.
  // However, in practice it appears that the interrupt sometimes does not fire. We call it here to remedy that. This
  // negates the CPU usage benefits of using the interrupt handler, but still maintains the benefit of keeping track of
  // events even if the main runloop is busy performing other tasks.
  const int closedValue = closedIsHigh_ ? 1 : 0;
  const bool liveIsClosed = gpio_get_level(static_cast<gpio_num_t>(pin_)) == closedValue;
  if (liveIsClosed != isClosedRawRunloop_) {
    const Microseconds currentTime64 = TimeMicros();
    JL_GPIO_DEBUG(JL_GPIO_TIME_FMT " Pin %u is unexpectedly %s from runloop, adding event to queue",
                  JL_GPIO_TIME_VAL(currentTime64), pin_, (liveIsClosed ? "closed" : "open"));
    uint64_t state = static_cast<uint64_t>(currentTime64);
    state &= kBitMaskTime;
    if (liveIsClosed) { state |= kBitMaskClosed; }
    xQueueSendToBack(queue_, &state, /*wait_time=*/0);
    RunLoop();
    return;
  }
  if (lastChangeAwayFromDebounced_) {
    const Microseconds currentTime64 = TimeMicros();
    const Microseconds timeSinceLastChange = currentTime64 - *lastChangeAwayFromDebounced_;
    if (isClosedDebouncedRunloop_ != isClosedRawRunloop_ && timeSinceLastChange > debounceDuration_) {
      // The debounce time has elapsed since the last change away from the previous debounced value.
      JL_GPIO_DEBUG(JL_GPIO_TIME_FMT
                    " Informing client that pin %u is %s after debouncing from runloop, telling client event time was "
                    "lastChangeAwayFromDebounced = " JL_GPIO_TIME_FMT,
                    JL_GPIO_TIME_VAL(currentTime64), pin_, (isClosedRawRunloop_ ? "closed" : "open"),
                    JL_GPIO_TIME_VAL(*lastChangeAwayFromDebounced_));
      isClosedDebouncedRunloop_ = isClosedRawRunloop_;
      pinInterface_.HandleChange(pin_, isClosedDebouncedRunloop_, *lastChangeAwayFromDebounced_);
    }
  }
}

void GpioButton::RunLoop() {
  gpioPin_.RunLoop();
  const Microseconds currentTime64 = TimeMicros();
  if (IsPressed() && lastEvent_ && currentTime64 - *lastEvent_ >= kLongPressTime) {
    // GpioButton has been held down for kLongPressTime since last event.
    lastEvent_ = currentTime64;
    if (!isHeld_) {
      // GpioButton has been held down for the first kLongPressTime.
      isHeld_ = true;
      buttonInterface_.LongPress(pin());
    } else {
      // GpioButton has been held down for another kLongPressTime.
      buttonInterface_.HeldDown(pin());
    }
  }
}

template <bool closedIsHigh>
void GpioSwitch<closedIsHigh>::RunLoop() {
  gpioPin_.RunLoop();
}

template class GpioSwitch<true>;
template class GpioSwitch<false>;

bool GpioButton::HasBeenPressedLongEnoughForLongPress() {
  return IsPressed() && (isHeld_ || (lastEvent_ && TimeMicros() - *lastEvent_ >= kLongPressTime));
}

// static
void GpioPin::InterruptHandler(void* arg) { reinterpret_cast<GpioPin*>(arg)->HandleInterrupt(); }

// Note that regular logging functions cannot be used inside interrupt handlers because they use locks.
// Instead use: ets_printf("foobar %d\n", 42);

void GpioPin::HandleInterrupt() {
  const int closedValue = closedIsHigh_ ? 1 : 0;
  const bool newIsClosed = gpio_get_level(static_cast<gpio_num_t>(pin_)) == closedValue;
  if (newIsClosed != lastIsClosedInISR_) {
    lastIsClosedInISR_ = newIsClosed;
    const Microseconds currentTime64 = TimeMicros();
    JL_GPIO_DEBUG_ISR(JL_GPIO_TIME_FMT " Pin %u switching raw to %s", JL_GPIO_TIME_VAL(currentTime64), pin_,
                      (newIsClosed ? "closed" : "open"));
    uint64_t state = static_cast<uint64_t>(currentTime64);
    state &= kBitMaskTime;
    if (newIsClosed) { state |= kBitMaskClosed; }
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(queue_, &state, &higherPriorityTaskWoken);
  }
}

}  // namespace jazzlights

#endif  // ESP32
