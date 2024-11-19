// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++17 -emit-llvm -o - %s | FileCheck %s

#include <functional>

struct Base {
  virtual void foo() = 0;
  virtual void bar(int x) = 0;
};

// Test static function in VMT
struct StaticImpl : Base {
  static void staticFoo() { }
  static void staticBar(int x) { }

  void foo() override { staticFoo(); }
  void bar(int x) override { staticBar(x); }
};

// Test lambda in VMT
struct LambdaImpl : Base {
  void foo() override {
    static auto lambda = []() { };
    lambda();
  }

  void bar(int x) override {
    static auto lambda = [](int y) { return y * 2; };
    lambda(x);
  }
};

// Test mutable lambda
struct MutableLambdaImpl : Base {
  void foo() override {
    static auto lambda = [count = 0]() mutable { ++count; };
    lambda();
  }

  void bar(int x) override {
    static auto lambda = [sum = 0](int y) mutable { sum += y; return sum; };
    lambda(x);
  }
};

// Test std::function
struct FunctionImpl : Base {
  static std::function<void()> staticFunction;
  static std::function<void(int)> paramFunction;

  void foo() override { staticFunction(); }
  void bar(int x) override { paramFunction(x); }
};

// Test template parameters
template<typename T>
struct TemplateImpl : Base {
  static void templateFoo() { T::process(); }
  static void templateBar(T x) { x.process(); }

  void foo() override { templateFoo(); }
  void bar(int x) override { templateBar(T(x)); }
};

// Test multiple inheritance
struct OtherBase {
  virtual void baz() = 0;
};

struct MultipleImpl : Base, OtherBase {
  static void staticBaz() { }

  void foo() override { StaticImpl::staticFoo(); }
  void bar(int x) override { StaticImpl::staticBar(x); }
  void baz() override { staticBaz(); }
};

// Test virtual inheritance
struct VirtualBase {
  virtual void qux() = 0;
};

struct VirtualImpl : virtual Base, virtual VirtualBase {
  static void staticQux() { }

  void foo() override { StaticImpl::staticFoo(); }
  void bar(int x) override { StaticImpl::staticBar(x); }
  void qux() override { staticQux(); }
};

// CHECK: @_ZTV10StaticImpl = {{.*}}constant
// CHECK: @_ZTV10LambdaImpl = {{.*}}constant
// CHECK: @_ZTV15MutableLambdaImpl = {{.*}}constant
// CHECK: @_ZTV12FunctionImpl = {{.*}}constant
// CHECK: @_ZTV11MultipleImpl = {{.*}}constant
// CHECK: @_ZTV11VirtualImpl = {{.*}}constant

// CHECK: define {{.*}}staticFoo
// CHECK: define {{.*}}staticBar
// CHECK: define {{.*}}lambda
// CHECK: define {{.*}}mutable_lambda
// CHECK: define {{.*}}std_function
// CHECK: define {{.*}}template_foo
// CHECK: define {{.*}}staticBaz
// CHECK: define {{.*}}staticQux
