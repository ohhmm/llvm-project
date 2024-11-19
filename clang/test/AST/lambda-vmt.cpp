// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm-only -verify %s

struct Base {
  virtual ~Base() = default;
  virtual void process(int x) = 0;
};

struct Derived : Base {
  template<typename T>
  auto makeHandler() {
    return [this](T x) { process(static_cast<int>(x)); };
  }

  void process(int x) override {
    auto handler = makeHandler<double>();
    handler(static_cast<double>(x));
  }
};

// Test multiple inheritance with lambdas
struct OtherBase {
  virtual ~OtherBase() = default;
  virtual void otherProcess() = 0;
};

struct MultiDerived : Base, OtherBase {
  void process(int x) override {
    auto lambda = [this, x]() { return x * 2; };
    lambda();
  }

  void otherProcess() override {
    auto lambda = [this]() { process(42); };
    lambda();
  }
};

// expected-no-diagnostics
