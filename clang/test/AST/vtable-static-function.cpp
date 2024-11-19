 // RUN: %clang_cc1 -std=c++17 -verify -ast-dump %s | FileCheck %s
// expected-no-diagnostics

struct Base {
  virtual ~Base() = default;
  virtual void process(int x) = 0;
  virtual void templateProcess(int x) = 0;
};

struct VirtualBase : virtual Base {
  void process(int x) override;
  void templateProcess(int x) override;
};

struct Derived : VirtualBase {
  // Static lambda in VMT
  static constexpr auto staticLambda = [](int x) { return x * 2; };

  // Mutable lambda with state
  static auto mutableLambda = [count = 0](int x) mutable {
    return x * (++count);
  };

  // Static member function
  static void staticProcess(int x) { /* process x */ }

  // Static template function
  template<typename T>
  static T staticTemplateProcess(T x) { return x * 2; }

  void process(int x) override {
    // Use static function address
    staticProcess(x);
    // Use static lambda address
    staticLambda(x);
    // Use mutable lambda
    mutableLambda(x);
  }

  void templateProcess(int x) override {
    // Use static template function
    staticTemplateProcess(x);
  }
};

#include <functional>

struct WithStdFunction : Base {
  // Static std::function with template
  static std::function<int(int)> staticFunc;

  template<typename T>
  static std::function<T(T)> staticTemplateFunc;

  void process(int x) override {
    staticFunc(x);
  }

  void templateProcess(int x) override {
    staticTemplateFunc<int>(x);
  }
};

// CHECK: CXXRecordDecl {{.*}} Derived
// CHECK: VTableLayout
// CHECK-NEXT: CK_StaticFunctionPtr {{.*}} staticProcess
// CHECK-NEXT: CK_StaticLambda {{.*}} staticLambda
// CHECK-NEXT: CK_StaticLambda {{.*}} mutableLambda {{.*}} IsMutable
// CHECK-NEXT: CK_StaticFunctionPtr {{.*}} staticTemplateProcess {{.*}} HasTemplateParams

// CHECK: CXXRecordDecl {{.*}} WithStdFunction
// CHECK: VTableLayout
// CHECK-NEXT: CK_StaticStdFunction {{.*}} staticFunc
// CHECK-NEXT: CK_StaticStdFunction {{.*}} staticTemplateFunc {{.*}} HasTemplateParams
