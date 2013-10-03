//===-- rvexISelDAGToDAG.cpp - A Dag to Dag Inst Selector for rvex --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the rvex target.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "rvex-isel"
#include "rvex.h"
#include "rvexMachineFunction.h"
#include "rvexRegisterInfo.h"
#include "rvexSubtarget.h"
#include "rvexTargetMachine.h"
#include "MCTargetDesc/rvexBaseInfo.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/CFG.h"
#include "llvm/IR/Type.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// rvexDAGToDAGISel - rvex specific code to select rvex machine
// instructions for SelectionDAG operations.
//===----------------------------------------------------------------------===//
namespace {

class rvexDAGToDAGISel : public SelectionDAGISel {

  /// TM - Keep a reference to rvexTargetMachine.
  rvexTargetMachine &TM;

  /// Subtarget - Keep a pointer to the rvexSubtarget around so that we can
  /// make the right decision when generating code for different targets.
  const rvexSubtarget &Subtarget;

public:
  explicit rvexDAGToDAGISel(rvexTargetMachine &tm) :
  SelectionDAGISel(tm),
  TM(tm), Subtarget(tm.getSubtarget<rvexSubtarget>()) {}

  // Pass Name
  virtual const char *getPassName() const {
    return "rvex DAG->DAG Pattern Instruction Selection";
  }

  //virtual bool runOnMachineFunction(MachineFunction &MF);

private:
  // Include the pieces autogenerated from the target description.
  #include "rvexGenDAGISel.inc"

  /// getTargetMachine - Return a reference to the TargetMachine, casted
  /// to the target-specific type.
  const rvexTargetMachine &getTargetMachine() {
    return static_cast<const rvexTargetMachine &>(TM);
  }

  /// getInstrInfo - Return a reference to the TargetInstrInfo, casted
  /// to the target-specific type.
  const rvexInstrInfo *getInstrInfo() {
    return getTargetMachine().getInstrInfo();
  }

  SDNode *getGlobalBaseReg();

  std::pair<SDNode*, SDNode*> SelectMULT(SDNode *N, unsigned Opc, DebugLoc dl,
                                         EVT Ty, bool HasLo, bool HasHi);

  std::pair<SDNode*, SDNode*> SelectADDC(SDNode *N, unsigned Opc, DebugLoc dl,
                                         EVT Ty, bool HasLo, bool HasHi);

  SDNode *Select(SDNode *N);
  // Complex Pattern.
  bool SelectAddr(SDNode *Parent, SDValue N, SDValue &Base, SDValue &Offset);
  // getImm - Return a target constant with the specified value.
  inline SDValue getImm(const SDNode *Node, unsigned Imm) {
    return CurDAG->getTargetConstant(Imm, Node->getValueType(0));
  }
  void InitGlobalBaseReg(MachineFunction &MF);
};
}
/*
bool rvexDAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  bool Ret = SelectionDAGISel::runOnMachineFunction(MF);

  return Ret;
}*/

/// getGlobalBaseReg - Output the instructions required to put the
/// GOT address into a register.
SDNode *rvexDAGToDAGISel::getGlobalBaseReg() {
  unsigned GlobalBaseReg = MF->getInfo<rvexFunctionInfo>()->getGlobalBaseReg();
  return CurDAG->getRegister(GlobalBaseReg, TLI.getPointerTy()).getNode();
}

/// ComplexPattern used on rvexInstrInfo
/// Used on rvex Load/Store instructions
bool rvexDAGToDAGISel::
SelectAddr(SDNode *Parent, SDValue Addr, SDValue &Base, SDValue &Offset) {
  EVT ValTy = Addr.getValueType();

  // If Parent is an unaligned f32 load or store, select a (base + index)
  // floating point load/store instruction (luxc1 or suxc1).
  const LSBaseSDNode* LS = 0;

  if (Parent && (LS = dyn_cast<LSBaseSDNode>(Parent))) {
    EVT VT = LS->getMemoryVT();

    if (VT.getSizeInBits() / 8 > LS->getAlignment()) {
      assert(TLI.allowsUnalignedMemoryAccesses(VT) &&
             "Unaligned loads/stores not supported for this type.");
      if (VT == MVT::f32)
        return false;
    }
  }

  // if Address is FI, get the TargetFrameIndex.
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), ValTy);
    Offset = CurDAG->getTargetConstant(0, ValTy);
    return true;
  }

  // on PIC code Load GA
  if (Addr.getOpcode() == rvexISD::Wrapper) {
    Base   = Addr.getOperand(0);
    Offset = Addr.getOperand(1);
    return true;
  }

  if (TM.getRelocationModel() != Reloc::PIC_) {
    if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
        Addr.getOpcode() == ISD::TargetGlobalAddress))
      return false;
  }

  Base   = Addr;
  Offset = CurDAG->getTargetConstant(0, ValTy);
  return true;
}

