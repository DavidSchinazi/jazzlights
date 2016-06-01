// Copyright 2016, Igor Chernyshev.

#ifndef UTIL_LED_LAYOUT_H_
#define UTIL_LED_LAYOUT_H_

#include <vector>

#include <stdint.h>
#include <unistd.h>

struct LedCoord {
  LedCoord() {}
  LedCoord(int x, int y) : x(x), y(y) {}

  int x = -1;
  int y = -1;
};

struct LedAddress {
  LedAddress() {}
  LedAddress(int strand_id, int led_id)
	: strand_id(strand_id), led_id(led_id) {}

  int strand_id = -1;
  int led_id = -1;
};

// Contains coordinates of the LED's for each strand.
struct LedLayout {
 public:
  LedLayout();
  ~LedLayout();

  void AddCoord(int strand_id, int x, int y);

  int GetStrandCount() const;
  int GetLedCount(int strand_id) const;
  bool GetLedCoord(int strand_id, int led_id, LedCoord* coord) const;

 private:
  struct StrandInfo {
    std::vector<LedCoord> coords;
  };

  StrandInfo* FindStrand(int strand_id);
  const StrandInfo* FindStrand(int strand_id) const;

  std::vector<StrandInfo> strands_;
};

// Assists in mapping LedLayout to a pixel image.
class LedLayoutMap {
 public:
  LedLayoutMap(int width, int height);

  int GetStrandCount() const;
  int GetLedCount(int strand_id) const;

  std::vector<LedCoord> GetLedCoords(int strand_id, int led_id) const;
  std::vector<LedAddress> GetHdrSiblings(int strand_id, int led_id) const;

  void PopulateLayoutMap(const LedLayout& layout);

 private:
  struct LedData {
    std::vector<LedCoord> pixel_coords;
    std::vector<LedAddress> hdr_siblings;
  };

  struct StrandData {
    std::vector<LedData> leds;
  };

  struct PixelUsage {
    //PixelUsage()
    //    : in_use(false), is_primary(false), strand_id(-1), led_id(-1) {}

    bool in_use = false;
    bool is_primary = false;
    LedAddress address;
  };

  void MapLedToPixel(int strand_id, int led_id, int x, int y);
  void CopyLedToPixelMapping(int dst_x, int dst_y, int src_x, int src_y);
  void AddLedAndCoord(int strand_id, int led_id, const LedCoord& coord);
  void AddHdrSibling(int strand_id, int led_id, int strand_id2, int led_id2);
  StrandData* FindStrand(int strand_id);
  const StrandData* FindStrand(int strand_id) const;
  StrandData* FindOrCreateStrand(int strand_id);
  LedData* GetLedData(int strand_id, int led_id);
  PixelUsage* GetPixelUsage(const LedCoord& coord);

  int width_;
  int height_;
  std::vector<PixelUsage> pixel_usage_;
  std::vector<StrandData> strands_;
};

// Contains color data for individual LED's.
class LedStrands {
 public:
  LedStrands(const LedLayout& layout);
  LedStrands(const LedLayoutMap& layout);

  int GetStrandCount() const;
  int GetLedCount(int strand_id) const;

  uint8_t* GetColorData(int strand_id);
  const uint8_t* GetColorData(int strand_id) const;
  int GetColorDataSize(int strand_id) const {
    return GetLedCount(strand_id) * 4;
  }

 private:
  struct StrandData {
    std::vector<uint8_t> color_data;
  };

  LedStrands(const LedStrands& src);
  LedStrands& operator=(const LedStrands& rhs);

  std::vector<StrandData> strands_;
};

#endif  // UTIL_LED_LAYOUT_H_
