// Need to refactor, temporarily disable
#if 0
#ifndef UNISPARKS_EFFECTS_SEQUENCE_H
#define UNISPARKS_EFFECTS_SEQUENCE_H
#include "unisparks/effects/wrapped.hpp"


#include <cxxabi.h>
#define DEBU(x) x

namespace unisparks {

template<typename E>
struct EffectWithDuration : public WrappedEffect<E> {
  EffectWithDuration(const E& e, Milliseconds d) : WrappedEffect<E>(e), duration(d) {
  }

  Milliseconds duration;
};

template<typename E>
EffectWithDuration<E> dur(const E& effect, Milliseconds duration) {
  return EffectWithDuration<E>(effect, duration);
}


template<typename... Effects>
class Sequence;

template<typename First, typename... Next>
class Sequence<Milliseconds, First, Next...> : public Effect {
 public:
  Sequence(const First& f, const Next&... s) : first(f), next(s...) {
    DEBU(printf(">>> creating %s sequence %p of %d element(s)\n", 
      abi::__cxa_demangle(typeid(this).name(), NULL, NULL, NULL), this, 1+sizeof...(s)/2);)  
  }

  Milliseconds duration() const {
    return first.duration + next.duration;
  }

  size_t contextSize(const Animation& a) const override {
    return sizeof(Context) + max(first.contextSize(a), next.contextSize(a));
  }

  void begin(const Frame& frame) const override {
    Context& ctx = cast_context<Context>(frame);
    ctx.isFirst = frame.time < first.duration;
    if (ctx.isFirst) {
      //info(">>>sequence %p %d begin first", this, frame.time);
      first.begin(firstFrame(frame));
    } else {
      //info(">>>sequence %p %d begin second", this, frame.time);
      next.begin(nextFrame(frame));
    }
  }

  void rewind(const Frame& frame) const override {
    Context& ctx = *static_cast<Context*>(frame.animation.context);
    bool isFirst = frame.time < first.duration;
    if (isFirst && ctx.isFirst) {
      //info(">>>sequence %p %d rewind first", this, frame.time);
      first.rewind(firstFrame(frame));
    } else if (isFirst && !ctx.isFirst) {
      //info(">>>sequence %p %d end second, begin first", this, frame.time);
      next.end(nextAnim(frame.animation));
      first.begin(firstFrame(frame));
    } else if (!isFirst && ctx.isFirst) {
      //info(">>>sequence %p %d end first, begin second", this, frame.time);
      first.end(firstAnim(frame.animation));
      next.begin(nextFrame(frame));
    } else {
      //info(">>>sequence %p %d rewind second", this, frame.time);
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

  struct Context {
    Milliseconds lastTime;
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
    fr.time -= first.duration;
    return fr;
  }

  Pixel nextPixel(Pixel px) const {
    px.frame = nextFrame(px.frame);
    return px;
  }

  First first;
  Sequence<Next...> next;
};

template<>
class Sequence<> : public Effect {
 public:
  Sequence() {
    DEBU(printf(">>> creating %s sequence %p of 0 element(s)\n", 
      abi::__cxa_demangle(typeid(this).name(), NULL, NULL, NULL), this);) 
  }

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
#endif
