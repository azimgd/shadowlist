#pragma once

#include <ReactCommon/TurboModule.h>
#include <react/bridging/Bridging.h>

namespace facebook::react {

class JSI_EXPORT NativeShadowlistCxxSpecJSI : public TurboModule {
  protected:
    NativeShadowlistCxxSpecJSI(std::shared_ptr<CallInvoker> jsInvoker);

  public:
    virtual void setup(jsi::Runtime &rt) = 0;
  };

  template <typename T>
  class JSI_EXPORT NativeShadowlistCxxSpec : public TurboModule {
  public:
  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propName) override {
    return delegate_.get(rt, propName);
  }

  static constexpr std::string_view kModuleName = "Shadowlist";

  protected:
  NativeShadowlistCxxSpec(std::shared_ptr<CallInvoker> jsInvoker) :
    TurboModule(std::string{NativeShadowlistCxxSpec::kModuleName}, jsInvoker),
      delegate_(reinterpret_cast<T*>(this), jsInvoker) {}

  private:
  class Delegate : public NativeShadowlistCxxSpecJSI {
  public:
    Delegate(T *instance, std::shared_ptr<CallInvoker> jsInvoker)
      : NativeShadowlistCxxSpecJSI(std::move(jsInvoker)), instance_(instance) {}

    void setup(jsi::Runtime &rt) override {
      static_assert(
        bridging::getParameterCount(&T::setup) == 1,
        "Expected setup(...) to have 1 parameters");

      return bridging::callFromJs<void>(
        rt, &T::setup, jsInvoker_, instance_);
    }

  private:
    friend class NativeShadowlistCxxSpec;
    T *instance_;
  };

  Delegate delegate_;
};
}
