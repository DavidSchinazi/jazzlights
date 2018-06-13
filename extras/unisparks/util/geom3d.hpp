#ifndef UNISPARKS_GEOM3D_HPP
#define UNISPARKS_GEOM3D_HPP
#include <iostream>

namespace unisparks {

struct Point3D {
  double x;
  double y;
  double z;
};

template<int WIDTH, int HEIGHT, int DEPTH>
struct MappedMatrix3D {
  constexpr MappedMatrix3D() {
    for(int x=0; x<WIDTH; ++x) {
      for(int y=0; y<HEIGHT; ++y) {
        for(int z=0; z<DEPTH; ++z) {
          points[x + HEIGHT*WIDTH*y*z] = {x,y,z};
        }
      }
    }
  }
  Point3D points[WIDTH*HEIGHT*DEPTH];
};

inline std::ostream& operator<<(std::ostream& out, const Point2D& v) {
  out << "(" << v.x << "," << v.y << ")";
  return out;
}

inline std::ostream& operator<<(std::ostream& out, const Point3D& v) {
  out << "(" << v.x << "," << v.y << "," << v.z << ")";
  return out;
}

} // namespace unisparks
#endif /* UNISPARKS_GEOM3D_HPP */
