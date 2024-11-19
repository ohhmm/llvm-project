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

class VMTStdFunctionTest : public ::testing::Test {
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

TEST_F(VMTStdFunctionTest, BasicStdFunctionVTable) {
  const char *Code = R"(
    #include <functional>
    struct Base {
      virtual ~Base() = default;
      virtual void process() = 0;
    };
    struct Derived : Base {
      std::function<void()> handler;
      void process() override {
        if (handler) handler();
      }
    };
  )";

  auto AST = buildASTFromCode(Code);
  ASSERT_TRUE(AST != nullptr);

  auto TU = AST->getASTContext().getTranslationUnitDecl();
  auto *DerivedDecl = llvm::dyn_cast<CXXRecordDecl>(
    *TU->lookup(DeclarationName(&AST->getASTContext().Idents.get("Derived"))).begin());
  ASSERT_TRUE(DerivedDecl != nullptr);

  // Verify VMT layout
  EXPECT_TRUE(DerivedDecl->hasDefinition());
  EXPECT_TRUE(DerivedDecl->isDynamicClass());
}

TEST_F(VMTStdFunctionTest, StdFunctionWithTemplatedOperator) {
  const char *Code = R"(
    #include <functional>
    struct Base {
      virtual ~Base() = default;
      virtual std::function<void(int)> getHandler() = 0;
    };
    struct Derived : Base {
      template<typename T>
      struct Functor {
        template<typename U>
        void operator()(U x) { /* templated operator */ }
      };

      std::function<void(int)> getHandler() override {
        return Functor<int>();
      }
    };
  )";

  auto AST = buildASTFromCode(Code);
  ASSERT_TRUE(AST != nullptr);

  auto TU = AST->getASTContext().getTranslationUnitDecl();
  auto *DerivedDecl = llvm::dyn_cast<CXXRecordDecl>(
    *TU->lookup(DeclarationName(&AST->getASTContext().Idents.get("Derived"))).begin());
  ASSERT_TRUE(DerivedDecl != nullptr);

  // Verify VMT layout with templated operator
  EXPECT_TRUE(DerivedDecl->hasDefinition());
  EXPECT_TRUE(DerivedDecl->isDynamicClass());
}

} // end anonymous namespace
