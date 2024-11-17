# VMTable Test Suite

This directory contains test cases for Virtual Method Table (VMT) functionality in Clang.

## Test Cases

### std-function-vmt.cpp
Tests VMT support for std::function types, particularly focusing on:
- Template parameter support in operator()
- Virtual method overrides using std::function
- Proper VMT layout with std::function members

### lambda-vmt.cpp
Tests VMT support for lambda types, covering:
- Template parameter support in lambda expressions
- Virtual method implementations using lambdas
- Multiple inheritance scenarios with lambda expressions

## Running Tests
```bash
ninja check-clang-ast
```

## Expected Results
All tests should pass without warnings or errors.
