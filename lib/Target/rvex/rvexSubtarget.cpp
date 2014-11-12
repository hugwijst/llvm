//===-- rvexSubtarget.cpp - rvex Subtarget Information --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the rvex specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "rvexSubtarget.h"
#include "rvex.h"
#include "rvexRegisterInfo.h"
#include "llvm/CodeGen/DFAPacketizer.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"

#include "MCTargetDesc/rvexSubtargetInfo.h"
#include "MCTargetDesc/rvexBuildDFA.h"

#define DEBUG_TYPE "rvex-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#include "rvexGenSubtargetInfo.inc"

using namespace llvm;

static cl::opt<bool>
IsLinuxOpt("rvex-islinux-format", cl::Hidden, cl::init(true),
                 cl::desc("Always use linux format."));

void rvexSubtarget::anchor() { }

rvexSubtarget::rvexSubtarget(const std::string &TT, const std::string &CPU,
                             const std::string &FS, bool little,
                             rvexTargetMachine &TM) :
  TargetSubtargetInfo(),
  InstrItins(), // We can initizialize InstrItins and SchedModel only after
  SchedModel(), // having called InitrvexMCSubtargetInfo
  DL(little ? ("e-p:32:32:32-i8:8:32-i16:16:32-i64:64:64-n32") :
              ("E-p:32:32:32-i8:8:32-i16:16:32-i64:64:64-n32")),
  InstrInfo(*this), TLInfo(TM), TSInfo(DL),
  FrameLowering(*this),
  rvexABI(UnknownABI), IsLittle(little), IsLinux(IsLinuxOpt)
{
  rvexUpdateSubtargetInfoFromConfig();

  InitrvexMCSubtargetInfo(this, TT, CPU, FS);

  std::string CPUName = CPU;
  if (CPUName.empty())
    CPUName = "rvex";

  InstrItins = getInstrItineraryForCPU(CPUName);
  SchedModel = getSchedModelForCPU(CPUName);

  // Parse features string.
  ParseSubtargetFeatures(CPUName, FS);

  // Set rvexABI if it hasn't been set yet.
  if (rvexABI == UnknownABI)
    rvexABI = O32;
}

 // bool rvexSubtarget::
 // enablePostRAScheduler(CodeGenOpt::Level OptLevel,
 //                       TargetSubtargetInfo::AntiDepBreakMode& Mode,
 //                       RegClassVector& CriticalPathRCs) const {
 //   Mode = TargetSubtargetInfo::ANTIDEP_NONE;
 //   CriticalPathRCs.clear();
 //   CriticalPathRCs.push_back(&rvex::CPURegsRegClass);
 //   return OptLevel >= CodeGenOpt::Default;
 // }

DFAPacketizer * rvexSubtarget::createDFAPacketizer(const InstrItineraryData *IID) const {
  DFAStateTables stateTables = rvexGetDFAStateTables();

  return new DFAPacketizer(IID, std::get<0>(stateTables), std::get<1>(stateTables));
}

