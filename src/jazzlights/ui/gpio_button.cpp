#include "jazzlights/ui/gpio_button.h"

#ifdef ESP32

#include <driver/gpio.h>
#include <esp_ipc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>

#include "jazzlights/esp32_shared.h"
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
static constexpr int64_t kButtonDebounceDuration = 20000;  // 20ms.
static constexpr int64_t kLongPressTime = 1000000;         // 1s.
}  // namespace

// GPIO reads can return spurious results sometimes. In particular when
// transitioning from not-pressed to pressed, it might rapidly alternate
// between the two a few times before settling on the correct value. To avoid
// reacting to these, we debounce the digital reads and only react after the
// value has settled for kDebounceTime.

GpioPin::GpioPin(uint8_t pin, PinInterface& pinInterface, int64_t debounceDuration)
    : pinInterface_(pinInterface),
      queue_(xQueueCreate(/*num_queue_items=*/16, /*queue_item_size=*/sizeof(uint64_t))),
      lastRunloopQueueEventTime_(-1),
      lastChangeAwayFromDebounced_(-1),
      lastIsClosedInISR_(false),
      isClosedRawRunloop_(false),
      isClosedDebouncedRunloop_(false),
      pin_(pin),
      debounceDuration_(debounceDuration) {
  if (queue_ == nullptr) { jll_fatal("Failed to create GpioPin queue"); }
  InstallGpioIsrService();
  // GPIO interrupt handlers are run on the core where the config calls were made, so we use esp_ipc_call to
  // ensure that all that happens on core 0. This prevents it from interfering with LED writes on core 1.
  ESP_ERROR_CHECK(esp_ipc_call(/*coreID=*/0, ConfigurePin, /*arg=*/this));
}

GpioButton::GpioButton(uint8_t pin, ButtonInterface& buttonInterface)
    : gpioPin_(pin, *this, kButtonDebounceDuration),
      buttonInterface_(buttonInterface),
      lastEvent_(-1),
      isHeld_(false) {}

