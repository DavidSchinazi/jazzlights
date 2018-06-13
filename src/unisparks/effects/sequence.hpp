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
           const Next& ... s) : firstDuration(d), first(f), next(s...) {
  }

  size_t contextSize(const Frame& fr) const override {
    return sizeof(Context) + max(first.contextSize(fr), next.contextSize(fr));
  }

  void begin(const Frame& fr) const override {
    Context& ctx = *new(fr.context) Context({nullptr, 0});
    if (fr.time < firstDuration) {
      ctx.current = &first;
      ctx.dt = 0;
    } else {
      ctx.current = &next;
      ctx.dt = -firstDuration;
    }
    Frame tfr = fr;
    tfr.time = fr.time + ctx.dt;
    tfr.context = static_cast<char*>(fr.context) + sizeof(Context);
    ctx.current->begin(tfr);
  }

  void end(const Frame& fr) const override {
    Context& ctx = *static_cast<Context*>(fr.context);
    Frame tfr = fr;
    tfr.time = fr.time + ctx.dt;
    tfr.context = static_cast<char*>(fr.context) + sizeof(Context);
    ctx.current->end(tfr);
  }

  Color color(Point pt, const Frame& fr) const override {
    Context& ctx = *static_cast<Context*>(fr.context);
    Frame tfr = fr;
    tfr.time = fr.time + ctx.dt;
    tfr.context = static_cast<char*>(fr.context) + sizeof(Context);
    return ctx.current->color(pt, tfr);
  }

  Milliseconds duration() const {
    return firstDuration + next.duration();
  }

  struct Context {
    const Effect* current;
    Milliseconds dt;
  };

  // private:
  Milliseconds firstDuration;
  First first;
  Sequence<Next...> next;
};

template<>
class Sequence<> : public Effect {
 public:
  size_t contextSize(const Frame&) const override {
    return 0;
  }

  void begin(const Frame&) const override {
  }

  void end(const Frame&) const override {
  }

  Color color(Point, const Frame&) const override {
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

  size_t contextSize(const Frame& fr) const override {
    return seq.contextSize(fr);
  }

  void begin(const Frame& fr) const override {
    Frame tfr = fr;
    tfr.time = fr.time % seq.duration();
    seq.begin(tfr);
  }

  void end(const Frame& fr) const override {
    Frame tfr = fr;
    tfr.time = fr.time % seq.duration();
    seq.end(tfr);
  }

  Color color(Point pt, const Frame& fr) const override {
    Frame tfr = fr;
    tfr.time = fr.time % seq.duration();
    return seq.color(pt, tfr);
  }

  Milliseconds duration() const {
    return seq.duration() * times;
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
