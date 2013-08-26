//===-- rvexSubtarget.h - Define Subtarget for the rvex ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the rvex specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef rvexSUBTARGET_H
#define rvexSUBTARGET_H

#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/MC/MCInstrItineraries.h"
#include <string>

#define GET_SUBTARGETINFO_HEADER
#include "rvexGenSubtargetInfo.inc"

namespace llvm {
class StringRef;

  extern const int rvexDFAStateInputTable[][2];

  extern const unsigned int rvexDFAStateEntryTable[];

class rvexSubtarget : public rvexGenSubtargetInfo {
  virtual void anchor();

  bool IsVLIWEnabled;
  InstrItineraryData InstrItins;

  const MCSchedModel *SchedModel;

public:

  bool isVLIWEnabled() const { return IsVLIWEnabled; }
  // NOTE: O64 will not be supported.
  enum rvexABIEnum {
    UnknownABI, O32
  };

protected:
  enum rvexArchEnum {
    rvex32
  };

  // rvex architecture version
  rvexArchEnum rvexArchVersion;

  // rvex supported ABIs
  rvexABIEnum rvexABI;

  // IsLittle - The target is Little Endian
  bool IsLittle;

  // isLinux - Target system is Linux. Is false we consider ELFOS for now.
  bool IsLinux;

  

public:
  unsigned getTargetABI() const { return rvexABI; }

  /// This constructor initializes the data members to match that
  /// of the specified triple.
  rvexSubtarget(const std::string &TT, const std::string &CPU,
                const std::string &FS, bool little);

  const InstrItineraryData &getInstItineraryData() const { return InstrItins; }

//- Vitual function, must have
  /// ParseSubtargetFeatures - Parses features string setting specified
  /// subtarget options.  Definition of function is auto generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);

  bool isLittle() const { return IsLittle; }
  bool isLinux() const { return IsLinux; }
};
} // End llvm namespace

#endif
