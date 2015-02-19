//===-- rvexTargetMachine.h - Define TargetMachine for rvex -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the rvex specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef rvexTARGETMACHINE_H
#define rvexTARGETMACHINE_H

#include "rvexSubtarget.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetFrameLowering.h"

namespace llvm {
  class formatted_raw_ostream;

  class rvexTargetMachine : public LLVMTargetMachine {
    std::unique_ptr<TargetLoweringObjectFile> TLOF;
    rvexSubtarget       Subtarget;

  public:
    rvexTargetMachine(const Target &T, StringRef TT,
                      StringRef CPU, StringRef FS, const TargetOptions &Options,
                      Reloc::Model RM, CodeModel::Model CM,
                      CodeGenOpt::Level OL,
                      bool isLittle);

    const rvexSubtarget *getSubtargetImpl() const override { return &Subtarget; }

    // Pass Pipeline Configuration
    TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

    TargetLoweringObjectFile *getObjFileLowering() const override {
      return TLOF.get();
    }
  };

/// rvexebTargetMachine - rvex32 big endian target machine.
///
class rvexebTargetMachine : public rvexTargetMachine {
  virtual void anchor();
public:
  rvexebTargetMachine(const Target &T, StringRef TT,
                      StringRef CPU, StringRef FS, const TargetOptions &Options,
                      Reloc::Model RM, CodeModel::Model CM,
                      CodeGenOpt::Level OL);
};

/// rvexelTargetMachine - rvex32 little endian target machine.
///
class rvexelTargetMachine : public rvexTargetMachine {
  virtual void anchor();
public:
  rvexelTargetMachine(const Target &T, StringRef TT,
                      StringRef CPU, StringRef FS, const TargetOptions &Options,
                      Reloc::Model RM, CodeModel::Model CM,
                      CodeGenOpt::Level OL);
};
} // End llvm namespace

#endif
