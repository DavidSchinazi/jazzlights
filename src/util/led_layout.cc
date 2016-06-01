// Copyright 2014, Igor Chernyshev.

#include "util/led_layout.h"

#include "util/logging.h"

////////////////////////////////////////////////////////////////////////////////
// LedLayout
////////////////////////////////////////////////////////////////////////////////

LedLayout::LedLayout() {}
LedLayout::~LedLayout() {}

void LedLayout::AddCoord(int strand_id, int x, int y) {
  StrandInfo* strand = FindStrand(strand_id);
  if (!strand) {
    strands_.resize(strand_id + 1);
    strand = FindStrand(strand_id);
  }
  strand->coords.push_back(LedCoord(x, y));
}

int LedLayout::GetStrandCount() const {
  return strands_.size();
}

LedLayout::StrandInfo* LedLayout::FindStrand(int strand_id) {
  CHECK(strand_id >= 0);
  return (static_cast<size_t>(strand_id) < strands_.size() ?
      &strands_.at(strand_id) : nullptr);
}

const LedLayout::StrandInfo* LedLayout::FindStrand(int strand_id) const {
  CHECK(strand_id >= 0);
  return (static_cast<size_t>(strand_id) < strands_.size() ?
      &strands_.at(strand_id) : nullptr);
}

int LedLayout::GetLedCount(int strand_id) const {
  const StrandInfo* strand = FindStrand(strand_id);
  return (strand ? strand->coords.size() : 0);
}

