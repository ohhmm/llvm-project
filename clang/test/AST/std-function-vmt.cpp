// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm-only -verify %s

#include <functional>

struct Base {
  virtual ~Base() = default;
  virtual void foo() = 0;
};

struct Derived : Base {
  std::function<void()> func;

  template<typename T>
  void operator()(T x) {
    // Template parameter usage
    func = [x]() { /* use x */ };
  }

  // Override with std::function
  void foo() override {
    func();
  }
};

// expected-no-diagnostics