// static
void GpioPin::ConfigurePin(void* arg) {
  GpioPin* gpioPin = reinterpret_cast<GpioPin*>(arg);
  const gpio_config_t config = {
      .pin_bit_mask = (1ULL << gpioPin->pin_),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
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

void GpioButton::HandleChange(uint8_t changedPin, bool isClosed, int64_t timeOfChange) {
  if (changedPin != pin()) { jll_fatal("Unexpected pin %u != %u", changedPin, pin()); }
  JL_GPIO_DEBUG("Pin %u at " JL_GPIO_TIME_FMT " the btn was %s", pin(), JL_GPIO_TIME_VAL(timeOfChange),
                (isClosed ? "closed" : "open"));
  if (isClosed) {
    // GpioButton was just pressed, record time.
    lastEvent_ = timeOfChange;
  } else {
    // GpioButton was just released.
    if (!isHeld_ && timeOfChange - lastEvent_ < kLongPressTime) {
      // GpioButton was released after a short duration.
      lastEvent_ = timeOfChange;
      buttonInterface_.ShortPress(pin());
    }
    isHeld_ = false;
  }
}

void GpioPin::RunLoop() {
  uint64_t state;
  while (xQueueReceive(queue_, &state, /*xTicksToWait=*/0)) {
    const bool isClosed = (state & 0x8000000000000000ULL) != 0;
    const int64_t eventTime = static_cast<int64_t>(state & 0x7FFFFFFFFFFFFFFFULL);
    if (isClosedRawRunloop_ == isClosed || eventTime <= lastRunloopQueueEventTime_) { continue; }
    lastRunloopQueueEventTime_ = eventTime;
    isClosedRawRunloop_ = isClosed;
    if (isClosedRawRunloop_ != isClosedDebouncedRunloop_) {  // Moving away from debounced state.
      lastChangeAwayFromDebounced_ = eventTime;
    } else {  // Going back to the debounced state.
      if ((eventTime - lastChangeAwayFromDebounced_) > debounceDuration_) {
        // We spent more than kDebounceTime away from the debounce time and then went back.
        JL_GPIO_DEBUG(
            JL_GPIO_TIME_FMT
            " Informing client that pin %u is %s while parsing events, lastChangeAwayFromDebounced = " JL_GPIO_TIME_FMT,
            JL_GPIO_TIME_VAL(eventTime), pin_, (!isClosedRawRunloop_ ? "closed" : "open"),
            JL_GPIO_TIME_VAL(lastChangeAwayFromDebounced_));
        pinInterface_.HandleChange(pin_, !isClosedRawRunloop_, lastChangeAwayFromDebounced_);
        isClosedDebouncedRunloop_ = !isClosedRawRunloop_;
        lastChangeAwayFromDebounced_ = eventTime;
      }
    }
  }
  // In theory this call to gpio_get_level() should not be necessary, as we get an interrupt any time the value changes.
  // However, in practice it appears that the interrupt sometimes does not fire. We call it here to remedy that. This
  // negates the CPU usage benefits of using the interrupt handler, but still maintains the benefit of keeping track of
  // events even if the main runloop is busy performing other tasks.
  const bool liveIsClosed = gpio_get_level(static_cast<gpio_num_t>(pin_)) == 0;
  if (liveIsClosed != isClosedRawRunloop_) {
    const int64_t currentTime64 = esp_timer_get_time();
    JL_GPIO_DEBUG(JL_GPIO_TIME_FMT " Pin %u is unexpectedly %s from runloop, adding event to queue",
                  JL_GPIO_TIME_VAL(currentTime64), pin_, (liveIsClosed ? "closed" : "open"));
    uint64_t state = static_cast<uint64_t>(currentTime64);
    state &= 0x7FFFFFFFFFFFFFFFULL;
    if (liveIsClosed) { state |= 0x8000000000000000ULL; }
    xQueueSendToBack(queue_, &state, /*wait_time=*/0);
    RunLoop();
    return;
  }
  const int64_t currentTime64 = esp_timer_get_time();
  const int64_t timeSinceLastChange = currentTime64 - lastChangeAwayFromDebounced_;
  if (isClosedDebouncedRunloop_ != isClosedRawRunloop_ && timeSinceLastChange > debounceDuration_) {
    // The debounce time has elapsed since the last change away from the previous debounced value.
    JL_GPIO_DEBUG(JL_GPIO_TIME_FMT
                  " Informing client that pin %u is %s after debouncing from runloop, telling client event time was "
                  "lastChangeAwayFromDebounced = " JL_GPIO_TIME_FMT,
                  JL_GPIO_TIME_VAL(currentTime64), pin_, (isClosedRawRunloop_ ? "closed" : "open"),
                  JL_GPIO_TIME_VAL(lastChangeAwayFromDebounced_));
    isClosedDebouncedRunloop_ = isClosedRawRunloop_;
    pinInterface_.HandleChange(pin_, isClosedDebouncedRunloop_, lastChangeAwayFromDebounced_);
  }
}

void GpioButton::RunLoop() {
  gpioPin_.RunLoop();
  const int64_t currentTime64 = esp_timer_get_time();
  if (IsPressed() && currentTime64 - lastEvent_ >= kLongPressTime) {
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

bool GpioButton::HasBeenPressedLongEnoughForLongPress() {
  return IsPressed() && (isHeld_ || esp_timer_get_time() - lastEvent_ >= kLongPressTime);
}

// static
void GpioPin::InterruptHandler(void* arg) { reinterpret_cast<GpioPin*>(arg)->HandleInterrupt(); }

// Note that regular logging functions cannot be used inside interrupt handlers because they use locks.
// Instead use: ets_printf("foobar %d\n", 42);

void GpioPin::HandleInterrupt() {
  const bool newIsClosed = gpio_get_level(static_cast<gpio_num_t>(pin_)) == 0;
  if (newIsClosed != lastIsClosedInISR_) {
    lastIsClosedInISR_ = newIsClosed;
    const int64_t currentTime64 = esp_timer_get_time();
    JL_GPIO_DEBUG_ISR(JL_GPIO_TIME_FMT " Pin %u switching raw to %s", JL_GPIO_TIME_VAL(currentTime64), pin_,
                      (newIsClosed ? "closed" : "open"));
    uint64_t state = static_cast<uint64_t>(currentTime64);
    state &= 0x7FFFFFFFFFFFFFFFULL;
    if (newIsClosed) { state |= 0x8000000000000000ULL; }
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(queue_, &state, &higherPriorityTaskWoken);
  }
}

}  // namespace jazzlights

#endif  // ESP32
