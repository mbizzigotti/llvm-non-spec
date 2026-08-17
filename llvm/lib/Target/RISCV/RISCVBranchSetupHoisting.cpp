//==- RISCVBranchSetupHoisting.cpp - Hoist BMOV insts. towards fn. entry -====//
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

using namespace llvm;

#define DEBUG_TYPE "riscv-branch-setup-hoisting"
#define PASS_NAME "RISC-V branch setup hoisting pass"

static cl::opt<bool> DisableBranchSetupHoisting(
    "disable-branch-setup-hoisting", cl::Hidden,
    cl::desc("Disable " PASS_NAME),
    cl::init(true));

namespace {

class RISCVBranchSetupHoisting : public MachineFunctionPass {
public:
  static char ID;
  const RISCVBranchSetupInfo *BSI;
  const TargetInstrInfo *TII;
  const TargetRegisterInfo *TRI;
  MachineDominatorTree *MDT;
  MachineLoopInfo *MLI;

  RISCVBranchSetupHoisting() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<RISCVBranchSetupAnalysisWrapper>();
    AU.addRequired<MachineDominatorTreeWrapperPass>();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.setPreservesAll();
  }

  StringRef getPassName() const override {
    return PASS_NAME;
  }

  bool scheduleBranchSetup(MachineInstr &MI, MachineInstr *S, MachineInstr *T,
                           MachineInstr *C);
  MachineBasicBlock::iterator findEarliestSafePoint(MachineInstr *SetupMI,
                                                    MachineInstr &BranchMI);
};

char RISCVBranchSetupHoisting::ID = 0;

bool RISCVBranchSetupHoisting::runOnMachineFunction(MachineFunction &MF) {
  if (DisableBranchSetupHoisting)
    return false;

  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  BSI = &getAnalysis<RISCVBranchSetupAnalysisWrapper>().getInfo();
  MDT = &getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  // Fail early for functions with too many branches that would
  // require proper register allocation
  if (BSI->Branches.size() > 30)
    return false;

  BSI->dump(MF);

  bool Changed = false;
  for (auto &MBB : MF) {
    // Scan backwards from the end of the block to find branches
    for (auto MII = MBB.rbegin(), MIE = MBB.rend(); MII != MIE; ) {
      MachineInstr &MI = *MII++;
      if (RISCVBranchSetup BS = BSI->Branches.lookup(&MI)) {
        MachineInstr *S = const_cast<MachineInstr *>(BS.S);
        MachineInstr *T = const_cast<MachineInstr *>(BS.T);
        MachineInstr *C = const_cast<MachineInstr *>(BS.C);
        Changed |= scheduleBranchSetup(MI, S, T, C);
      }
    }
  }
  BSI->dump(MF);
  return Changed;
}

} // end of anonymous namespace

bool RISCVBranchSetupHoisting::scheduleBranchSetup(MachineInstr &MI,
                                                   MachineInstr *S,
                                                   MachineInstr *T,
                                                   MachineInstr *C) {
  MachineBasicBlock::iterator InsertPt;
  assert(S && "Branch must have BMOVS");
  assert(T && "Branch must have BMOVT");
  bool Changed = false;

  LLVM_DEBUG(dbgs() << "Setup for:"; MI.dump());

  InsertPt = findEarliestSafePoint(S, MI);
  if (InsertPt != S->getIterator()) {
    InsertPt->getParent()->splice(InsertPt, S->getParent(), S);
    Changed = true;
  }
  InsertPt = findEarliestSafePoint(T, MI);
  if (InsertPt != T->getIterator()) {
    InsertPt->getParent()->splice(InsertPt, T->getParent(), T);
    Changed = true;
  }
  if (C) {
    InsertPt = findEarliestSafePoint(C, MI);
    if (InsertPt != C->getIterator()) {
      InsertPt->getParent()->splice(InsertPt, C->getParent(), C);
      Changed = true;
    }
  }
  return Changed;
}

static bool redefinesSourceRegs(const MachineInstr &MI,
                                const MachineInstr &SetupMI,
                                const TargetRegisterInfo *TRI) {
  for (const MachineOperand &MO : SetupMI.explicit_uses()) {
    if (MO.isReg() && MI.modifiesRegister(MO.getReg(), TRI))
      return true;
  }
  return false;
}

#if 0
static bool isClobberedByCall(const MachineInstr &CallMI,
                              const MachineInstr &SetupMI) {
  for (const MachineOperand &MO : CallMI.operands()) {
    if (MO.isRegMask()) {
      // Check if b0 or any source reg in SetupMI is clobbered by call mask
      for (const MachineOperand &Use : SetupMI.operands()) {
        if (Use.isReg() && MO.clobbersPhysReg(Use.getReg()))
          return true;
      }
    }
  }
  return false;
}
#endif

MachineBasicBlock::iterator RISCVBranchSetupHoisting::findEarliestSafePoint(
  MachineInstr *SetupMI, MachineInstr &BranchMI) {
  MachineBasicBlock *CurBB = SetupMI->getParent();
  MachineBasicBlock::iterator SafePoint = SetupMI->getIterator();

  LLVM_DEBUG(SetupMI->dump());

  // Track physical register uses/defs in SetupMI (e.g., b0, x1, x2)
  //Register DestReg = SetupMI->getOperand(0).getReg(); // b0

  // Walk backwards through instructions to find dependencies/hazards
  for (MachineBasicBlock::reverse_iterator I = std::next(SetupMI->getReverseIterator()), E = CurBB->rend(); I != E; ++I) {
    MachineInstr &CurrMI = *I;

    // HAZARD 1: Does CurrMI redefine any source registers used by SetupMI?
    if (redefinesSourceRegs(CurrMI, *SetupMI, TRI)) {
      LLVM_DEBUG(dbgs() << "Source hazard with:"; CurrMI.dump());
      break;
    }

    // HAZARD 2: Is CurrMI a CALL that clobbers DestReg or SetupMI's sources?
    if (CurrMI.isCall()) {
      LLVM_DEBUG(dbgs() << "Call hazard with:"; CurrMI.dump());
      // if (isClobberedByCall(CurrMI, *SetupMI, TRI))
      break; // Call boundary reached, stop hoisting across this call
    }

    // Safe to move above CurrMI
    SafePoint = CurrMI.getIterator();
  }

  // TODO: Cross Basic Block Hoisting using MDT
  // If SafePoint reached top of CurBB, check parent Dominator nodes...
  return SafePoint;
}

INITIALIZE_PASS(RISCVBranchSetupHoisting, "riscv-branch-setup-hoisting",
                PASS_NAME,
                false, // is CFG only?
                false  // is analysis?
)

namespace llvm {

FunctionPass *createRISCVBranchSetupHoistingPass() {
  return new RISCVBranchSetupHoisting();
}

} // namespace llvm
