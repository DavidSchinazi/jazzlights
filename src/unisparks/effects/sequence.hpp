#ifndef UNISPARKS_EFFECTS_SEQUENCE_H
#define UNISPARKS_EFFECTS_SEQUENCE_H
#include "unisparks/effect.hpp"

namespace unisparks {

template<typename... Effects>
class Sequence;

template<typename First, typename... Next>
class Sequence<Milliseconds, First, Next...> : public Effect {
 public:
  Sequence(Milliseconds d, const First& f,
           const Next&... s) : firstDuration(d), first(f), next(s...) {
  }

  Milliseconds duration() const {
    return firstDuration + next.duration();
  }

  size_t contextSize(const Animation& a) const override {
    return sizeof(Context) + max(first.contextSize(a), next.contextSize(a));
  }

  void begin(const Frame& frame) const override {
    Context& ctx = *static_cast<Context*>(frame.animation.context);
    ctx.isFirst = frame.time < firstDuration;
    if (ctx.isFirst) {
//      info(">>>%p %d begin first", this, frame.time);
      first.begin(firstFrame(frame));
    } else {
//      info(">>>%p %d begin second", this, frame.time);
      next.begin(nextFrame(frame));
    }
  }

  void rewind(const Frame& frame) const override {
    Context& ctx = *static_cast<Context*>(frame.animation.context);
    bool isFirst = frame.time < firstDuration;
    if (isFirst && ctx.isFirst) {
//      info(">>>%p %d rewind first", this, frame.time);
      first.rewind(firstFrame(frame));
    } else if (isFirst && !ctx.isFirst) {
//      info(">>>%p %d end second, begin first", this, frame.time);
      next.end(nextAnim(frame.animation));
      first.begin(firstFrame(frame));
    } else if (!isFirst && ctx.isFirst) {
//      info(">>>%p %d end first, begin second", this, frame.time);
      first.end(firstAnim(frame.animation));
      next.begin(nextFrame(frame));
    } else {
//      info(">>>%p %d rewind second", this, frame.time);
      next.rewind(nextFrame(frame));
    }
    ctx.isFirst = isFirst;
  }

  Color color(const Pixel& px) const override {
    Context& ctx = *static_cast<Context*>(px.frame.animation.context);
    Color res = ctx.isFirst ? first.color(firstPixel(px)) : next.color(nextPixel(px));
//    info(">>>%p %d return %s color: RGB(%d,%d,%d)", this, px.frame.time,
//         ctx.isFirst ? "first" : "second",
//      res.asRgb().red, res.asRgb().green, res.asRgb().blue);
    return res;
  }

  void end(const Animation& a) const override {
    Context& ctx = *static_cast<Context*>(a.context);
    ctx.isFirst ? first.end(firstAnim(a)) : next.end(nextAnim(a));
  }

  struct Context {
    bool isFirst;
  };

  Animation firstAnim(Animation am) const {
    am.context = static_cast<char*>(am.context) + sizeof(Context);
    return am;
  }

  Frame firstFrame(Frame fr) const {
    fr.animation = firstAnim(fr.animation);
    return fr;
  }

  Pixel firstPixel(Pixel px) const {
    px.frame = firstFrame(px.frame);
    return px;
  }

  Animation nextAnim(Animation am) const {
    am.context = static_cast<char*>(am.context) + sizeof(Context) +
                 first.contextSize(am);
    return am;
  }

  Frame nextFrame(Frame fr) const {
    fr.animation = nextAnim(fr.animation);
    fr.time -= firstDuration;
    return fr;
  }

  Pixel nextPixel(Pixel px) const {
    px.frame = nextFrame(px.frame);
    return px;
  }

  Milliseconds firstDuration;
  First first;
  Sequence<Next...> next;
};

// TODO: Replace it with specialization of just one item
template<>
class Sequence<> : public Effect {
 public:
  size_t contextSize(const Animation&) const override {
    return 0;
  }

  void begin(const Frame&) const override {
  }

  void rewind(const Frame&) const override {
  }

  Color color(const Pixel&) const override {
    return 0x0;
  }

  void end(const Animation&) const override {
  }

  Milliseconds duration() const {
    return 0;
  }
};

template <typename... Effects>
Sequence<Effects...> sequence(const Effects& ... effects) {
  return Sequence<Effects...>(effects...);
}


// This is a modifier for sequences & loops, ordinary effects
// loop indefinitely by default
template<typename SequenceT>
struct Loop : Effect {

  Loop(int times, SequenceT ef) : times(times), seq(ef) {
  }

  size_t contextSize(const Animation& a) const override {
    return seq.contextSize(a);
  }

  void begin(const Frame& f) const override {
    seq.begin(tframe(f));
  }

  void rewind(const Frame& f) const override {
    seq.rewind(tframe(f));
  }

  Color color(const Pixel& px) const override {
    return seq.color(tpixel(px));
  }

  void end(const Animation& a) const override {
    seq.end(a);
  }

  Milliseconds duration() const {
    return seq.duration() * times;
  }

  Frame tframe(Frame f) const {
    f.time = f.time % seq.duration();
    return f;
  }

  Pixel tpixel(Pixel p) const {
    p.frame = tframe(p.frame);
    return p;
  }

  int times;
  SequenceT seq;
};

template <typename T>
Loop<T> loop(int times, const T& seq) {
  return Loop<T>(times, seq);
}

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_SEQUENCE_H */
