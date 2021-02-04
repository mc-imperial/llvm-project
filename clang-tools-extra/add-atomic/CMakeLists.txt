set(LLVM_LINK_COMPONENTS
  FrontendOpenMP
  Support
  )

add_clang_executable(add-atomic
  AddAtomic.cpp
  )

clang_target_link_libraries(add-atomic
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangRewrite
  clangSerialization
  clangTooling
  clangToolingRefactoring
  )
