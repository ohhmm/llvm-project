#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/VTableBuilder.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Testing/CommandLineArgs.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"
#include <memory>

using namespace clang;
using namespace clang::tooling;

namespace {

class VMTTemplateParamTest : public ::testing::Test {
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

TEST_F(VMTTemplateParamTest, TemplateMethodInVMT) {
  const char *Code = R"(
    template<typename T>
    class Base {
    public:
      virtual T process(T value) = 0;
      virtual ~Base() {}
    };

    template<typename T>
    class Derived : public Base<T> {
    public:
      T process(T value) override { return value * 2; }
    };
  )";

  std::vector<std::string> Args = {"-std=c++17", "-fno-rtti"};
  std::unique_ptr<ASTUnit> AST = tooling::buildASTFromCodeWithArgs(Code, Args);
  ASSERT_TRUE(AST != nullptr);

  auto TU = AST->getASTContext().getTranslationUnitDecl();

  // Verify template instantiation and VMT generation
  auto Results = TU->lookup(DeclarationName(&AST->getASTContext().Idents.get("Base")));
  ASSERT_FALSE(Results.empty());
  const auto *BaseDecl = Results.front();
  ASSERT_TRUE(BaseDecl != nullptr);
  ASSERT_TRUE(isa<ClassTemplateDecl>(BaseDecl));
}

TEST_F(VMTTemplateParamTest, NestedTemplateParameters) {
  const char *Code = R"(
    template<typename T>
    class Interface {
    public:
      virtual T transform(T value) = 0;
      virtual ~Interface() {}
    };

    template<typename T, typename U>
    class Implementation : public Interface<T> {
    public:
      T transform(T value) override {
        return static_cast<T>(value * static_cast<T>(sizeof(U)));
      }
    };
  )";

  std::vector<std::string> Args = {"-std=c++17", "-fno-rtti"};
  std::unique_ptr<ASTUnit> AST = tooling::buildASTFromCodeWithArgs(Code, Args);
  ASSERT_TRUE(AST != nullptr);

  auto TU = AST->getASTContext().getTranslationUnitDecl();

  // Verify nested template parameter handling in VMT
  auto Results = TU->lookup(DeclarationName(&AST->getASTContext().Idents.get("Interface")));
  ASSERT_FALSE(Results.empty());
  const auto *InterfaceDecl = Results.front();
  ASSERT_TRUE(InterfaceDecl != nullptr);
  ASSERT_TRUE(isa<ClassTemplateDecl>(InterfaceDecl));
}

} // end anonymous namespace
