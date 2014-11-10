
//===-- rvexMCTargetDesc.cpp - rvex Target Descriptions ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//                               rvex Backend
//
// Author: David Juhasz
// E-mail: juhda@caesar.elte.hu
// Institute: Dept. of Programming Languages and Compilers, ELTE IK, Hungary
//
// The research is supported by the European Union and co-financed by the
// European Social Fund (grant agreement no. TAMOP
// 4.2.1./B-09/1/KMR-2010-0003).
//
// This file provides rvex specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "rvexMCTargetDesc.h"
#include "rvexMCAsmInfo.h"
#include "llvm/MC/MCCodeGenInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/TargetRegistry.h"

#include "llvm/Support/CommandLine.h"

#include "rvexFlags.h"

#include "rvexReadConfig.h"
#include "rvexBuildDFA.h"
#include <iostream>

#define GET_INSTRINFO_MC_DESC
#include "rvexGenInstrInfo.inc"

  namespace llvm {
  // Sorted (by key) array of values for CPU features.
  extern const llvm::SubtargetFeatureKV rvexFeatureKV[] = {
    { "vliw", "Enable VLIW scheduling", rvex::FeatureVLIW, 0ULL }
  };

  // Sorted (by key) array of values for CPU subtype.
  extern const llvm::SubtargetFeatureKV rvexSubTypeKV[] = {
    { "rvex", "Select the rvex processor", 0ULL, 0ULL },
    { "rvex-vliw", "Select the rvex-vliw processor", rvex::FeatureVLIW, 0ULL }
  };

  #ifdef DBGFIELD
  #error "<target>GenSubtargetInfo.inc requires a DBGFIELD macro"
  #endif
  #ifndef NDEBUG
  #define DBGFIELD(x) x,
  #else
  #define DBGFIELD(x)
  #endif

  namespace rvexGenericItinerariesFU {
    const unsigned P0 = 1 << 0;
    const unsigned P1 = 1 << 1;
  }


  llvm::InstrStage rvexStages[] = {
    { 0, 0, 0, llvm::InstrStage::Required }, // No itinerary
    { 1, 15, -1, (llvm::InstrStage::ReservationKinds)0 }, // 1
    { 1, 15, -1, (llvm::InstrStage::ReservationKinds)0 }, // 15
    { 1, 15, -1, (llvm::InstrStage::ReservationKinds)0 }, // 1
    { 1, 15, -1, (llvm::InstrStage::ReservationKinds)0 }, // 1
    { 1, 15, -1, (llvm::InstrStage::ReservationKinds)0 }, // 1
    { 1, 15, -1, (llvm::InstrStage::ReservationKinds)0 }, // 1
    { 1, 15, -1, (llvm::InstrStage::ReservationKinds)0 }, // 1
    { 0, 0, 0, llvm::InstrStage::Required } // End stages
  };  
  
  //llvm::InstrStage rvexStages[4];

  // Functional units for "rvexGenericItineraries"
  namespace rvexGenericItinerariesFU2 {
    const unsigned P0 = 1 << 0;
    const unsigned P1 = 1 << 1;
    const unsigned P2 = 1 << 2;
    const unsigned P3 = 1 << 3;
    const unsigned P4 = 1 << 4;
    const unsigned P5 = 1 << 5;
    const unsigned P6 = 1 << 6;
    const unsigned P7 = 1 << 7;
  }

  extern const unsigned rvexOperandCycles[] = {
    0, // No itinerary
    0 // End operand cycles
  };
  extern const unsigned rvexForwardingPaths[] = {
   0, // No itinerary
   0 // End bypass tables
  };

  static const llvm::InstrItinerary *NoItineraries = 0;

  static llvm::InstrItinerary rvexGenericItineraries[] = {
    { 0, 0, 0, 0, 0 }, // 0 NoInstrModel
    { 1, 1, 2, 0, 0 }, // 1 IIAlu
    { 1, 2, 3, 0, 0 }, // 2 IIAluImm
    { 1, 1, 2, 0, 0 }, // 3 IIPseudo
    { 1, 1, 2, 0, 0 }, // 4 IIBranch
    { 1, 2, 3, 0, 0 }, // 5 IILoadStore
    { 1, 1, 2, 0, 0 }, // 6 IIImul
    { 0, ~0U, ~0U, ~0U, ~0U } // end marker
  };

  // ===============================================================
  // Data tables for the new per-operand machine model.

  // {ProcResourceIdx, Cycles}
  extern const llvm::MCWriteProcResEntry rvexWriteProcResTable[] = {
    { 0,  0}, // Invalid
  }; // rvexWriteProcResTable

  // {Cycles, WriteResourceID}
  extern const llvm::MCWriteLatencyEntry rvexWriteLatencyTable[] = {
    { 0,  0}, // Invalid
  }; // rvexWriteLatencyTable

  // {UseIdx, WriteResourceID, Cycles}
  extern const llvm::MCReadAdvanceEntry rvexReadAdvanceTable[] = {
    {0,  0,  0}, // Invalid
  }; // rvexReadAdvanceTable

  static const llvm::MCSchedModel NoSchedModel = llvm::MCSchedModel::GetDefaultSchedModel();

  static llvm::MCSchedModel rvexModel = {
    8, // IssueWidth
    MCSchedModel::DefaultMicroOpBufferSize,
    MCSchedModel::DefaultLoopMicroOpBufferSize,
    2, // LoadLatency
    MCSchedModel::DefaultHighLatency,
    MCSchedModel::DefaultMispredictPenalty,
    false,
    true,
    1, // Processor ID
    nullptr, nullptr, 0, 0, // No instruction-level machine model.
    rvexGenericItineraries};

  // Sorted (by key) array of itineraries for CPU subtype.
  extern const llvm::SubtargetInfoKV rvexProcSchedKV[] = {
    { "rvex", (const void *)&rvexModel },
    { "rvex-vliw", (const void *)&rvexModel }
  };
  #undef DBGFIELD
  static inline void InitrvexMCSubtargetInfo(MCSubtargetInfo *II, StringRef TT, StringRef CPU, StringRef FS) {
    II->InitMCSubtargetInfo(TT, CPU, FS, rvexFeatureKV, rvexSubTypeKV, 
                        rvexProcSchedKV, rvexWriteProcResTable, rvexWriteLatencyTable, rvexReadAdvanceTable, 
                        rvexStages, rvexOperandCycles, rvexForwardingPaths);
  }

} // End llvm namespace 

