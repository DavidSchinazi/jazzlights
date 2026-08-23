#ifndef JL_PSEUDORANDOM_H
#define JL_PSEUDORANDOM_H

#include <cstddef>
#include <cstdint>

namespace jazzlights {

// Shared implementation, only meant to be used by subclasses.
class Random {
 public:
  virtual uint8_t GetRandomByte() = 0;
  virtual uint32_t GetRandom32bits() = 0;
  virtual void GetRandomBytes(void* buffer, size_t length) = 0;
  // Between functions consider min and max to be inclusive.
  int32_t GetRandomNumberBetween(int32_t min, int32_t max);
  double GetRandomDoubleBetween(double min, double max);

 protected:
  Random() = default;
};

// Provides the most unpredictable randomness that the underlying system can provide.
class UnpredictableRandom : public Random {
 public:
  static uint8_t GetByte();
  static uint32_t Get32bits();
  static void GetBytes(void* buffer, size_t length);
  // Between functions consider min and max to be inclusive.
  static int32_t GetNumberBetween(int32_t min, int32_t max);
  static double GetDoubleBetween(double min, double max);

 private:
  UnpredictableRandom() = default;
  uint8_t GetRandomByte() override;
  uint32_t GetRandom32bits() override;
  void GetRandomBytes(void* buffer, size_t length) override;
};

}  // namespace jazzlights

#endif  // JL_PSEUDORANDOM_H
