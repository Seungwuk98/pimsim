#include "pimsim/TypeSupport.h"
#include "llvm/ADT/FunctionExtras.h"
#include "llvm/ADT/StringSwitch.h"

namespace pimsim {

std::tuple<
    size_t,
    llvm::function_ref<void(llvm::raw_ostream &os, llvm::ArrayRef<Byte>)>,
    llvm::function_ref<bool(llvm::ArrayRef<Byte>, llvm::StringRef,
                            llvm::raw_ostream &)>,
    bool>
parseType(llvm::StringRef typeStr, Context *ctx) {
  return llvm::StringSwitch<std::tuple<
      size_t,
      llvm::function_ref<void(llvm::raw_ostream & os, llvm::ArrayRef<Byte>)>,
      llvm::function_ref<bool(llvm::ArrayRef<Byte>, llvm::StringRef,
                              llvm::raw_ostream &)>,
      bool>>(typeStr)

#define CASE_TYPE(TYPE, isInt)                                                 \
  Case(#TYPE, {sizeof(TYPE), TypeSupport<TYPE>::print,                         \
               TypeSupport<TYPE>::verify, isInt})

      .CASE_TYPE(u8, true)
      .CASE_TYPE(i8, true)
      .CASE_TYPE(u16, true)
      .CASE_TYPE(i16, true)
      .CASE_TYPE(u32, true)
      .CASE_TYPE(i32, true)
      .CASE_TYPE(u64, true)
      .CASE_TYPE(i64, true)
      .CASE_TYPE(f16, false)
      .CASE_TYPE(bf16, false)
      .CASE_TYPE(f32, false)
      .CASE_TYPE(f64, false)
      .Default(std::make_tuple(0, nullptr, nullptr, false));
}

const llvm::fltSemantics &parseFloatSemantics(llvm::StringRef typeStr) {
  auto semantics = llvm::StringSwitch<const llvm::fltSemantics *>(typeStr)
                       .Case("f16", TypeSupport<f16>::getSemantics())
                       .Case("bf16", TypeSupport<bf16>::getSemantics())
                       .Case("f32", TypeSupport<float>::getSemantics())
                       .Case("f64", TypeSupport<double>::getSemantics())
                       .Default(nullptr);
  assert(semantics && "Unssupported floating point type");
  return *semantics;
}

} // namespace pimsim
