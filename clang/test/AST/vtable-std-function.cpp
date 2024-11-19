// RUN: %clang_cc1 -std=c++17 -verify %s
// expected-no-diagnostics

#include <functional>

struct Base {
  virtual ~Base() = default;
  virtual void foo() = 0;
};

struct Derived : Base {
  std::function<void()> func;

  // Test basic std::function in VMT
  void foo() override {
    if (func) func();
  }

  // Test templated functor
  template<typename T>
  struct Functor {
    template<typename U>
    void operator()(U x) const { }
  };

  // Test std::function with templated functor
  template<typename T>
  void setFunctor() {
    func = Functor<T>();
  }
};

// Test lambda with templated operator()
struct LambdaDerived : Base {
  void foo() override {
    auto lambda = [](auto x) { };
    lambda(42);
  }
};

int main() {
  Derived d;
  d.setFunctor<int>();
  d.foo();

  LambdaDerived ld;
  ld.foo();
  return 0;
}
