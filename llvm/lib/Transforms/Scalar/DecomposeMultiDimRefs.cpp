//===- llvm/Transforms/DecomposeMultiDimRefs.cpp - Lower array refs to 1D -===//
//
// DecomposeMultiDimRefs - Convert multi-dimensional references consisting of
// any combination of 2 or more array and structure indices into a sequence of
// instructions (using getelementpr and cast) so that each instruction has at
// most one index (except structure references, which need an extra leading
// index of [0]).
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Constant.h"
#include "llvm/iMemory.h"
#include "llvm/iOther.h"
#include "llvm/BasicBlock.h"
#include "llvm/Pass.h"
#include "Support/StatisticReporter.h"

static Statistic<> NumAdded("lowerrefs\t\t- New instructions added");

namespace {
  struct DecomposePass : public BasicBlockPass {
    virtual bool runOnBasicBlock(BasicBlock &BB);

  private:
    static bool decomposeArrayRef(BasicBlock::iterator &BBI);
  };

  RegisterOpt<DecomposePass> X("lowerrefs", "Decompose multi-dimensional "
                               "structure/array references");
}

Pass
*createDecomposeMultiDimRefsPass()
{
  return new DecomposePass();
}


// runOnBasicBlock - Entry point for array or structure references with multiple
// indices.
//
bool
DecomposePass::runOnBasicBlock(BasicBlock &BB)
{
  bool Changed = false;
  for (BasicBlock::iterator II = BB.begin(); II != BB.end(); ) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&*II))
      if (GEP->getNumIndices() >= 2) {
        Changed |= decomposeArrayRef(II); // always modifies II
        continue;
      }
    ++II;
  }
  return Changed;
}

// Check for a constant (uint) 0.
inline bool
IsZero(Value* idx)
{
  return (isa<ConstantInt>(idx) && cast<ConstantInt>(idx)->isNullValue());
}

// For any GetElementPtrInst with 2 or more array and structure indices:
// 
//      opCode CompositeType* P, [uint|ubyte] idx1, ..., [uint|ubyte] idxN
// 
// this function generates the foll sequence:
// 
//      ptr1   = getElementPtr P,         idx1
//      ptr2   = getElementPtr ptr1,   0, idx2
//      ...
//      ptrN-1 = getElementPtr ptrN-2, 0, idxN-1
//      opCode                 ptrN-1, 0, idxN  // New-MAI
// 
// Then it replaces the original instruction with this sequence,
// and replaces all uses of the original instruction with New-MAI.
// If idx1 is 0, we simply omit the first getElementPtr instruction.
// 
// On return: BBI points to the instruction after the current one
//            (whether or not *BBI was replaced).
// 
// Return value: true if the instruction was replaced; false otherwise.
// 
bool
DecomposePass::decomposeArrayRef(BasicBlock::iterator &BBI)
{
  GetElementPtrInst &GEP = cast<GetElementPtrInst>(*BBI);
  BasicBlock *BB = GEP.getParent();
  Value *LastPtr = GEP.getPointerOperand();

  // Remove the instruction from the stream
  BB->getInstList().remove(BBI);

  // The vector of new instructions to be created
  std::vector<Instruction*> NewInsts;

  // Process each index except the last one.
  User::const_op_iterator OI = GEP.idx_begin(), OE = GEP.idx_end();
  for (; OI+1 != OE; ++OI) {
    std::vector<Value*> Indices;
    
    // If this is the first index and is 0, skip it and move on!
    if (OI == GEP.idx_begin()) {
      if (IsZero(*OI)) continue;
    } else
      // Not the first index: include initial [0] to deref the last ptr
      Indices.push_back(Constant::getNullValue(Type::UIntTy));

    Indices.push_back(*OI);

    // New Instruction: nextPtr1 = GetElementPtr LastPtr, Indices
    LastPtr = new GetElementPtrInst(LastPtr, Indices, "ptr1");
    NewInsts.push_back(cast<Instruction>(LastPtr));
    ++NumAdded;
  }

  // Now create a new instruction to replace the original one
  //
  const PointerType *PtrTy = cast<PointerType>(LastPtr->getType());

  // Get the final index vector, including an initial [0] as before.
  std::vector<Value*> Indices;
  Indices.push_back(Constant::getNullValue(Type::UIntTy));
  Indices.push_back(*OI);

  Instruction *NewI = new GetElementPtrInst(LastPtr, Indices, GEP.getName());
  NewInsts.push_back(NewI);

  // Replace all uses of the old instruction with the new
  GEP.replaceAllUsesWith(NewI);

  // Now delete the old instruction...
  delete &GEP;

  // Insert all of the new instructions...
  BB->getInstList().insert(BBI, NewInsts.begin(), NewInsts.end());

  // Advance the iterator to the instruction following the one just inserted...
  BBI = NewInsts.back();
  ++BBI;
  return true;
}
