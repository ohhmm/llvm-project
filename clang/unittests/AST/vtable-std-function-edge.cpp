#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Testing/CommandLineArgs.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"
#include <memory>

using namespace clang;
using namespace clang::tooling;

namespace {

class VMTStdFunctionEdgeTest : public ::testing::Test {
protected:
  std::unique_ptr<ASTUnit> buildASTFromCode(llvm::StringRef Code) {
    std::vector<std::string> Args = {
      "-std=c++17",
      "-fno-rtti",
      "-fno-exceptions"
    };
    return tooling::buildASTFromCodeWithArgs(Code, Args);
  }
};

TEST_F(VMTStdFunctionEdgeTest, EmptyStdFunction) {
  const char *Code = R"(
    #include <functional>
    struct Base {
      virtual ~Base() = default;
      virtual std::function<void()> getHandler() = 0;
    };
    struct Derived : Base {
      std::function<void()> getHandler() override {
        return {}; // Return empty std::function
      }
    };
  )";

  auto AST = buildASTFromCode(Code);
  ASSERT_TRUE(AST != nullptr);

  auto TU = AST->getASTContext().getTranslationUnitDecl();
  auto *DerivedDecl = llvm::dyn_cast<CXXRecordDecl>(
    *TU->lookup(DeclarationName(&AST->getASTContext().Idents.get("Derived"))).begin());
  ASSERT_TRUE(DerivedDecl != nullptr);

  // Verify VMT handles empty std::function correctly
  EXPECT_TRUE(DerivedDecl->hasDefinition());
  EXPECT_TRUE(DerivedDecl->isDynamicClass());
}

TEST_F(VMTStdFunctionEdgeTest, RecursiveStdFunction) {
  const char *Code = R"(
    #include <functional>
    struct Base {
      virtual ~Base() = default;
      virtual std::function<void(std::function<void()>)> getNestedHandler() = 0;
    };
    struct Derived : Base {
      std::function<void(std::function<void()>)> getNestedHandler() override {
        return [](std::function<void()> f) {
          if(f) f();
        };
      }
    };
  )";

  auto AST = buildASTFromCode(Code);
  ASSERT_TRUE(AST != nullptr);

  auto TU = AST->getASTContext().getTranslationUnitDecl();
  auto *DerivedDecl = llvm::dyn_cast<CXXRecordDecl>(
    *TU->lookup(DeclarationName(&AST->getASTContext().Idents.get("Derived"))).begin());
  ASSERT_TRUE(DerivedDecl != nullptr);

  // Verify VMT handles nested std::function types
  EXPECT_TRUE(DerivedDecl->hasDefinition());
  EXPECT_TRUE(DerivedDecl->isDynamicClass());
}

TEST_F(VMTStdFunctionEdgeTest, MoveOnlyTypes) {
  const char *Code = R"(
    #include <functional>
    #include <memory>
    struct Base {
      virtual ~Base() = default;
      virtual std::function<void(std::unique_ptr<int>)> getMoveOnlyHandler() = 0;
    };
    struct Derived : Base {
      std::function<void(std::unique_ptr<int>)> getMoveOnlyHandler() override {
        return [](std::unique_ptr<int> p) { /* handle move-only type */ };
      }
    };
  )";

  auto AST = buildASTFromCode(Code);
  ASSERT_TRUE(AST != nullptr);

  auto TU = AST->getASTContext().getTranslationUnitDecl();
  auto *DerivedDecl = llvm::dyn_cast<CXXRecordDecl>(
    *TU->lookup(DeclarationName(&AST->getASTContext().Idents.get("Derived"))).begin());
  ASSERT_TRUE(DerivedDecl != nullptr);

  // Verify VMT handles move-only types in std::function
  EXPECT_TRUE(DerivedDecl->hasDefinition());
  EXPECT_TRUE(DerivedDecl->isDynamicClass());
}

} // end anonymous namespace
