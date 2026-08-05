# The LLVM Compiler Infrastructure

Non-speculative RISCV modification to the [LLVM Project](https://github.com/llvm/llvm-project).

## Overview of changes
- Added new BMOV/PBAL instructions to RISCV ISA
- Removed tablegen instruction selection for Pseudo branch instructions
- Added Pseudo branch instruction expansion after optimizations
	- See RISCVExpandPseudoInsts.cpp
- Added Branch Support Analysis pass (used to emit PBAL labels)
	- See RISCVBranchSupportAnalysis.{h,cpp} and RISCVAsmPrinter.cpp

## My CMake Setup
```bash
-G Ninja
-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++
-DCMAKE_BUILD_TYPE=Debug
"-DLLVM_ENABLE_PROJECTS=clang;lld"
-DLLVM_TARGETS_TO_BUILD=RISCV
-DLLVM_DEFAULT_TARGET_TRIPLE=riscv64-unknown-linux-gnu
-DCLANG_DEFAULT_RTLIB=compiler-rt
-DCLANG_DEFAULT_LINKER=lld
-DCLANG_DEFAULT_CXX_STDLIB=libc++
-DCLANG_DEFAULT_UNWINDLIB=libunwind
-DLLVM_RUNTIME_TARGETS=riscv64-unknown-linux-gnu
"-DLLVM_ENABLE_RUNTIMES=compiler-rt;libunwind;libc;libcxx;libcxxabi"
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBUNWIND_USE_COMPILER_RT=ON
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBUNWIND_ENABLE_SHARED=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBCXXABI_ENABLE_SHARED=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBCXX_ENABLE_SHARED=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBC_KERNEL_HEADERS=/home/mitchell/riscv-kernel-headers/include
"-DRUNTIMES_riscv64-unknown-linux-gnu_LLVM_ENABLE_RUNTIMES=compiler-rt;libunwind;libc;libcxx;libcxxabi"
-DRUNTIMES_riscv64-unknown-linux-gnu_LLVM_INCLUDE_TESTS=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_SANITIZERS=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_XRAY=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_LIBFUZZER=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_MEMPROF=OFF
```

## How to Build
```bash
ninja -C build llc llvm-mc llvm-objdump
```

## CMake variables for installation with MUSL
```
-G Ninja
-DCMAKE_BUILD_TYPE=Release
-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++
"-DLLVM_ENABLE_PROJECTS=clang;lld"
-DLLVM_TARGETS_TO_BUILD=RISCV
-DLLVM_DEFAULT_TARGET_TRIPLE=riscv64-unknown-linux-gnu
-DCLANG_DEFAULT_RTLIB=compiler-rt
-DCLANG_DEFAULT_LINKER=lld
-DCLANG_DEFAULT_CXX_STDLIB=libc++
-DCLANG_DEFAULT_UNWINDLIB=libunwind
-DLLVM_RUNTIME_TARGETS=riscv64-unknown-linux-gnu
"-DLLVM_ENABLE_RUNTIMES=compiler-rt;libunwind"
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBUNWIND_USE_COMPILER_RT=ON
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBUNWIND_ENABLE_SHARED=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBCXXABI_ENABLE_SHARED=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_LIBCXX_ENABLE_SHARED=OFF
"-DRUNTIMES_riscv64-unknown-linux-gnu_LLVM_ENABLE_RUNTIMES=compiler-rt;libunwind"
-DRUNTIMES_riscv64-unknown-linux-gnu_LLVM_INCLUDE_TESTS=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_SANITIZERS=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_XRAY=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_LIBFUZZER=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_MEMPROF=OFF
-DCMAKE_INSTALL_PREFIX=/home/mitchell/llvm-ns
-DLLVM_INSTALL_TOOLCHAIN_ONLY=ON
-DCOMPILER_RT_BUILD_SANITIZERS=OFF
-DCOMPILER_RT_BUILD_CRT=ON
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_CRT=ON
-DCOMPILER_RT_BUILD_PROFILE=OFF
-DCOMPILER_RT_BUILD_GWP_ASAN=OFF
-DCOMPILER_RT_BUILD_ORC=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_PROFILE=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_GWP_ASAN=OFF
-DRUNTIMES_riscv64-unknown-linux-gnu_COMPILER_RT_BUILD_ORC=OFF
```

List of Implicit LLVM Assumptions that are not documented anywhere:
- Branch instructions will always hold their branch target
- All terminators of a basic block will be at the end of a basic block
- All Phi instructions must be at the beginning of a basic block
- LLVM will just copy MachineInstr's as it pleases without letting you know

# NOTES TO SELF
- Try reverting back to old MachineBasicBlock.cpp, I don't think my changes matter anymore..

