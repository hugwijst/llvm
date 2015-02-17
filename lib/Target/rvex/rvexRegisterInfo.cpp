//===-- rvexRegisterInfo.cpp - rvex Register Information -== --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the rvex implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "rvex-reg-info"

#include "rvexRegisterInfo.h"
#include "rvex.h"
#include "rvexSubtarget.h"
#include "rvexMachineFunction.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Function.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/STLExtras.h"

#define GET_REGINFO_TARGET_DESC
#include "rvexGenRegisterInfo.inc"

using namespace llvm;

rvexRegisterInfo::rvexRegisterInfo(const rvexSubtarget &ST,
                                   const TargetInstrInfo &tii)
  : rvexGenRegisterInfo(rvex::LR), Subtarget(ST), TII(tii) {}

const TargetRegisterClass*
rvexRegisterInfo::getPointerRegClass(const MachineFunction &MF,
                                      unsigned Kind) const {
  return &rvex::CPURegsRegClass;
}

//===----------------------------------------------------------------------===//
// Callee Saved Registers methods
//===----------------------------------------------------------------------===//
/// rvex Callee Saved Registers
// In rvexCallConv.td,
// def CSR_O32 : CalleeSavedRegs<(add LR, FP,
//                                   (sequence "S%u", 2, 0))>;
// llc create CSR_O32_SaveList and CSR_O32_RegMask from above defined.
const uint16_t* rvexRegisterInfo::
getCalleeSavedRegs(const MachineFunction *MF) const
{
  return CSR_O32_SaveList;
}

const uint32_t*
rvexRegisterInfo::getCallPreservedMask(CallingConv::ID) const
{
  return CSR_O32_RegMask; 
}

BitVector rvexRegisterInfo::
getReservedRegs(const MachineFunction &MF) const {
  static const unsigned ReservedCPURegs[] = {
    //rvex::ZERO, rvex::AT, rvex::K0, rvex::K1, 
    //rvex::GP, rvex::SP, rvex::FP, rvex::RA, 0
    rvex::R0, rvex::R1, 
      rvex::R3, rvex::R4, rvex::R5, rvex::R6,
      rvex::R7, rvex::R8, rvex::R9, rvex::R10,    
    rvex::LR, rvex::R63, 0
  };

  BitVector Reserved(getNumRegs());
  typedef TargetRegisterClass::iterator RegIter;

  for (const unsigned *Reg = ReservedCPURegs; *Reg; ++Reg)
    Reserved.set(*Reg);

  return Reserved;
}

//- If eliminateFrameIndex() is empty, it will hang on run. 
// pure virtual method
// FrameIndex represent objects inside a abstract stack.
// We must replace FrameIndex with an stack/frame pointer
// direct reference.
void rvexRegisterInfo::
eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                    unsigned FIOperandNum, RegScavenger *RS) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineFrameInfo *MFI = MF.getFrameInfo();
  rvexFunctionInfo *rvexFI = MF.getInfo<rvexFunctionInfo>();

  unsigned i = 0;
  while (!MI.getOperand(i).isFI()) {
    ++i;
    assert(i < MI.getNumOperands() &&
           "Instr doesn't have FrameIndex operand!");
  }

  DEBUG(errs() << "\nFunction : " << MF.getFunction()->getName() << "\n";
        errs() << "<--------->\n" << MI);

  int FrameIndex = MI.getOperand(i).getIndex();
  uint64_t stackSize = MF.getFrameInfo()->getStackSize();
  int64_t spOffset = MF.getFrameInfo()->getObjectOffset(FrameIndex);

  DEBUG(errs() << "FrameIndex : " << FrameIndex << "\n"
               << "spOffset   : " << spOffset << "\n"
               << "stackSize  : " << stackSize << "\n");

  const std::vector<CalleeSavedInfo> &CSI = MFI->getCalleeSavedInfo();
  int MinCSFI = 0;
  int MaxCSFI = -1;

  if (CSI.size()) {
    MinCSFI = CSI[0].getFrameIdx();
    MaxCSFI = CSI[CSI.size() - 1].getFrameIdx();
  }

  // The following stack frame objects are always referenced relative to $sp:
  //  1. Outgoing arguments.
  //  2. Pointer to dynamically allocated stack space.
  //  3. Locations for callee-saved registers.
  // Everything else is referenced relative to whatever register
  // getFrameRegister() returns.
  unsigned FrameReg;

  if (rvexFI->isOutArgFI(FrameIndex) || rvexFI->isDynAllocFI(FrameIndex) ||
      (FrameIndex >= MinCSFI && FrameIndex <= MaxCSFI))
    FrameReg = rvex::R1;
  else
    FrameReg = getFrameRegister(MF);

  // Calculate final offset.
  // - There is no need to change the offset if the frame object is one of the
  //   following: an outgoing argument, pointer to a dynamically allocated
  //   stack space or a $gp restore location,
  // - If the frame object is any of the following, its offset must be adjusted
  //   by adding the size of the stack:
  //   incoming argument, callee-saved register location or local variable.
  int64_t Offset;
  if (rvexFI->isOutArgFI(FrameIndex) || rvexFI->isGPFI(FrameIndex) ||
      rvexFI->isDynAllocFI(FrameIndex))
    Offset = spOffset;
  else
    Offset = spOffset + (int64_t)stackSize;

  Offset    += MI.getOperand(i+1).getImm();

  DEBUG(errs() << "Offset     : " << Offset << "\n" << "<--------->\n");

  // If MI is not a debug value, make sure Offset fits in the 16-bit immediate
  // field.
  // if (!MI.isDebugValue() && !isInt<9>(Offset)) {
  //   DEBUG(errs() << "Load frame through register\n");
  //   //FIXME very ugly hack which uses R63 to calculate frameindex.
  //   assert("(!MI.isDebugValue() && !isInt<8>(Offset))");

  //   MachineBasicBlock &MBB = *MI.getParent();
  //   DebugLoc DL = II->getDebugLoc();

  //   MachineRegisterInfo &RegInfo = MBB.getParent()->getRegInfo();

  //   // unsigned Reg = RegInfo.createVirtualRegister(&rvex::CPURegsRegClass);
  //   unsigned Reg = MI.getOperand(i-1).getReg();

  //   BuildMI(MBB, II, DL, TII.get(rvex::MOV), Reg).addReg(Reg, RegState::Kill)
  //     .addImm(Offset);

  //   FrameReg = Reg;
  //   Offset = 0; 

    
  // }



  MI.getOperand(i).ChangeToRegister(FrameReg, false);
  MI.getOperand(i+1).ChangeToImmediate(Offset);
}

// pure virtual method
unsigned rvexRegisterInfo::
getFrameRegister(const MachineFunction &MF) const {
  //const TargetFrameLowering *TFI = MF.getTarget().getFrameLowering();
  return (rvex::R1);
}

