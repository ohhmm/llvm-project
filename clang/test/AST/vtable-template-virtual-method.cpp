 // RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++20 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++20 -ast-dump %s | FileCheck %s --check-prefix=AST

// Test case based on the scenario from:
// https://www.quora.com/profile/Serg-Kryvonos/Template-virtual-method-into-C

// Basic template virtual method test
class Base {
public:
    virtual ~Base() = default;
    template<typename T>
    virtual void method(T obj) {
        // Base implementation
    }
};

class Derived : public Base {
public:
    template<typename T>
    void method(T obj) override {
        // Derived implementation
    }
};

// Test with auto parameter and lambda storage
class AutoBase {
public:
    virtual ~AutoBase() = default;
    virtual void process(auto obj) {
        static auto lambda = [](auto x) { /* base impl */ };
        lambda(obj);
    }
};

class AutoDerived : public AutoBase {
public:
    void process(auto obj) override {
        static auto lambda = [](auto x) { /* derived impl */ };
        lambda(obj);
    }
};

// Test with explicit lambda VMT storage
class LambdaBase {
private:
    using LambdaType = decltype([](auto x) { /* base impl */ });
    static inline LambdaType staticLambda = [](auto x) { /* base impl */ };

public:
    virtual ~LambdaBase() = default;
    virtual void execute(auto param) const {
        staticLambda(param);
    }
};

class LambdaDerived : public LambdaBase {
private:
    using LambdaType = decltype([](auto x) { /* derived impl */ });
    static inline LambdaType staticLambda = [](auto x) { /* derived impl */ };

public:
    void execute(auto param) const override {
        staticLambda(param);
    }
};

// CHECK: @_ZTV4Base = {{.*}}constant
// CHECK-NEXT: i8* null
// CHECK-NEXT: i8* bitcast
// CHECK-NEXT: i8* bitcast {{.*}} to i8*

// CHECK: @_ZTV7Derived = {{.*}}constant
// CHECK-NEXT: i8* null
// CHECK-NEXT: i8* bitcast
// CHECK-NEXT: i8* bitcast {{.*}} to i8*

// CHECK: @_ZTV8AutoBase = {{.*}}constant
// CHECK-NEXT: i8* null
// CHECK-NEXT: i8* bitcast
// CHECK-NEXT: i8* bitcast {{.*}}lambda{{.*}} to i8*

// CHECK: @_ZTV11AutoDerived = {{.*}}constant
// CHECK-NEXT: i8* null
// CHECK-NEXT: i8* bitcast
// CHECK-NEXT: i8* bitcast {{.*}}lambda{{.*}} to i8*

// AST: CXXRecordDecl {{.*}} Base
// AST-NEXT: |-CXXDestructorDecl {{.*}} virtual ~Base
// AST-NEXT: `-FunctionTemplateDecl {{.*}} method

// AST: CXXRecordDecl {{.*}} Derived
// AST-NEXT: |-CXXMethodDecl {{.*}} virtual method

int main() {
    Base* b = new Derived();
    b->method(42);           // Test with int
    b->method("test");       // Test with string
    delete b;

    AutoBase* ab = new AutoDerived();
    ab->process(3.14);       // Test with double
    delete ab;

    LambdaBase* lb = new LambdaDerived();
    lb->execute(true);       // Test with bool
    delete lb;

    return 0;
}
