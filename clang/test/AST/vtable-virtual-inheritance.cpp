// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm-only -verify %s

#include <functional>

struct Base {
  virtual ~Base() = default;
  virtual void process() = 0;
};

struct VirtualBase {
  virtual ~VirtualBase() = default;
  virtual std::function<void()> getHandler() = 0;
};

// Test virtual inheritance with std::function
struct Derived : virtual Base, virtual VirtualBase {
  std::function<void()> handler;

  void process() override {
    if (handler) handler();
  }

  std::function<void()> getHandler() override {
    return [this]() { process(); };
  }
};

// Test multiple virtual inheritance
struct OtherVirtualBase {
  virtual ~OtherVirtualBase() = default;
  virtual void otherProcess(std::function<void(int)>) = 0;
};

struct ComplexDerived : virtual Base, virtual VirtualBase, virtual OtherVirtualBase {
  std::function<void()> handler;
  std::function<void(int)> intHandler;

  void process() override {
    if (handler) handler();
  }

  std::function<void()> getHandler() override {
    return [this]() {
      if (intHandler) intHandler(42);
      process();
    };
  }

  void otherProcess(std::function<void(int)> fn) override {
    intHandler = fn;
  }
};

// expected-no-diagnostics
