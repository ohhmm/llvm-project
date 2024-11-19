// RUN: %clang_cc1 -triple x86_64-unknown-unknown -std=c++17 -ast-dump %s | FileCheck %s

template<typename T>
class Base {
  virtual void method() {}
  virtual ~Base() {}
};

template<typename T>
class Derived : public Base<T> {
  void method() override {}
};

// CHECK: CXXRecordDecl {{.*}} class Base
// CHECK: -CXXMethodDecl {{.*}} virtual method 'void (void)'
// CHECK: -CXXDestructorDecl {{.*}} virtual ~Base 'void (void)'
// CHECK: CXXRecordDecl {{.*}} class Derived
// CHECK: -CXXMethodDecl {{.*}} virtual method 'void (void)' override
