#ifndef UNISPARKS_EFFECT_H
#define UNISPARKS_EFFECT_H
#include <stdint.h>
#include <assert.h>
#include "unisparks/pixels.h"
#include "unisparks/log.h"

// <new> header is not available on all boards, so we'll
// declare placement new/delete operators here
#ifdef ARDUINO_AVR_UNO
inline void* operator new(size_t, void* p) noexcept {return p;}
inline void* operator new[](size_t, void* p) noexcept {return p;}
inline void operator delete  (void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}
#else 
#include <new>
#endif

namespace unisparks {


class Effect;
    
struct Frame {
  int32_t timeElapsed;
  int32_t timeRemaining;
  int32_t timeSinceBeat;
};


class Effect {
public:
  class Renderer;

  virtual ~Effect() noexcept = default;

  virtual const char *name() const = 0;
  virtual int32_t preferredDuration() const = 0;

  virtual size_t renderBufSize(const Pixels& pixels) const = 0;
  virtual Renderer *start(Pixels& pixels, void *buf, size_t size) const = 0;

  class Renderer {
  public:
    virtual ~Renderer() = default;
    virtual const Effect& effect() const = 0;
    virtual void render(const Frame& frame) = 0;
  };
};


class BasicEffect : public Effect {
public:
  BasicEffect(const char *name) : name_(name) {}
  const char *name() const final {return name_;}
  int32_t preferredDuration() const {return 10000;}

private:
  const char *name_;
};


class StatelessEffect : public BasicEffect {
public:
  using BasicEffect::BasicEffect;

  size_t renderBufSize(const Pixels&) const final {
    return sizeof(Renderer);
  }

  Effect::Renderer* start(Pixels& pixels, void *buf, size_t bufsize) const;

private:
    virtual void render(Pixels& pixels, const Frame& frame) const = 0;

    struct Renderer : public Effect::Renderer {
        Renderer(const StatelessEffect& ef, Pixels& px) : effect_(ef), pixels_(px) {}
        const Effect& effect() const final { return effect_; }
        void render(const Frame& frame) final {effect_.render(pixels_, frame);}
        const StatelessEffect& effect_;
        Pixels& pixels_;
    };
    friend struct Renderer;
};


template<typename ContextT>
class StatefulEffect : public BasicEffect {
public:
  using BasicEffect::BasicEffect;

  size_t renderBufSize(const Pixels& pixels) const final {
    assert(contextSize(pixels) >= sizeof(ContextT));
    return sizeof(Renderer) + contextSize(pixels);
  }

  Effect::Renderer* start(Pixels& pixels, void *buf, size_t bufsize) const {
      assert(renderBufSize(pixels) <= bufsize);
      return new (buf) Renderer{*this, pixels};
  }

private:
    virtual size_t contextSize(const Pixels& pixels) const = 0;
    virtual void begin(const Pixels& pixels, ContextT &ctx) const = 0;
    virtual void end(const Pixels& pixels, ContextT &ctx) const = 0;
    virtual void render(Pixels& pixels, const Frame& frame, ContextT& ctx) const = 0;

    struct Renderer : public Effect::Renderer {
        Renderer(const StatefulEffect<ContextT>& ef, Pixels& px) : effect_(ef), pixels_(px) {effect_.begin(pixels_, context_);}
        ~Renderer() {effect_.end(pixels_, context_);}
        const Effect& effect() const final { return effect_; }
        void render(const Frame& frame) final {effect_.render(pixels_, frame, context_);}
        const StatefulEffect& effect_;
        Pixels& pixels_;
        ContextT context_;
    };
    friend struct Renderer;
};


template<typename RenderFuncT>
class FunctionalEffect : public StatelessEffect {
public:
    FunctionalEffect(const char *name, RenderFuncT render) :
      StatelessEffect(name), render_(render) {}

private:
    void render(Pixels& pixels, const Frame& frame) const final {render_(pixels, frame);}
    RenderFuncT render_;
};
    
template<typename RenderFuncT>
FunctionalEffect<RenderFuncT> functionalEffect(const char *name, RenderFuncT f) {
    return FunctionalEffect<RenderFuncT>(name, f);
}

} // namespace unisparks
#endif /* UNISPARKS_EFFECT_H */