/// Select multiply instructions.
std::pair<SDNode*, SDNode*>
rvexDAGToDAGISel::SelectMULT(SDNode *N, unsigned Opc, DebugLoc dl, EVT Ty,
                             bool HasLo, bool HasHi) {
  SDNode *Lo = 0, *Hi = 0;
  SDNode *Mul = CurDAG->getMachineNode(Opc, dl, MVT::Glue, N->getOperand(0),
                                       N->getOperand(1));
  SDValue InFlag = SDValue(Mul, 0);

  if (HasLo) {
    Lo = CurDAG->getMachineNode(rvex::MFLO, dl,
                                Ty, MVT::Glue, InFlag);
    InFlag = SDValue(Lo, 1);
  }
  if (HasHi)
    Hi = CurDAG->getMachineNode(rvex::MFHI, dl,
                                Ty, InFlag);

  return std::make_pair(Lo, Hi);
}

std::pair<SDNode*, SDNode*>
rvexDAGToDAGISel::SelectADDC(SDNode *N, unsigned Opc, DebugLoc dl, EVT Ty,
                             bool HasLo, bool HasHi) {
  SDNode *Lo = 0, *Hi = 0;
  SDNode *Mul = CurDAG->getMachineNode(Opc, dl, MVT::Glue, N->getOperand(0),
                                       N->getOperand(1));
  SDValue InFlag = SDValue(Mul, 0);

  if (HasLo) {
    Lo = CurDAG->getMachineNode(rvex::MFLO, dl,
                                Ty, MVT::Glue, InFlag);
    InFlag = SDValue(Lo, 1);
  }
  if (HasHi)
    Hi = CurDAG->getMachineNode(rvex::MFHI, dl,
                                Ty, InFlag);

  return std::make_pair(Lo, Hi);
}

/// Select instructions not customized! Used for
/// expanded, promoted and normal instructions
SDNode* rvexDAGToDAGISel::Select(SDNode *Node) {
  unsigned Opcode = Node->getOpcode();
  DebugLoc dl = Node->getDebugLoc();

  // Dump information about the Node being selected
  DEBUG(errs() << "Selecting: "; Node->dump(CurDAG); errs() << "\n");

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    DEBUG(errs() << "== "; Node->dump(CurDAG); errs() << "\n");
    return NULL;
  }

  ///
  // Instruction Selection not handled by the auto-generated
  // tablegen selection should be handled here.
  ///
  EVT NodeTy = Node->getValueType(0);
  unsigned MultOpc;

  switch(Opcode) {
  default: break;


/*
  case ISD::ADDE: {
    SDValue InFlag = Node->getOperand(2), CmpLHS;
    unsigned Opc = InFlag.getOpcode(); (void)Opc;
    assert(((Opc == ISD::ADDC || Opc == ISD::ADDE) ||
            (Opc == ISD::SUBC || Opc == ISD::SUBE)) &&
           "(ADD|SUB)E flag operand must come from (ADD|SUB)C/E insn");

    unsigned MOp;
    if (Opcode == ISD::ADDE) {
      CmpLHS = InFlag.getValue(0);
      MOp = rvex::ADDC_T;
    } else {
      CmpLHS = InFlag.getOperand(0);
      MOp = rvex::ADDC_T;
    }

    SDValue Ops[] = { CmpLHS, InFlag.getOperand(1) };

    SDValue LHS = Node->getOperand(0);
    SDValue RHS = Node->getOperand(1);

    EVT VT = LHS.getValueType();

    unsigned Sltu_op = rvex::ADDC_T;
    SDNode *Carry = CurDAG->getMachineNode(Sltu_op, dl, VT, Ops);
    unsigned Addu_op = rvex::ADDC_T;
    SDNode *AddCarry = CurDAG->getMachineNode(Addu_op, dl, VT,
                                              SDValue(Carry,0), RHS);

    SDNode *Result = CurDAG->SelectNodeTo(Node, MOp, VT, MVT::Glue, LHS,
                                          SDValue(AddCarry,0));
    return (std::make_pair(true, Result));
  }
 */   

  case ISD::MULHS:
  case ISD::MULHU: {
    MultOpc = (Opcode == ISD::MULHU ? rvex::MULTu : rvex::MULT);
    return SelectMULT(Node, MultOpc, dl, NodeTy, false, true).second;
  }

  // Get target GOT address.
  // For global variables as follows,
  //- @gI = global i32 100, align 4
  //- %2 = load i32* @gI, align 4
  // =>
  //- .cpload	$gp
  //- ld	$2, %got(gI)($gp)
  case ISD::GLOBAL_OFFSET_TABLE:
    return getGlobalBaseReg();

  case ISD::Constant: {
    const ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Node);
    unsigned Size = CN->getValueSizeInBits(0);

    if (Size == 32)
      break;
  }
  }

  // Select the default instruction
  SDNode *ResNode = SelectCode(Node);

  DEBUG(errs() << "=> ");
  if (ResNode == NULL || ResNode == Node)
    DEBUG(Node->dump(CurDAG));
  else
    DEBUG(ResNode->dump(CurDAG));
  DEBUG(errs() << "\n");
  return ResNode;
}

/// creatervexISelDag - This pass converts a legalized DAG into a
/// rvex-specific DAG, ready for instruction scheduling.
FunctionPass *llvm::creatervexISelDag(rvexTargetMachine &TM) {
  return new rvexDAGToDAGISel(TM);
}
