add_llvm_library( LLVMHelloWord MODULE
  HelloWord.cpp

  PLUGIN_TOOL
  opt
  )