bool LedLayout::GetLedCoord(int strand_id, int led_id, LedCoord* coord) const {
  const StrandInfo* strand = FindStrand(strand_id);
  if (!strand)
    return false;

  *coord = strand->coords[led_id];
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LedLayoutMap
////////////////////////////////////////////////////////////////////////////////

LedLayoutMap::LedLayoutMap(int width, int height)
    : width_(width), height_(height) {
  pixel_usage_.resize(width_ * height_);
}

int LedLayoutMap::GetStrandCount() const {
  return strands_.size();
}

int LedLayoutMap::GetLedCount(int strand_id) const {
  const StrandData* strand = FindStrand(strand_id);
  return (strand ? strand->leds.size() : 0);
}

std::vector<LedCoord> LedLayoutMap::GetLedCoords(int strand_id,
						 int led_id) const {
  const StrandData* strand = FindStrand(strand_id);
  if (!strand || static_cast<size_t>(led_id) >= strand->leds.size())
    return std::vector<LedCoord>();

  return strand->leds.at(led_id).pixel_coords;
}

std::vector<LedAddress> LedLayoutMap::GetHdrSiblings(int strand_id,
						     int led_id) const {
  const StrandData* strand = FindStrand(strand_id);
  if (!strand || static_cast<size_t>(led_id) >= strand->leds.size())
    return std::vector<LedAddress>();

  return strand->leds.at(led_id).hdr_siblings;
}

void LedLayoutMap::AddLedAndCoord(int strand_id,
				  int led_id,
				  const LedCoord& coord) {
  GetLedData(strand_id, led_id)->pixel_coords.push_back(coord);
}

void LedLayoutMap::AddHdrSibling(int strand_id,
				 int led_id,
				 int strand_id2,
				 int led_id2) {
  CHECK(strand_id2 >= 0);
  CHECK(led_id2 >= 0);
  GetLedData(strand_id, led_id)->hdr_siblings.push_back(
      LedAddress(strand_id2, led_id2));
}

LedLayoutMap::StrandData* LedLayoutMap::FindStrand(int strand_id) {
  CHECK(strand_id >= 0);
  return (static_cast<size_t>(strand_id) < strands_.size() ?
      &strands_.at(strand_id) : nullptr);
}

const LedLayoutMap::StrandData* LedLayoutMap::FindStrand(int strand_id) const {
  CHECK(strand_id >= 0);
  return (static_cast<size_t>(strand_id) < strands_.size() ?
      &strands_.at(strand_id) : nullptr);
}

LedLayoutMap::StrandData* LedLayoutMap::FindOrCreateStrand(int strand_id) {
  StrandData* strand = FindStrand(strand_id);
  if (strand) {
    return strand;
  }
  strands_.resize(strand_id + 1);
  return FindStrand(strand_id);
}

LedLayoutMap::LedData* LedLayoutMap::GetLedData(int strand_id, int led_id) {
  StrandData* strand = FindOrCreateStrand(strand_id);
  CHECK(led_id >= 0);
  if (static_cast<size_t>(led_id) >= strand->leds.size()) {
    strand->leds.resize(led_id + 1);
  }
  return &strand->leds.at(led_id);
}

LedLayoutMap::PixelUsage* LedLayoutMap::GetPixelUsage(const LedCoord& coord) {
  if (coord.x < 0 || coord.x >= width_ || coord.y < 0 || coord.y >= height_)
    return nullptr;

  return &pixel_usage_[coord.y * width_ + coord.x];
}

void LedLayoutMap::MapLedToPixel(int strand_id, int led_id, int x, int y) {
  LedCoord coord(x, y);
  PixelUsage* usage = GetPixelUsage(coord);
  if (!usage || usage->in_use)
    return;

  AddLedAndCoord(strand_id, led_id, coord);
  usage->in_use = true;
  usage->is_primary = true;
  usage->address = LedAddress(strand_id, led_id);
}

void LedLayoutMap::CopyLedToPixelMapping(int dst_x,
					 int dst_y,
					 int src_x,
					 int src_y) {
  LedCoord src_coord(src_x, src_y);
  LedCoord dst_coord(dst_x, dst_y);
  PixelUsage* src_usage = GetPixelUsage(src_coord);
  PixelUsage* dst_usage = GetPixelUsage(dst_coord);
  if (!src_usage || !dst_usage || !src_usage->is_primary || dst_usage->in_use)
    return;

  /*AddLedAndCoord(src_usage->strand_id, src_usage->led_id, dst_coord);
  dst_usage->strand_id = src_usage->strand_id;
  dst_usage->led_id = src_usage->led_id;
  dst_usage->in_use = true;*/
}

void LedLayoutMap::PopulateLayoutMap(const LedLayout& layout) {
  int strand_count = layout.GetStrandCount();
  for (int strand_id = 0; strand_id < strand_count; ++strand_id) {
    int led_count = layout.GetLedCount(strand_id);
    for (int led_id = 0; led_id < led_count; ++led_id) {
      LedCoord coord;
      if (!layout.GetLedCoord(strand_id, led_id, &coord))
        continue;

      int x = coord.x;
      int y = coord.y;
      MapLedToPixel(strand_id, led_id, x, y);
      MapLedToPixel(strand_id, led_id, x-1, y-1);
      MapLedToPixel(strand_id, led_id, x-1, y);
      MapLedToPixel(strand_id, led_id, x-1, y+1);
      MapLedToPixel(strand_id, led_id, x+1, y-1);
      MapLedToPixel(strand_id, led_id, x+1, y);
      MapLedToPixel(strand_id, led_id, x+1, y+1);
      MapLedToPixel(strand_id, led_id, x,   y-1);
      MapLedToPixel(strand_id, led_id, x,   y+1);
    }
  }

  // TODO(igorc): Fill more of the pixel area.
  // Find matches for all points that were not filled. Do only one iteration
  // as the majority of the relevant pixels have already been mapped.
  for (int x = 0; x < width_; ++x) {
    for (int y = 0; y < height_; ++y) {
      CopyLedToPixelMapping(x, y, x-1, y-1);
      CopyLedToPixelMapping(x, y, x-1, y  );
      CopyLedToPixelMapping(x, y, x-1, y+1);
      CopyLedToPixelMapping(x, y, x+1, y-1);
      CopyLedToPixelMapping(x, y, x+1, y  );
      CopyLedToPixelMapping(x, y, x+1, y+1);
      CopyLedToPixelMapping(x, y, x,   y-1);
      CopyLedToPixelMapping(x, y, x,   y+1);
    }
  }

  // Find HDR siblings.
  // TODO(igorc): Compute max distance instead of hard-coding.
  static const int kHdrSiblingsDistance = 13;
  int max_distance2 = kHdrSiblingsDistance * kHdrSiblingsDistance;
  for (int strand_id1 = 0; strand_id1 < strand_count; ++strand_id1) {
    int led_count1 = layout.GetLedCount(strand_id1);
    for (int led_id1 = 0; led_id1 < led_count1; ++led_id1) {
      LedCoord coord1;
      if (!layout.GetLedCoord(strand_id1, led_id1, &coord1))
	continue;

      for (int strand_id2 = 0; strand_id2 < strand_count; ++strand_id2) {
        int led_count2 = layout.GetLedCount(strand_id2);
        for (int led_id2 = 0; led_id2 < led_count2; ++led_id2) {
          LedCoord coord2;
          if (!layout.GetLedCoord(strand_id2, led_id2, &coord2))
            continue;

          int x_d = coord2.x - coord1.x;
          int y_d = coord1.y - coord1.y;
          int distance2 = x_d * x_d + y_d * y_d;
          if (distance2 < max_distance2)
            AddHdrSibling(strand_id1, led_id1, strand_id2, led_id2);
        }
      }
    }
  }

  // TODO(igorc): Warn when no coordingates were found.

  /*for (int strand_id  = 0; strand_id < STRAND_COUNT; ++strand_id) {
    for (int led_id = 0; led_id < layout_.lengths_[strand_id]; ++led_id) {
      fprintf(stderr, "HDR siblings: strand=%d, led=%d, count=%ld\n",
              strand_id, led_id,
              layout_.hdr_siblings_[strand_id][led_id].size());
    }
  }*/
}

////////////////////////////////////////////////////////////////////////////////
// LedStrands
////////////////////////////////////////////////////////////////////////////////

LedStrands::LedStrands(const LedLayout& layout) {
  strands_.resize(layout.GetStrandCount());
  for (int i = 0; i < layout.GetStrandCount(); ++i) {
    int led_count = layout.GetLedCount(i);
    strands_[i].color_data.resize(led_count * 4);
  }
}

LedStrands::LedStrands(const LedLayoutMap& layout) {
  strands_.resize(layout.GetStrandCount());
  for (int i = 0; i < layout.GetStrandCount(); ++i) {
    int led_count = layout.GetLedCount(i);
    strands_[i].color_data.resize(led_count * 4);
  }
}

int LedStrands::GetStrandCount() const {
  return strands_.size();
}

int LedStrands::GetLedCount(int strand_id) const {
  CHECK(strand_id >= 0 && strand_id < static_cast<int>(strands_.size()));
  return strands_[strand_id].color_data.size() / 4;
}

uint8_t* LedStrands::GetColorData(int strand_id) {
  CHECK(strand_id >= 0 && strand_id < static_cast<int>(strands_.size()));
  return &strands_[strand_id].color_data[0];
}

const uint8_t* LedStrands::GetColorData(int strand_id) const {
  CHECK(strand_id >= 0 && strand_id < static_cast<int>(strands_.size()));
  return &strands_[strand_id].color_data[0];
}
