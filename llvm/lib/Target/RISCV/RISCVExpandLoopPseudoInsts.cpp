//==- RISCVExpandLoopPseudo.cpp - Hoist BMOV insts. towards fn. entry -====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVBranchSetupAnalysis.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-expand-loop-pseudo"
#define PASS_NAME "RISC-V loop pseudo instruction expansion pass"

namespace {

class RISCVExpandLoopPseudo : public MachineFunctionPass {
public:
  static char ID;
  RISCVBranchSetupInfo *BSI;
  const TargetInstrInfo *TII;
  const TargetRegisterInfo *TRI;
  MachineLoopInfo *MLI;

  RISCVExpandLoopPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<RISCVBranchSetupAnalysisWrapper>();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesAll();
  }

  StringRef getPassName() const override {
    return PASS_NAME;
  }

  void expandLoop(MachineFunction &MF, MachineInstr &Setup, MachineInstr &End) {
    MachineBasicBlock &MBB = *Setup.getParent();
    MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
    Register BReg = MRI.createVirtualRegister(&RISCV::PBRRegClass);
    Register Count = Setup.getOperand(0).getReg();
    MachineBasicBlock *TBB = End.getOperand(0).getMBB();
    MCContext &Context = MF.getContext();
    MCSymbol* Sym = Context.createTempSymbol("ns_loop_");

    // Emit BMOVS B0, Sym
    MachineInstr *S = BuildMI(*Setup.getParent(), Setup.getIterator(),
      Setup.getDebugLoc(), TII->get(RISCV::BMOVS_J))
        .addDef(BReg)
        .addSym(Sym);

    // Emit BMOVT B0, TBB
    MachineInstr *T = BuildMI(*Setup.getParent(), Setup.getIterator(),
      Setup.getDebugLoc(), TII->get(RISCV::BMOVT_J))
        .addReg(BReg)
        .addMBB(TBB);

    // Emit BMOVC_LOOP B0, Count, 0
    MachineInstr *C = BuildMI(*Setup.getParent(), Setup.getIterator(),
      Setup.getDebugLoc(), TII->get(RISCV::BMOVC_LOOP))
        .addReg(BReg)
        .addReg(Count)
        .addImm(0);

    // Emit PBAL B0, RA (JALR X1, GPR:$rs1, 0)
    MachineInstr *PB = BuildMI(*End.getParent(), End.getIterator(),
      End.getDebugLoc(), TII->get(RISCV::PseudoPBC))
        .addReg(BReg)
        .addMBB(TBB);

    RISCVBranchSetup BranchSetup = RISCVBranchSetup { S, T, C };
    const_cast<RISCVBranchSetupInfo*>(BSI)->Branches.try_emplace(PB, BranchSetup);

    Setup.removeFromParent();
    End.removeFromParent();
  }
};

char RISCVExpandLoopPseudo::ID = 0;

static MachineInstr *findLoopSetup(MachineLoop &ML) {
  MachineBasicBlock *MBB = ML.getLoopPreheader();
  if (!MBB) return nullptr;
  for (MachineInstr &MI : *MBB) {
    if (MI.getOpcode() == RISCV::PseudoLoopSetup)
      return &MI;
  }
  return nullptr;
}
static MachineInstr *findLoopEnd(MachineLoop &ML) {
  MachineBasicBlock *MBB = ML.getLoopLatch();
  if (!MBB) return nullptr;
  for (MachineInstr &MI : *MBB) {
    if (MI.getOpcode() == RISCV::PseudoLoopEnd)
      return &MI;
  }
  return nullptr;
}

bool RISCVExpandLoopPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  BSI = &getAnalysis<RISCVBranchSetupAnalysisWrapper>().getInfo();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  bool Changed = false;

  int Count = 0;
  for (MachineLoop *ML : *MLI) {
    dbgs() << "Loop " << Count << ":\n";
    MachineInstr *Setup = findLoopSetup(*ML);
    MachineInstr *End = findLoopEnd(*ML);

    if (Setup || End)
      assert((Setup && End) && "Loop setup and end must come together.");
    if (!Setup || !End)
      continue;

    expandLoop(MF, *Setup, *End);
    Changed = true;
  }

  //MF.dump();
  return Changed;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandLoopPseudo, DEBUG_TYPE, PASS_NAME, false, false)

namespace llvm {

FunctionPass *createRISCVExpandLoopPseudoPass() {
  return new RISCVExpandLoopPseudo();
}

} // namespace llvm