#define GET_REGINFO_MC_DESC
#include "rvexGenRegisterInfo.inc"

using namespace llvm;

static MCInstrInfo *creatervexMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitrvexMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *creatervexMCRegisterInfo(StringRef TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitrvexMCRegisterInfo(X, rvex::R0); //TODO: is that right?
  return X;
}

static MCSubtargetInfo *creatervexMCSubtargetInfo(StringRef TT, StringRef CPU,
                                                    StringRef FS) {
  MCSubtargetInfo *X = new MCSubtargetInfo();
  InitrvexMCSubtargetInfo(X, TT, CPU, FS);
  return X;
}

static MCCodeGenInfo *creatervexMCCodeGenInfo(StringRef TT, Reloc::Model RM,
                                                CodeModel::Model CM,
                                                CodeGenOpt::Level OL) {
  MCCodeGenInfo *X = new MCCodeGenInfo();
  X->InitMCCodeGenInfo(RM, CM, OL);
  return X;
}

cl::opt<std::string>
Config("config", cl::desc("Path to config file"));

// cl::opt<bool> Is_Generic_flag ("f", cl::desc("Generate generic binary"));

bool Is_Generic_flag;
static cl::opt<bool, true> Generic ("f", cl::desc("Generate generic binary"), cl::location(Is_Generic_flag));

extern "C" void LLVMInitializervexTargetMC() {
  // Register the MC asm info.
  int i;

  // Read configuration file
  if (!read_config(Config))
  {
    //Init InstrStages from config file
    rvexModel.IssueWidth = is_width;
    
    if (is_generic == 1)
      Is_Generic_flag = true;
    else
      Is_Generic_flag = false;

    
    for (i = 0; i < (int)Stages.size(); i++)
    {
      //llvm::InstrStage TempStage = {Stages[i].num1, Stages[i].num2, -1, (llvm::InstrStage::ReservationKinds)1 };
      rvexStages[i+1].Cycles_ = Stages[i].delay;
      rvexStages[i+1].Units_ = Stages[i].FU;
      //rvexStages[i+1] = TempStage;
    }
    rvexStages[i+1].Cycles_ = 0;
    rvexStages[i+1].Units_ = 0; 
    rvexStages[i+1].NextCycles_ = 0;
    rvexStages[i+1].Kind_ = llvm::InstrStage::Required;
    

    // Init InstrItin from config file
    for (i = 0; i < (int)Itin.size(); i++)
    {
      llvm::InstrItinerary TempItin = {0, static_cast<unsigned int>(Itin[i].num1),
          static_cast<unsigned int>(Itin[i].num2), 0, 0};
      rvexGenericItineraries[i + 1] = TempItin;
    }
    llvm::InstrItinerary TempItin = { 0, ~0U, ~0U, ~0U, ~0U }; // end marker
    rvexGenericItineraries[i + 1] = TempItin;
  }

  // Build VLIW DFA from config stages
  rvexBuildDFA(Stages);

  RegisterMCAsmInfo<rvexELFMCAsmInfo> X(ThervexTarget);

  // Register the MC codegen info.
  TargetRegistry::RegisterMCCodeGenInfo(ThervexTarget,
                                        creatervexMCCodeGenInfo);

  // Register the MC instruction info.
  TargetRegistry::RegisterMCInstrInfo(ThervexTarget, creatervexMCInstrInfo);

  // Register the MC register info.
  TargetRegistry::RegisterMCRegInfo(ThervexTarget, creatervexMCRegisterInfo);

  // Register the MC subtarget info.
  TargetRegistry::RegisterMCSubtargetInfo(ThervexTarget,
                                          creatervexMCSubtargetInfo);
}
