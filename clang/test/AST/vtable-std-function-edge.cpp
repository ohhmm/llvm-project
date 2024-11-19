// RUN: %clang_cc1 -std=c++17 -verify -ast-dump %s | FileCheck %s
// expected-no-diagnostics

#include <functional>

// Test multiple inheritance with std::function
struct A {
  virtual ~A() = default;
  virtual void foo() = 0;
};

struct B {
  virtual ~B() = default;
  virtual int bar() = 0;
};

// CHECK: CXXRecordDecl {{.*}} Derived
// CHECK: VTableLayout
// CHECK-NEXT: CK_StdFunction
struct Derived : A, B {
  std::function<void()> func1;
  std::function<int(int)> func2;

  // Test std::function in multiple VMTs
  void foo() override {
    if (func1) func1();
  }

  int bar() override {
    return func2 ? func2(42) : 0;
  }

  // Test templated member function with std::function
  template<typename T>
  void setFunc(std::function<T(T)> f) {
    func2 = f;
  }
};

// Test std::function with mutable lambda
struct MutableLambda : A {
  int counter = 0;
  void foo() override {
    auto lambda = [count = counter]() mutable {
      return ++count;
    };
    func = lambda;
  }
  std::function<int()> func;
};

// Test std::function with templated callable object
template<typename T>
struct TemplatedCallable {
  template<typename U>
  auto operator()(U x) const {
    return static_cast<T>(x);
  }
};

// CHECK: CXXRecordDecl {{.*}} TemplateTest
// CHECK: VTableLayout
// CHECK-NEXT: CK_StdFunction
struct TemplateTest : A {
  void foo() override {
    func = TemplatedCallable<int>();
  }
  std::function<int(double)> func;
};

int main() {
  Derived d;
  d.setFunc<int>([](int x) { return x * 2; });
  d.bar();

  MutableLambda ml;
  ml.foo();
  ml.func();

  TemplateTest tt;
  tt.foo();
  tt.func(3.14);

  return 0;
}
