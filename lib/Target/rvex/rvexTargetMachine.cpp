//===-- rvexTargetMachine.cpp - Define TargetMachine for rvex -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements the info about rvex target spec.
//
//===----------------------------------------------------------------------===//

#include "rvexTargetMachine.h"
#include "rvex.h"
#include "llvm/PassManager.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/TargetRegistry.h"
#include "rvexMachineScheduler.h"
using namespace llvm;

extern "C" void LLVMInitializervexTarget() {
  // Register the target.
  //- Big endian Target Machine
  RegisterTargetMachine<rvexebTargetMachine> X(ThervexTarget);

}

// DataLayout --> Big-endian, 32-bit pointer/ABI/alignment
// The stack is always 8 byte aligned
// On function prologue, the stack is created by decrementing
// its pointer. Once decremented, all references are done with positive
// offset from the stack/frame pointer, using StackGrowsUp enables
// an easier handling.
// Using CodeModel::Large enables different CALL behavior.
rvexTargetMachine::
rvexTargetMachine(const Target &T, StringRef TT,
                  StringRef CPU, StringRef FS, const TargetOptions &Options,
                  Reloc::Model RM, CodeModel::Model CM,
                  CodeGenOpt::Level OL,
                  bool isLittle)
  //- Default is big endian
  : LLVMTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL),
    Subtarget(TT, CPU, FS, isLittle, *this)
     {
    initAsmInfo();
}

void rvexebTargetMachine::anchor() { }

rvexebTargetMachine::
rvexebTargetMachine(const Target &T, StringRef TT,
                    StringRef CPU, StringRef FS, const TargetOptions &Options,
                    Reloc::Model RM, CodeModel::Model CM,
                    CodeGenOpt::Level OL)
  : rvexTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL, false) {}

void rvexelTargetMachine::anchor() { }

static ScheduleDAGInstrs *createVLIWMachineSched(MachineSchedContext *C) {
  return new rvexVLIWMachineScheduler(C, make_unique<ConvergingrvexVLIWScheduler>());
}

static MachineSchedRegistry
SchedCustomRegistry("rvex", "Run rvex's custom scheduler",
                    createVLIWMachineSched);

rvexelTargetMachine::
rvexelTargetMachine(const Target &T, StringRef TT,
                    StringRef CPU, StringRef FS, const TargetOptions &Options,
                    Reloc::Model RM, CodeModel::Model CM,
                    CodeGenOpt::Level OL)
  : rvexTargetMachine(T, TT, CPU, FS, Options, RM, CM, OL, false) {}
namespace {
/// rvex Code Generator Pass Configuration Options.
class rvexPassConfig : public TargetPassConfig {
public:
  rvexPassConfig(rvexTargetMachine *TM, PassManagerBase &PM)
    : TargetPassConfig(TM, PM) {
      enablePass(&MachineSchedulerID);
    }

  rvexTargetMachine &getrvexTargetMachine() const {
    return getTM<rvexTargetMachine>();
  }

  const rvexSubtarget &getrvexSubtarget() const {
    return *getrvexTargetMachine().getSubtargetImpl();
  }
  bool addInstSelector() override;
  bool addPreEmitPass() override;
};
} // namespace

TargetPassConfig *rvexTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new rvexPassConfig(this, PM);
}

// Install an instruction selector pass using
// the ISelDag to gen rvex code.
bool rvexPassConfig::addInstSelector() {
  addPass(creatervexISelDag(getrvexTargetMachine()));
  return false;
}




bool rvexPassConfig::addPreEmitPass() {
  if(TM->getSubtarget<rvexSubtarget>().isVLIWEnabled()) {
    // addPass(creatervexPostRAScheduler());
    addPass(creatervexExpandPredSpillCode(getrvexTargetMachine()));    
    addPass(creatervexVLIWPacketizer());
    // addPass(CreateHelloPass(getrvexTargetMachine()));
  }

  return false;
}

