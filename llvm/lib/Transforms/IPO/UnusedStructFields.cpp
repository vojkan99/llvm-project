#include "llvm/Transforms/IPO/UnusedStructFields.h"
#include "../lib/IR/ConstantsContext.h"
#include "../lib/IR/LLVMContextImpl.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
// #include "llvm/IR/DerivedUser.h"
// #include "llvm/IR/Operator.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// #define DEBUG_TYPE "unusedstruct"

using namespace llvm;

// Creates new global variable.
GlobalVariable *createNewGlobalVar(Module &M, const GlobalVariable &Var,
                                   StructType *NewStruct,
                                   Constant *Initializer) {
  DataLayout DL(&M);

  std::string NewVarName = Var.getName().str().append("_modified");

  M.getOrInsertGlobal(NewVarName, NewStruct);
  GlobalVariable *NewGlobalVar = M.getNamedGlobal(NewVarName);

  NewGlobalVar->setConstant(Var.isConstant());
  NewGlobalVar->setLinkage(Var.getLinkage());
  NewGlobalVar->setInitializer(Initializer);
  NewGlobalVar->setExternallyInitialized(Var.isExternallyInitialized());
  NewGlobalVar->setAttributes(Var.getAttributes());
  NewGlobalVar->setSection(Var.getSection());
  NewGlobalVar->setAlignment(DL.getPrefTypeAlign(NewStruct));
  NewGlobalVar->setComdat(const_cast<Comdat *>(Var.getComdat()));
  // NewGlobalVar->setGlobalValueSubClassData(Var.getGlobalValueSubClassData());
  // // protected pa ne moze samo ovako, mogla bi friend klasa da se stavi
  // GlobalValue recimo
  NewGlobalVar->copyAttributesFrom(&Var);
  SmallVector<DIGlobalVariableExpression *, 1> GVEs;
  Var.getDebugInfo(GVEs);
  for (unsigned i = 0, e = GVEs.size(); i != e; ++i)
    NewGlobalVar->addDebugInfo(GVEs[i]);

  return NewGlobalVar;
}

// Change source element type for getelementptr constant expressions.
void changeSourceElementTypeForGEPCEs(Module &M, std::string StructName,
                                      std::vector<unsigned> Indices,
                                      StructType *NewStruct) {
  std::unordered_set<Constant *> InvalidGEPCEs;
  for (Function &F : M)
    for (BasicBlock &BB : F)
      for (Instruction &I : BB) {
        unsigned Cnt = -1;
        for (auto &O : I.operands()) {
          Cnt++;
          if (&*O && !isa<Instruction>(&*O))
            if (auto *GEPCE = dyn_cast<GetElementPtrConstantExpr>(&*O))
              if (auto *GEPCEType =
                      dyn_cast<StructType>(GEPCE->getSourceElementType()))
                if (GEPCEType->getName().str().compare(StructName) == 0) {
                  // errs() <<
                  // M.getContext().pImpl->ExprConstants.getMap().count(
                  //               GEPCE)
                  //        << '\n';
                  // errs() << GEPCE << '\n';
                  // I.dump();
                  // errs() << "Start\n";
                  // for (auto elem : GEPCE->materialized_users())
                  //   elem->dump();
                  // errs() << "End\n";
                  GetElementPtrConstantExpr *NewGEPCE =
                      M.getContext()
                          .pImpl->ExprConstants
                          .updateGEPCEWithNewSourceTypeAndIndice(
                              GEPCE, NewStruct, Indices, InvalidGEPCEs);

                  I.setOperand(Cnt, NewGEPCE);
                  // if (isa<LoadInst>(I)) {
                  //   errs() << "change\n";
                  //   I.getOperand(cnt)->dump();
                  //   NewGEPCE->dump();
                  // }

                  // GEPCE = NewGEPCE;
                  // GEPCE->setSourceElementType(GEPCE->getSourceElementType());
                  // errs() <<
                  // M.getContext().pImpl->ExprConstants.getMap().count(
                  //     GEPCE) << '\n';
                  // errs() << '\n';
                  // errs() << GEPCE << '\n';
                  // errs() << GEPCEType->getName().str() << ' ' << StructName
                  //        << '\n';
                }
        }
      }

  // Remove old invalid GEPCEs.
  M.getContext().pImpl->ExprConstants.removeOldGEPCEs(M, InvalidGEPCEs);
}

uint64_t findFirstLarger(uint64_t Num, uint64_t Factor) {
  if (Num < Factor)
    return Factor;
  else
    return Num + Factor - (Num % Factor);
}

std::string getGVTypeName(Module &M, StringRef GVName) {
  for (const auto &GV : M.globals())
    if (isa<GlobalVariable>(GV) &&
        GV.getName().str().compare(GVName.str()) == 0)
      if (auto *ST = dyn_cast<StructType>(
              GV.getValueType())) {
        // GV.dump();
        // errs() << "Type " << ST->getName().str() << '\n';
        // errs() << "Struct name: " << ST->getName().str() << '\n';
        if (ST->getName().str().substr(0, 7).compare("struct.") == 0)
          return ST->getName().str().substr(7);
      }
  return "";
}

// VarScope == true means that DIGlobalVariable is passed, while VarScope ==
// false means that DILocalVariable is passed.
bool createNewCompositeType(
    DIBuilder &DB, Module &M, DIVariable *V, std::string FuncName,
    std::unordered_map<std::string, std::vector<unsigned>> &FieldUsage,
    bool VarScope) {
  // DINodeArray Elements = DINodeArray();
  // std::string StructName = "";
  // DICompositeType *CT = nullptr;
  // unsigned State = 0;
  // if (isa<DIDerivedType>(V->getType()) &&
  //     V->getType()->getTag() == dwarf::DW_TAG_typedef) {
  //   if ((CT = dyn_cast<DICompositeType>(
  //            (dyn_cast<DIDerivedType>(V->getType()))
  //                ->getBaseType()))) {
  //     StructName = CT->getName().str();

  //     // If the composite type does not have a name, get the name
  //     of
  //     // the struct from GlobalVariable instead of
  //     DIGlobalVariable.
  //     // If there is a typedef attached to this composite type, its
  //     // name will be the name of the struct, otherwise anon is
  //     used. if (StructName.length() == 0)
  //       StructName = getGVTypeName(M, V->getName());

  //     Elements = CT->getElements();
  //     State = 1;
  //   }
  // } else if ((CT = dyn_cast<DICompositeType>(V->getType()))) {
  //   StructName = CT->getName().str();

  //   // If the composite type does not have a name, get the name of
  //   the
  //   // struct from GlobalVariable instead of DIGlobalVariable. If
  //   // there is a typedef attached to this composite type, its name
  //   // will be the name of the struct, otherwise anon is used.
  //   if (StructName.length() == 0)
  //     StructName = getGVTypeName(M, V->getName());

  //   Elements = CT->getElements();
  //   State = 2;
  // }

  // // errs() << '\n';

  // // Create new DICompositeType node and its members with correct
  // // sizes and offsets.
  // if (Elements.size() > 0) {
  //   errs() << StructName << '\n';
  //   auto E = FieldUsage.find("struct." + StructName);
  //   if (E == FieldUsage.end())
  //     continue;

  //   std::vector<unsigned> NewVect =
  //       FieldUsage["struct." + StructName];
  //   std::vector<Metadata *> NewElements;

  //   unsigned Cnt = 0;
  //   uint64_t Offset = 0;
  //   // Alignment of the new structure.
  //   uint64_t MaxAlignInBits = 0;

  //   for (auto *Field : Elements) {
  //     if (NewVect[Cnt] > 0) {
  //       // errs() <<
  //       (dyn_cast<DIDerivedType>(Field))->getSizeInBits()
  //       // << '\n'; errs() <<
  //       // (dyn_cast<DIDerivedType>(Field))->getOffsetInBits() <<
  //       // '\n';
  //       if (auto *Member = dyn_cast<DIDerivedType>(Field)) {
  //         uint64_t AlignInBits =
  //             Member->getBaseType()->getSizeInBits();
  //         errs() << AlignInBits << "\n\n";
  //         if (AlignInBits > MaxAlignInBits)
  //           MaxAlignInBits = AlignInBits;

  //         // If the current offset in the new structure is valid
  //         for
  //         // the member to start at, make it its offset. Otherwise,
  //         // insert padding to the first valid offset.
  //         if (Offset % AlignInBits != 0)
  //           Offset = findFirstLarger(Offset, AlignInBits);

  //         DIDerivedType *NewMemberType = DB.createMemberType(
  //             Member->getScope(), Member->getName(),
  //             Member->getFile(), Member->getLine(),
  //             Member->getSizeInBits(), Member->getAlignInBits(),
  //             Offset, Member->getFlags(), Member->getBaseType(),
  //             Member->getAnnotations());
  //         // NewMemberType->dump();

  //         Offset += Member->getSizeInBits();
  //         errs() << Offset << '\n';

  //         NewElements.push_back(NewMemberType);
  //       }
  //     }
  //     Cnt++;
  //   }

  //   // for (auto &elem : NewElements)
  //   //   elem->dump();

  //   uint64_t NewSizeInBits = Offset;
  //   if (MaxAlignInBits > 0 && (NewSizeInBits % MaxAlignInBits !=
  //   0))
  //     NewSizeInBits = findFirstLarger(NewSizeInBits,
  //     MaxAlignInBits);

  //   DICompositeType *NewStructType = DB.createStructType(
  //       CT->getScope(), CT->getName(), CT->getFile(),
  //       CT->getLine(), NewSizeInBits, CT->getAlignInBits(),
  //       CT->getFlags(), CT->getBaseType(),
  //       DB.getOrCreateArray(NewElements), CT->getRuntimeLang(),
  //       CT->getVTableHolder());

  //   if (CT->isDistinct())
  //     NewStructType->setDistinct();

  //   // Set scope of DW_TAG_member fields of struct to point to the
  //   // newly formed DICompositeType.
  //   for (auto *Field : NewStructType->getElements())
  //     Field->replaceOperandWith(1, NewStructType);

  //   // V->dump();
  //   // errs() << '\n';
  //   // V->getOperand(0)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(1)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(2)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(3)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(4)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(5)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(6)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(7)->dump();
  //   // errs() << '\n';
  //   // V->getOperand(8)->dump();
  //   // NewStructType->dump();
  //   if (State == 1)
  //     V->getType()->replaceOperandWith(3, NewStructType);
  //   else if (State == 2)
  //     V->replaceOperandWith(3, NewStructType);
  DINodeArray Elements = DINodeArray();
  std::string StructName = "";
  DICompositeType *CT = nullptr;
  unsigned State = 0;
  if (isa<DIDerivedType>(V->getType()) &&
      V->getType()->getTag() == dwarf::DW_TAG_typedef) {
    if ((CT = dyn_cast<DICompositeType>(
             (dyn_cast<DIDerivedType>(V->getType()))->getBaseType()))) {
      StructName = CT->getName().str();

      // If the composite type does not have a name, get the name of
      // the struct from GlobalVariable instead of DIGlobalVariable.
      // If there is a typedef attached to this composite type, its
      // name will be the name of the struct, otherwise anon is used.
      if (StructName.length() == 0) {
        if (VarScope)
          StructName = getGVTypeName(M, V->getName());
        // NOTE: If V is a DILocalVariable*, the corresponding local variable is
        // used to create a global variable with prefix __const.function_name.
        // so that local and global variables can be used uniformly.
        else
          StructName = getGVTypeName(M, "__const." + FuncName + "." +
                                            V->getName().str());
      }

      Elements = CT->getElements();
      State = 1;
    }
  } else if ((CT = dyn_cast<DICompositeType>(V->getType()))) {
    StructName = CT->getName().str();

    // If the composite type does not have a name, get the name of the
    // struct from GlobalVariable instead of DIGlobalVariable. If
    // there is a typedef attached to this composite type, its name
    // will be the name of the struct, otherwise anon is used.
    if (StructName.length() == 0) {
      if (VarScope)
        StructName = getGVTypeName(M, V->getName());
      // NOTE: If V is a DILocalVariable *, the corresponding local variable is
      // used to create a global variable with prefix __const.function_name.
      // so that local and global variables can be used uniformly.
      else
        StructName =
            getGVTypeName(M, "__const." + FuncName + "." + V->getName().str());
    }

    Elements = CT->getElements();
    State = 2;
  }

  // If V is DILocalVariable *, all global variables should have been already
  // updated with new struct type, but with struct name thas has a sufix _1, as
  // the old type is not yet replaced in full by a new type. This is known
  // because this function is, in this case, called when finding alloca
  // instructions in run method.
  // If StructName has length 0, there is no corresponding global variable to
  // this local variable which means that it is initialized from a copy of an
  // existing local or global variable. Therefore, the struct type of that
  // variable will be processed when going through the mentioned existing
  // variable.
  // V->dump();
  // errs() << StructName.length() << '\n' << Elements.size() << '\n';
  if (!VarScope && StructName.length() > 0 &&
      StructName.substr(StructName.length() - 2, 2).compare("_1") == 0)
    StructName = StructName.substr(0, StructName.length() - 2);

  // errs() << '\n';

  // Create new DICompositeType node and its members with correct
  // sizes and offsets.
  // V->dump();
  // CT->dump();
  if (Elements.size() > 0 && StructName.length() > 0) {
    errs() << StructName << '\n';
    auto E = FieldUsage.find("struct." + StructName);
    if (E == FieldUsage.end())
      return false;
    // continue;

    std::vector<unsigned> NewVect = FieldUsage["struct." + StructName];
    // std::vector<Metadata *> NewElements;

    bool IsAnyFieldOptimizedOut = false;
    unsigned Cnt = 0;
    uint64_t Offset = 0;
    // Alignment of the new structure.
    uint64_t MaxAlignInBits = 0;

    for (auto *Field : Elements) {
      if (auto *Member = dyn_cast<DIDerivedType>(Field)) {
        if (NewVect[Cnt] > 0) {
          // errs() <<
          // (dyn_cast<DIDerivedType>(Field))->getSizeInBits()
          // << '\n'; errs() <<
          // (dyn_cast<DIDerivedType>(Field))->getOffsetInBits() <<
          // '\n';
          uint64_t AlignInBits = Member->getBaseType()->getSizeInBits();
          errs() << AlignInBits << "\n\n";
          if (AlignInBits > MaxAlignInBits)
            MaxAlignInBits = AlignInBits;

          // If the current offset in the new structure is valid for
          // the member to start at, make it its offset. Otherwise,
          // insert padding to the first valid offset.
          if (Offset % AlignInBits != 0)
            Offset = findFirstLarger(Offset, AlignInBits);

          // DIDerivedType *NewMemberType = DB.createMemberType(
          //     Member->getScope(), Member->getName(),
          //     Member->getFile(), Member->getLine(),
          //     Member->getSizeInBits(), Member->getAlignInBits(),
          //     Member->getOffsetInBits(), Member->getFlags(),
          //     Member->getBaseType(), Member->getAnnotations());
          // NewMemberType->dump();

          Member->setNewOffsetInBits(Offset);
          Member->setOptimizedOut(false);

          Offset += Member->getSizeInBits();
          errs() << Offset << '\n';

          // NewElements.push_back(NewMemberType);
        } else {
          Member->setOptimizedOut(true);
          if (!IsAnyFieldOptimizedOut)
            IsAnyFieldOptimizedOut = true;
        }
      }
      Cnt++;
    }

    // for (auto &elem : NewElements)
    //   elem->dump();

    // If no fields were optimized out, do not create a new
    // DICompositeType with FlagFieldsOptimizedOut and newSize set.
    if (!IsAnyFieldOptimizedOut)
      return false;
    // continue;

    uint64_t NewSizeInBits = Offset;
    if (MaxAlignInBits > 0 && (NewSizeInBits % MaxAlignInBits != 0))
      NewSizeInBits = findFirstLarger(NewSizeInBits, MaxAlignInBits);

    DICompositeType *NewStructType = DB.createStructType(
        CT->getScope(), CT->getName(), CT->getFile(), CT->getLine(),
        CT->getSizeInBits(), CT->getAlignInBits(),
        CT->getFlags() | DINode::DIFlags::FlagFieldsOptimizedOut,
        CT->getBaseType(), Elements, CT->getRuntimeLang(),
        CT->getVTableHolder());

    NewStructType->setNewSizeInBits(NewSizeInBits);

    if (CT->isDistinct())
      NewStructType->setDistinct();

    // Set scope of DW_TAG_member fields of struct to point to the
    // newly formed DICompositeType.
    for (auto *Field : NewStructType->getElements())
      Field->replaceOperandWith(1, NewStructType);

    // V->dump();
    // errs() << '\n';
    // V->getOperand(0)->dump();
    // errs() << '\n';
    // V->getOperand(1)->dump();
    // errs() << '\n';
    // V->getOperand(2)->dump();
    // errs() << '\n';
    // V->getOperand(3)->dump();
    // errs() << '\n';
    // V->getOperand(4)->dump();
    // errs() << '\n';
    // V->getOperand(5)->dump();
    // errs() << '\n';
    // V->getOperand(6)->dump();
    // errs() << '\n';
    // V->getOperand(7)->dump();
    // errs() << '\n';
    // V->getOperand(8)->dump();
    // NewStructType->dump();
    if (State == 1)
      V->getType()->replaceOperandWith(3, NewStructType);
    else if (State == 2)
      V->replaceOperandWith(3, NewStructType);

    return true;
  } else return false;
}

// Alter debug info nodes to reflect the lack of usage of certain fields in
// structs.
void changeDebugInfoForNewStructs(
    Module &M,
    std::unordered_map<std::string, std::vector<unsigned>> &FieldUsage,
    std::unordered_set<DIType *> &AlreadyUpdated) {
  // Process global struct variables.
  for (auto &MD : M.named_metadata())
    for (auto *N : MD.operands())
      if (auto *CU = dyn_cast<DICompileUnit>(N)) {
        DIBuilder DB(M, false, CU);
        DataLayout DL(&M);
        for (auto *GVE : CU->getGlobalVariables()) {
          auto *V = GVE->getVariable();
          // errs() << "Here\n";
          // V->dump();
          // errs() << "Here\n";
          if (V && AlreadyUpdated.count(V->getType()) == 0) {
            bool Updated = createNewCompositeType(DB, M, V, "", FieldUsage, true);
            if (Updated)
              AlreadyUpdated.insert(V->getType());
          }
        }
      }
}

// Creates new getelementptr instruction with new indices, based on a new
// struct type.
GetElementPtrInst *createNewGEPInst(GetElementPtrInst *OldGEPInst,
                                    std::vector<unsigned> Indices,
                                    StructType *NewStruct) {
  std::vector<Value *> IdxList(OldGEPInst->getNumOperands() - 1);
  Use *OperandList = OldGEPInst->getOperandList();
  for (unsigned i = 0, e = OldGEPInst->getNumOperands() - 1; i != e; ++i) {
    assert(isa<Constant>(OperandList[i + 1].get()) &&
           "Struct indices must be i32 constants!");
    IdxList[i] = cast<Constant>(OperandList[i + 1].get());
    if (i == 1)
      if (auto *NewIndex = dyn_cast<ConstantInt>(IdxList[i])) {
        unsigned NumOfInvalidIndices = 0;
        for (unsigned j = 0; j != NewIndex->getZExtValue(); ++j)
          if (Indices[j] == 0)
            NumOfInvalidIndices++;

        // New index is the result of a subtraction between the old
        // index and NumOfInvalidIndices. That is the number of the
        // unused fields with indexes between 0 and old index in the old
        // struct.
        APInt AP(NewIndex->getBitWidth(),
                 NewIndex->getZExtValue() - NumOfInvalidIndices);
        IdxList[i] = Constant::getIntegerValue(NewIndex->getType(), AP);
      }
  }

  // OldGEPInst->dump();
  // OldGEPInst->getPointerOperand()->getType()->getScalarType()->dump();
  // if (auto* Val = dyn_cast<LoadInst>(OldGEPInst->getPointerOperand()))
  //   Val->getOperand(0)->dump();
  // NewStruct->dump();

  errs() << '\n';

  if (!OldGEPInst->isInBounds())
    return GetElementPtrInst::Create(NewStruct, OldGEPInst->getPointerOperand(),
                                     IdxList, OldGEPInst->getName(),
                                     OldGEPInst);
  else
    return GetElementPtrInst::CreateInBounds(
        NewStruct, OldGEPInst->getPointerOperand(), IdxList,
        OldGEPInst->getName(), OldGEPInst);
}

// Filters structs which have at least one pointer pointing to them from
// FieldUsage.
void filterStructsWithPointerVars(
    Module &M,
    std::unordered_map<std::string, std::vector<unsigned>> &FieldUsage) {
  for (Function &F : M)
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *GEPInst = dyn_cast<GetElementPtrInst>(&I))
          if (isa<LoadInst>(GEPInst->getOperand(0)))
            // The identified usage of a struct field is being performed by
            // a pointer. Therefore, this struct should not be optimized.
            if (auto *GEPType =
                    dyn_cast<StructType>(GEPInst->getSourceElementType()))
              if (FieldUsage.count(GEPType->getName().str()) == 1)
                FieldUsage.erase(GEPType->getName().str());
}

// Filters structs which are being passed as parameters of functions which
// only have declarations in this module from FieldUsage. NOTE: This code does
// not handle the C case where the function is defined in one compile unit and
// called in the other but without matching the prototype of that function.
void filterStructsAsFunctionParameters(
    Module &M,
    std::unordered_map<std::string, std::vector<unsigned>> &FieldUsage) {
  // for (Function &F : M)
  //   for (BasicBlock &BB : F)
  //     for (Instruction &I : BB)
  //       if (auto *CInst = dyn_cast<CallInst>(&I))
  //         if (!isa<IntrinsicInst>(CInst)) {
  //           // CInst->getFunctionType()->dump();
  //           CInst->dump();
  //           if (isa<Function>(CInst->getOperand(1)))
  //             CInst->getOperand(1)->dump();
  //           errs() << '\n';
  //         }
  // errs() << "Functions:\n";
  for (Function &F : M)
    if (F.isDeclaration()) {
      // errs() << F.getName() << '\n';
      for (auto &A : F.args())
        if (auto *T = dyn_cast<StructType>(A.getType()))
          if (FieldUsage.count(T->getName().str()) == 1)
            FieldUsage.erase(T->getName().str());

      // for (auto &U : F.uses()) {
      // errs() << "Here in\n";
      // U.getUser()->dump();
      // errs() << "Here in again\n";
      // U.get()->dump();
      // if (isa<Function>(U.get())) errs() << "Yes\n";
      // }
      // errs() << '\n';
    }
  // errs() << "Functions end\n";
}

// void cloneFuncBody(Function *OldFunc, Function *NewFunc) {
//   for (BasicBlock &BB : make_early_inc_range(*OldFunc)) {
//     Twine BBName = BB.getName();
//     BB.setName(BBName + Twine("_1"));
//     BasicBlock *NewBB = BasicBlock::Create(BB.getContext(), BBName,
//     NewFunc);

//     for (Instruction &I : make_early_inc_range(BB)) {
//       Instruction *NewI = I.clone();
//       Twine IName = I.getName();
//       I.setName(IName + Twine("_1"));

//       // errs() << NewI->getParent() << '\n';
//       NewI->setName(IName);
//       // NewI->dump();
//       NewBB->getInstList().push_back(NewI);
//       // NewI->setParentAfterClone(NewBB);
//       // errs() << NewI->getParent() << '\n';

//       I.replaceAllUsesWith(NewI, false);
//       // I.dump();
//       I.eraseFromParent();
//       // I.dump();
//     }

//     BB.replaceAllUsesWith(NewBB, false);
//     BB.eraseFromParent();
//   }
// }

// Function *cloneFunctionWithNewType(Function &Func, std::string StructName,
//                                    StructType *NewStruct, bool &IsNewRetTy)
//                                    {
//   auto *OldFuncType = Func.getFunctionType();

//   // If Func returns the value of type StructName, new return type
//   // is NewStruct.
//   Type *NewFuncReturnType = OldFuncType->getReturnType();
//   if (auto *OldST = dyn_cast<StructType>(OldFuncType->getReturnType()))
//     if (OldST->getName().compare(StructName) == 0) {
//       IsNewRetTy = true;
//       NewFuncReturnType = NewStruct;
//     }

//   // If Func has any parameters of type StructName, they will be
//   // replaced with NewStruct parameters.
//   // unsigned Cnt = 0;
//   // std::vector<Type *> NewFuncParams = OldFuncType->params();
//   // for (auto *ParamTy : make_early_inc_range(NewFuncParams)) {
//   //   if (auto *OldST = dyn_cast<StructType>(ParamTy))
//   //     if (OldST->getName().compare(StructName) == 0)
//   //       NewFuncParams[Cnt] = NewStruct;
//   //   Cnt++;
//   // }

//   std::vector<Type *> ArgTypes;

//   // for (const Argument &A : Func.args())
//   //   if (auto *OldST = dyn_cast<StructType>(A.getType())) {
//   //     if (OldST->getName().compare(StructName) == 0)
//   //       ArgTypes.push_back(NewStruct);
//   //     else
//   //       ArgTypes.push_back(A.getType());
//   //   }
//   for (const Argument &A : Func.args())
//     ArgTypes.push_back(A.getType());

//   Twine FuncName = Func.getName();
//   Func.setName(FuncName + Twine("_1"));

//   // Create a new function type.
//   FunctionType *NewFuncType =
//       FunctionType::get(NewFuncReturnType, ArgTypes,
//       OldFuncType->isVarArg());

//   // Create the new function.
//   Function *NewFunc =
//       Function::Create(NewFuncType, Func.getLinkage(),
//       Func.getAddressSpace(),
//                        FuncName, Func.getParent());

//   ValueToValueMapTy VMap;
//   errs() << Func.getName() << '\n';
//   // Loop over the arguments, copying the names of the mapped arguments
//   over. Function::arg_iterator DestA = NewFunc->arg_begin(); for (const
//   Argument &A : Func.args()) {
//     A.dump();
//     if (VMap.count(&A) == 0) { // Is this argument preserved?
//       errs() << A.getName().str() << '\n';
//       DestA->setName(A.getName()); // Copy the name over.
//       VMap[&A] = &*DestA++;        // Add mapping to VMap.
//     }
//   }

//   SmallVector<ReturnInst *, 8> Returns; // Ignore returns cloned.
//   CloneFunctionInto(NewFunc, &Func, VMap,
//                     CloneFunctionChangeType::LocalChangesOnly, Returns, "",
//                     nullptr);

//   return NewFunc;
// }

void fixArgumentAttributes(Function *OldFunc, Function *NewFunc,
                           StructType *NewStruct, std::string StructName) {
  // NewArgAttrs is a vector of attribute sets for arguments of NewFunc
  // function.
  std::vector<AttributeSet> NewArgAttrs;
  bool AttrChange = false;

  for (const Argument &A : OldFunc->args()) {
    unsigned ArgNo = A.getArgNo();
    // If an argument represents a structure being passed by value, it
    // is of pointer type (an opaque pointer) with the appropriate
    // attribute (byval, preallocated, inalloca, byref, sret) which
    // has a type argument, which is that structure type.
    if (A.hasPointeeInMemoryValueAttr()) {
      AttributeSet ParamAttrs =
          OldFunc->getAttributes().getParamAttrs(ArgNo);
      AttrBuilder B(OldFunc->getContext(), ParamAttrs);
      // if (ParamAttrs.hasAttribute(Attribute::ByVal)) {
      //   ParamAttrs = ParamAttrs.removeAttribute(A.getContext(),
      //                                           Attribute::ByVal);
      //   B.addByValAttr(NewStruct);
      //   // ParamAttrs = ParamAttrs.addAttributes(
      //   //     A.getContext(), AttributeSet::get(A.getContext(), B));
      // }
      // if (ParamAttrs.hasAttribute(Attribute::Preallocated)) {
      //   ParamAttrs = ParamAttrs.removeAttribute(
      //       A.getContext(), Attribute::Preallocated);
      //   B.addPreallocatedAttr(NewStruct);
      //   // ParamAttrs = ParamAttrs.addAttributes(
      //   //     A.getContext(), AttributeSet::get(A.getContext(), B));
      // }
      // if (ParamAttrs.hasAttribute(Attribute::InAlloca)) {
      //   ParamAttrs = ParamAttrs.removeAttribute(
      //       A.getContext(), Attribute::InAlloca);
      //   B.addInAllocaAttr(NewStruct);
      //   // ParamAttrs = ParamAttrs.addAttributes(
      //   //     A.getContext(), AttributeSet::get(A.getContext(), B));
      // }
      // if (ParamAttrs.hasAttribute(Attribute::ByRef)) {
      //   ParamAttrs = ParamAttrs.removeAttribute(A.getContext(),
      //                                           Attribute::ByRef);
      //   B.addByRefAttr(NewStruct);
      //   // ParamAttrs = ParamAttrs.addAttributes(
      //   //     A.getContext(), AttributeSet::get(A.getContext(), B));
      // }
      // if (ParamAttrs.hasAttribute(Attribute::StructRet)) {
      //   ParamAttrs = ParamAttrs.removeAttribute(
      //       A.getContext(), Attribute::StructRet);
      //   B.addStructRetAttr(NewStruct);
      //   // ParamAttrs = ParamAttrs.addAttributes(
      //   //     A.getContext(), AttributeSet::get(A.getContext(), B));
      // }
      if (auto *OldST = dyn_cast_or_null<StructType>(B.getByValType()))
        if (OldST->getName().compare(StructName) == 0) {
          B.addByValAttr(NewStruct);
          AttrChange = true;
        }
      if (auto *OldST = dyn_cast_or_null<StructType>(B.getPreallocatedType()))
        if (OldST->getName().compare(StructName) == 0) {
          B.addPreallocatedAttr(NewStruct);
          AttrChange = true;
        }
      if (auto *OldST = dyn_cast_or_null<StructType>(B.getInAllocaType()))
        if (OldST->getName().compare(StructName) == 0) {
          B.addInAllocaAttr(NewStruct);
          AttrChange = true;
        }
      if (auto *OldST = dyn_cast_or_null<StructType>(B.getByRefType()))
        if (OldST->getName().compare(StructName) == 0) {
          B.addByRefAttr(NewStruct);
          AttrChange = true;
        }
      if (auto *OldST = dyn_cast_or_null<StructType>(B.getStructRetType()))
        if (OldST->getName().compare(StructName) == 0) {
          B.addStructRetAttr(NewStruct);
          AttrChange = true;
        }

      NewArgAttrs.push_back(AttributeSet::get(OldFunc->getContext(), B));
      continue;
    }

    NewArgAttrs.push_back(OldFunc->getAttributes().getParamAttrs(ArgNo));
  }

  if (AttrChange)
    NewFunc->setAttributes(AttributeList::get(
        OldFunc->getContext(), OldFunc->getAttributes().getFnAttrs(),
        OldFunc->getAttributes().getRetAttrs(), NewArgAttrs));
}

// TODO: Implement support for pointers on structs as parameters and return
// types in functions, on any level of indirection.
void fixCallBasesIfNecessary(Function *F, std::string StructName) {
  // NewArgAttrs is a vector of attribute sets for arguments of NewFunc
  // function.
  std::vector<AttributeSet> NewArgAttrs;
  bool AttrChange = false;

  // F->dump();

  for (auto &U : F->uses())
    if (CallBase *CB = dyn_cast<CallBase>(U.getUser())) {
      for (unsigned ArgNo = 0; ArgNo < F->getFunctionType()->getNumParams();
           ++ArgNo) {
        if (F->getArg(ArgNo)->hasPointeeInMemoryValueAttr()) {
          AttributeSet ParamAttrs =
              CB->getAttributes().getParamAttrs(ArgNo);
          AttrBuilder B(F->getContext(), ParamAttrs);

          if (auto *OldST = dyn_cast_or_null<StructType>(B.getByValType()))
            if (OldST->getName().compare(StructName) == 0) {
              B.addByValAttr(F->getParamByValType(ArgNo));
              AttrChange = true;
            }
          if (auto *OldST = dyn_cast_or_null<StructType>(B.getPreallocatedType()))
            if (OldST->getName().compare(StructName) == 0) {
              B.addPreallocatedAttr(F->getParamPreallocatedType(ArgNo));
              AttrChange = true;
            }
          if (auto *OldST = dyn_cast_or_null<StructType>(B.getInAllocaType()))
            if (OldST->getName().compare(StructName) == 0) {
              B.addInAllocaAttr(F->getParamInAllocaType(ArgNo));
              AttrChange = true;
            }
          if (auto *OldST = dyn_cast_or_null<StructType>(B.getByRefType()))
            if (OldST->getName().compare(StructName) == 0) {
              B.addByRefAttr(F->getParamByRefType(ArgNo));
              AttrChange = true;
            }
          if (auto *OldST = dyn_cast_or_null<StructType>(B.getStructRetType()))
            if (OldST->getName().compare(StructName) == 0) {
              B.addStructRetAttr(F->getParamStructRetType(ArgNo));
              AttrChange = true;
            }

          NewArgAttrs.push_back(AttributeSet::get(F->getContext(), B));
          continue;
        }

        NewArgAttrs.push_back(CB->getAttributes().getParamAttrs(ArgNo));
      }

      if (AttrChange)
        CB->setAttributes(AttributeList::get(
            F->getContext(), CB->getAttributes().getFnAttrs(),
            CB->getAttributes().getRetAttrs(), NewArgAttrs));

      CB->mutateFunctionType(F->getFunctionType());
    }
}

// Returns true if V is either a GlobalVariable or an AllocaInst and
// its pointee type has the name StructName + "_1", otherwise false.
bool isGlobVarOrAllocaInstWithSameType(Value *V, std::string StructName) {
  if (auto *GV = dyn_cast<GlobalVariable>(V)) {
    if (auto *ST = dyn_cast<StructType>(GV->getValueType()))
      if (ST->getName().str().compare(StructName + "_1") == 0)
        return true;
  } else if (auto *AI = dyn_cast<AllocaInst>(V)) {
    if (auto *ST = dyn_cast<StructType>(AI->getAllocatedType()))
      if (ST->getName().str().compare(StructName + "_1") == 0)
        return true;
  }

  return false;
}

PreservedAnalyses
UnusedStructureFieldsEliminationPass::run(Module &M, ModuleAnalysisManager &) {
  std::vector<StructType *> AllStructs = M.getIdentifiedStructTypes();
  // Every element of this map corresponds to one identified struct type,
  // where the key is the name of the structure and the value is a vector
  // containing numbers of usages of its fields.
  std::unordered_map<std::string, std::vector<unsigned>> FieldUsage;

  for (StructType *ST : AllStructs) {
    // ST->dump();
    std::string StructName = ST->getName().str();
    std::vector<unsigned> Vect(ST->elements().size(), 0);
    FieldUsage[StructName] = Vect;
  }

  errs() << "Module start\n";

  M.dump();

  errs() << "Module end\n";

  // Finds all occurences of struct fields usage and records them in the
  // FieldUsage map.
  for (Function &F : M)
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        // Go through the usage of struct fields in global struct variables.
        if (isa<LoadInst>(I) || isa<StoreInst>(I)) {
          Value *PointerOp = getLoadStorePointerOperand(&I);
          if (auto *GEPCE = dyn_cast<GetElementPtrConstantExpr>(PointerOp)) {
            // GEPCE->dump();

            // Use *Indexes = GEPCE->getOperandList();
            // GEPCE->getOperand(0)->dump();
            // GEPCE->getOperand(1)->dump();
            // GEPCE->getOperand(2)->dump();

            if (auto *GEPType = dyn_cast<StructType>(
                    GEPCE->getOperand(0)
                        ->getType()
                        ->getNonOpaquePointerElementType()))
              if (FieldUsage.count(GEPType->getName().str()) == 1) {
                auto *ConstValue = dyn_cast<ConstantInt>(GEPCE->getOperand(2));
                FieldUsage[GEPType->getName().str()]
                          [ConstValue->getZExtValue()]++;
              }

            // errs() << '\n';
          }
        }
        // Go through the usage of struct fields in local struct variables.
        else if (auto *GEP = dyn_cast<GetElementPtrInst>(&I))
          // GEP->dump();
          if (auto *GEPType =
                  dyn_cast<StructType>(GEP->getOperand(0)
                                           ->getType()
                                           ->getNonOpaquePointerElementType()))
            if (FieldUsage.count(GEPType->getName().str()) == 1) {
              // errs() << "In here\n";
              auto *ConstValue = dyn_cast<ConstantInt>(GEP->getOperand(2));
              FieldUsage[GEPType->getName().str()]
                        [ConstValue->getZExtValue()]++;
            }
  // Go through the usage of struct fields in local struct variables.
  // else if (auto *DV = dyn_cast<DbgValueInst>(&I)) {
  //   // Process DW_OP_LLVM_fragment occurrences...
  // }

  errs() << '\n';

  for (const auto &mapEntry : FieldUsage) {
    errs() << mapEntry.first << '\n';
    for (unsigned i = 0; i < mapEntry.second.size(); i++)
      errs() << i << " : " << mapEntry.second[i] << '\n';
    errs() << '\n';
  }

  // errs() << "Start:\n";
  // for (Function &F : M)
  //   for (BasicBlock &BB : F)
  //     for (Instruction &I : BB)
  //         if (isa<LoadInst>(I) || isa<StoreInst>(I))
  //           I.dump();

  filterStructsWithPointerVars(M, FieldUsage);
  filterStructsAsFunctionParameters(M, FieldUsage);

  // Used in order not to update the debug info for a struct multiple times
  // (due to multiple variables of that struct type for example).
  std::unordered_set<DIType *> IsDbgInfoAlreadyUpdated;

  std::unordered_set<unsigned> InvalidIndexes;
  for (const auto &mapEntry : FieldUsage) {
    // If there is an unused field of the given structure, eliminate it from
    // the structure by not including it in the new structure body.
    std::string StructName = mapEntry.first;
    ArrayRef<Type *> StructFields =
        StructType::getTypeByName(M.getContext(), StructName)->elements();
    std::vector<Type *> NewBody;
    for (unsigned i = 0; i < mapEntry.second.size(); i++)
      if (mapEntry.second[i] != 0)
        NewBody.push_back(StructFields[i]);
      else
        InvalidIndexes.insert(i);

    StructType *NewStruct = StructType::create(M.getContext(), NewBody);
    NewStruct->setName(StructName + "_1");

    // errs() << "Subtypes:\n";
    // for (auto Tempor :
    //      StructType::getTypeByName(M.getContext(), StructName)->subtypes())
    //   Tempor->dump();
    // errs() << "Subtypes end\n";

    // errs() << "Subtypes:\n";
    // for (auto Tempor : NewStruct->subtypes())
    //   Tempor->dump();
    // errs() << "Subtypes end\n";

    // errs() << "Functions:\n";
    // for (Function &F : M)
    //   F.dump();
    // errs() << "Functions end\n";

    // Go through all functions and update their struct parameters which are
    // of StructName type to be of NewStruct type.
    Module::iterator J = M.begin();
    for (unsigned Cnt = 0; Cnt < M.getFunctionList().size(); Cnt++) {
      // errs() << M.getFunctionList().size() << '\n';
      // errs() << *(++M.begin()) << '\n';
      // errs() << (*J).getName() << '\n';
      // errs() << *J << '\n';
      Function &Func = *J;
      ++J;
      // errs() << Func.getName() << '\n';
      if (!Func.isDeclaration()) {
        // if (Cnt == 5) break;
        // auto *OldFuncType = Func.getFunctionType();
        // // If Func returns the value of type StructName, new return type
        // // is NewStruct.
        // Type *NewFuncReturnType = OldFuncType->getReturnType();
        // if (auto *OldST =
        // dyn_cast<StructType>(OldFuncType->getReturnType()))
        //   if (OldST->getName().compare(StructName) == 0) {
        //     IsNewRetTy = true;
        //     NewFuncReturnType = NewStruct;
        //   }

        // // If Func has any parameters of type StructName, they will be
        // // replaced with NewStruct parameters.
        // unsigned Cnt = 0;
        // std::vector<Type *> NewFuncParams = OldFuncType->params();
        // for (auto *ParamTy : make_early_inc_range(NewFuncParams)) {
        //   if (auto *OldST = dyn_cast<StructType>(ParamTy))
        //     if (OldST->getName().compare(StructName) == 0)
        //       NewFuncParams[Cnt] = NewStruct;
        //   Cnt++;
        // }

        // Twine FuncName = Func.getName();
        // Func.setName(FuncName + Twine("_1"));
        // auto *NewFuncType =
        //     FunctionType::get(NewFuncReturnType, NewFuncParams, false);

        // Function *NewFunc =
        //     Function::Create(NewFuncType, Func.getLinkage(),
        //                      Func.getAddressSpace(), FuncName, &M);

        // cloneFuncBody(&Func, NewFunc);

        // // ValueToValueMapTy VMap;
        // // Function *NewFunc = CloneFunction(&Func, VMap);

        // NewFunc->copyAttributesFrom(&Func);

        // Func.replaceAllUsesWith(NewFunc, IsNewRetTy);
        // // errs() << "Here\n";
        // Func.eraseFromParent();
        // // errs() << "Here\n";

        auto *OldFuncType = Func.getFunctionType();
        bool IsNewTy = false;

        // If Func returns the value of type StructName, new return type
        // is NewStruct.
        Type *NewFuncReturnType = OldFuncType->getReturnType();
        if (auto *OldST = dyn_cast<StructType>(OldFuncType->getReturnType()))
          if (OldST->getName().compare(StructName) == 0) {
            IsNewTy = true;
            NewFuncReturnType = NewStruct;
          }

        // If Func has any parameters of type StructName, they will be
        // replaced with NewStruct parameters.
        // unsigned Cnt = 0;
        // std::vector<Type *> NewFuncParams = OldFuncType->params();
        // for (auto *ParamTy : make_early_inc_range(NewFuncParams)) {
        //   if (auto *OldST = dyn_cast<StructType>(ParamTy))
        //     if (OldST->getName().compare(StructName) == 0)
        //       NewFuncParams[Cnt] = NewStruct;
        //   Cnt++;
        // }

        // for (const Argument &A : Func.args())
        //   ArgTypes.push_back(A.getType());

        Twine FuncName = Func.getName();
        Func.setName(FuncName + Twine("_1"));

        std::vector<Type *> ArgTypes;

        // Prior to opaque pointers transition, the types of function
        // parameters representing struct variables were pointers to
        // those structs. Now, since opaque pointers do not have pointee
        // types, updating those types is not being performed, because
        // they do not exist anymore. Checking whether an argument type
        // has the struct type (not pointer to struct type) may not be
        // necessary (because it may never be the case), but it is kept
        // for now, until it is figured out if that case is even possible
        // to occur in the code.
        for (const Argument &A : Func.args())
          if (auto *OldST = dyn_cast<StructType>(A.getType())) {
            if (OldST->getName().compare(StructName) == 0) {
              ArgTypes.push_back(NewStruct);
              IsNewTy = true;
              continue;
            }
          } else
            ArgTypes.push_back(A.getType());

        // Create a new function type.
        FunctionType *NewFuncType = FunctionType::get(
            NewFuncReturnType, ArgTypes, OldFuncType->isVarArg());

        // Create the new function.
        Function *NewFunc =
            Function::Create(NewFuncType, Func.getLinkage(),
                             Func.getAddressSpace(), FuncName, &M);

        errs() << "Name: " << FuncName << ' ' << '\n';
        ValueToValueMapTy VMap;
        // Loop over the arguments, copying the names of the mapped arguments
        // over.
        Function::arg_iterator DestA = NewFunc->arg_begin();
        // errs() << Func.arg_end() - Func.arg_begin() + 1 << '\n';
        for (const Argument &A : Func.args()) {
          A.dump();
          if (VMap.count(&A) == 0) { // Is this argument preserved?
            // errs() << A.getName().str() << '\n';
            DestA->setName(A.getName()); // Copy the name over.
            VMap[&A] = &*DestA++;        // Add mapping to VMap.
          }
        }

        SmallVector<ReturnInst *, 8> Returns; // Ignore returns cloned.
        CloneFunctionInto(NewFunc, &Func, VMap,
                          CloneFunctionChangeType::LocalChangesOnly, Returns,
                          "", nullptr);
        // errs() << Cnt++ << '\n';
        // for (Function& FF : M)
        //   errs() << FF.getName() << '\n';
        // errs() << "Size: " << M.size() << '\n';

        fixArgumentAttributes(&Func, NewFunc, NewStruct, StructName);

        // NewFunc->dump();

        Func.replaceAllUsesWith(NewFunc, IsNewTy);
        Func.eraseFromParent();

        fixCallBasesIfNecessary(NewFunc, StructName);
      }
    }
    errs() << "Out\n";

    // M.dump();

    // ValueType of a GlobalVariable should be set here to NewStruct
    // because it is being looked up in run of TypeFinder. Here,
    // another GlobalVariable is created with modified type and the
    // current one is eliminated. Note that all global variables are
    // of pointer type.
    for (auto &Var : make_early_inc_range(M.globals()))
      // TODO: Implement support for variables of pointer type which
      // refer structs, on any level of indirection.
      if (isa<GlobalVariable>(Var))
        if (auto *TempType = dyn_cast<StructType>(
                Var.getValueType()))
          // Var.dump();
          if (TempType->getName().compare(StructName) == 0) {
            Constant *Initializer;
            if (!Var.hasInitializer())
              Initializer = nullptr;
            else if (isa<ConstantAggregateZero>(Var.getInitializer()))
              Initializer = ConstantAggregateZero::get(NewStruct);
            else if (isa<ConstantStruct>(Var.getInitializer())) {
              std::vector<Constant *> FieldInits(NewStruct->getNumElements(),
                                                 nullptr);
              unsigned Cnt = 0;
              for (unsigned j = 0; j < Var.getInitializer()->getNumOperands();
                   j++)
                if (InvalidIndexes.count(j) == 0)
                  FieldInits[Cnt++] =
                      dyn_cast<Constant>(Var.getInitializer()->getOperand(j));

              Initializer = ConstantStruct::get(NewStruct, FieldInits);
              // errs() << "Num: " <<
              // (cast<ConstantStruct>(Initializer))->getType()->getNumElements()
              // << '\n';
              // errs() << "Subtypes:\n";
              // for (auto Tempor :
              //      (cast<ConstantStruct>(Initializer))->getType()->subtypes())
              //   Tempor->dump();
              // errs() << "Subtypes end\n";
            }

            // Initializer->dump();
            // Var.getInitializer()->dump();

            // GlobalVariable NewGlobalVar(
            //     M, NewStruct, Var.isConstant(), Var.getLinkage(),
            //     nullptr, Var.getName() + "_new", nullptr,
            //     Var.getThreadLocalMode(), Var.getAddressSpace(),
            //     Var.isExternallyInitialized());

            GlobalVariable *NewGlobalVar =
                createNewGlobalVar(M, Var, NewStruct, Initializer);

            std::string NewGlobalVarName = Var.getName().str();

            // Type of the new global variable is the same as the old one,
            // because it is an opaque pointer (ptr). The ValueType of that
            // global variable, however, should be the NewStruct type.
            Var.replaceAllUsesWith(NewGlobalVar, false);
            Var.eraseFromParent();

            NewGlobalVar->setName(NewGlobalVarName);
          }

    DataLayout DL(&M);

    // // This set is used so as to prevent multiple updates of the same
    // // structure.
    // std::unordered_set<Type *> ProcessedStructTypesWithOnlyLocalVariables;

    // Update the old struct type with the new struct type regarding the usage
    // of the local struct variables. Also, update the old struct type with
    // the new struct type in call instructions, where necessary.
    for (Function &F : M)
      for (BasicBlock &BB : F)
        for (Instruction &I : make_early_inc_range(BB))
          if (auto *AllocationInst = dyn_cast<AllocaInst>(&I)) {
            // AllocationInst->dump();
            if (auto *AIST =
                    dyn_cast<StructType>(AllocationInst->getAllocatedType()))
              // AIST->dump();
              // errs() << '\n';
              if (AIST->getName().str().compare(StructName) == 0) {
                SmallVector<DbgVariableIntrinsic *> DbgUsers;
                findDbgUsers(DbgUsers, AllocationInst);
                // for (const auto &GV : M.getGlobalList())
                //   GV.dump();
                // if (auto *Subprogram = dyn_cast<DISubprogram>(
                //         AllocationInst->getDebugLoc()->getScope())) {
                DISubprogram *Subprogram = F.getSubprogram();

                DIBuilder DB(M, false, Subprogram->getUnit());
                for (auto *DbgUser : DbgUsers)
                  //   DbgUser->getOperand(1)->dump();
                  if (auto *LocalVar =
                          dyn_cast<DILocalVariable>(DbgUser->getVariable()))
                    if (IsDbgInfoAlreadyUpdated.count(LocalVar->getType()) ==
                        0) {
                      bool Updated = createNewCompositeType(DB, M, LocalVar, F.getName().str(),
                                             FieldUsage, false);
                      if (Updated)
                        IsDbgInfoAlreadyUpdated.insert(LocalVar->getType());
                    }

                AllocaInst *NewAllocationInst =
                    new AllocaInst(NewStruct, AllocationInst->getAddressSpace(),
                                   AllocationInst->getArraySize(),
                                   DL.getPrefTypeAlign(NewStruct),
                                   AllocationInst->getName(), AllocationInst);
                // Function getType() returns ptr type (opaque pointer) for both
                // AllocationInst and NewAllocationInst which means they have the
                // same type. The difference is that getAllocatedType function
                // returns the old struct type for AllocationInst and the
                // NewStruct type for NewAllocationInst.
                AllocationInst->replaceAllUsesWith(NewAllocationInst, false);
                AllocationInst->eraseFromParent();
              }
          } else if (auto *GEPInst = dyn_cast<GetElementPtrInst>(&I)) {
            // errs() << "Her\n";
            // GEPInst->getOperand(0)->dump();
            if (auto *GEPType =
                    dyn_cast<StructType>(GEPInst->getSourceElementType()))
              if (GEPType->getName().str().compare(StructName) == 0) {
                // GEPInst->dump();
                GetElementPtrInst *NewGEPInst = createNewGEPInst(
                    GEPInst, FieldUsage[StructName], NewStruct);
                GEPInst->replaceAllUsesWith(NewGEPInst, false);
                NewGEPInst->copyMetadata(*GEPInst);
                GEPInst->eraseFromParent();
              }
          }
          // Fix the length of the memory block to copy to represent the
          // size of the new struct.
          else if (auto *MCInst = dyn_cast<MemCpyInst>(&I)) {
            // errs() << "Start\n";
            // MCInst->getSource()->getType()->dump();
            // MCInst->getDest()->getType()->dump();
            // if (MCInst->getSource()->getType() ==
            // MCInst->getDest()->getType())
            //   errs() << "Are same\n";
            // else
            //   errs() << "Are not same\n";
            Value *Src = MCInst->getSource();
            Value *Dest = MCInst->getDest();
            bool IsSrcTypeMatch = isGlobVarOrAllocaInstWithSameType(Src, StructName);
            bool IsDestTypeMatch = isGlobVarOrAllocaInstWithSameType(Dest, StructName);
            if (IsSrcTypeMatch && IsDestTypeMatch) {
              // MCInst->dump();
              DataLayout DL(&M);
              uint64_t Offset = 0;
              uint64_t MaxAlignInBits = 0;

              for (auto *SubType : NewStruct->subtypes()) {
                uint64_t FieldSizeInBits =
                    DL.getTypeAllocSizeInBits(SubType);
                if (FieldSizeInBits > MaxAlignInBits)
                  MaxAlignInBits = FieldSizeInBits;

                if (Offset % FieldSizeInBits != 0)
                  Offset = findFirstLarger(Offset, FieldSizeInBits);

                Offset += FieldSizeInBits;

                // errs() << Offset << '\n';
              }

              uint64_t NewSizeInBits = Offset;
              if (MaxAlignInBits > 0 &&
                  (NewSizeInBits % MaxAlignInBits != 0))
                NewSizeInBits =
                    findFirstLarger(NewSizeInBits, MaxAlignInBits);

              if (auto *OldLength =
                      dyn_cast<ConstantInt>(MCInst->getLength()))
                MCInst->setLength(Constant::getIntegerValue(
                    MCInst->getLength()->getType(),
                    APInt(OldLength->getBitWidth(), NewSizeInBits / 8)));
            }
          }

    InvalidIndexes.clear();
    // M.getContext().pImpl->NamedStructTypes["hello"] = NewStruct;

    changeSourceElementTypeForGEPCEs(M, StructName, FieldUsage[StructName],
                                     NewStruct);

    // The name of the old struct is changed to an irrelevant name so that the
    // new struct can have its old name.
    StructType::getTypeByName(M.getContext(), StructName)
        ->setName(StructName + " ");
    NewStruct->setName(StructName);
  }

  // errs() << '\n' << "Globals:\n\n";

  // for (const auto &elem : M.getGlobalList())
  //   if (isa<GlobalVariable>(elem))
  //     if
  //     (isa<StructType>(elem.getType()->getNonOpaquePointerElementType()))
  //     {
  //       elem.dump();
  //     }

  // // for (const auto &elem : M.getGlobalList())
  // //   if (isa<GlobalVariable>(elem))
  // //     if (auto *TempType = dyn_cast<StructType>(
  // //             elem.getType()->getNonOpaquePointerElementType())) {
  // //       errs() << TempType->getName() << '\n';
  // //     }

  // errs() << '\n';

  AllStructs = M.getIdentifiedStructTypes();

  errs() << "Struct types:\n\n";

  for (StructType *ST : AllStructs)
    ST->dump();

  errs() << '\n';

  // errs() << "Module metadata:\n";

  changeDebugInfoForNewStructs(M, FieldUsage, IsDbgInfoAlreadyUpdated);

  errs() << "Module start\n";

  M.dump();

  errs() << "Module end\n";

  // errs() << "\nConstant table:\n";

  // for (auto &elem : M.getContext().pImpl->ExprConstants)
  //   elem->dump();

  // errs() << '\n';

  // errs() << "Start:\n";
  // for (Function &F : M)
  //   for (BasicBlock &BB : F)
  //     for (Instruction &I : BB)
  //         if (isa<LoadInst>(I) || isa<StoreInst>(I))
  //           I.dump();

  errs() << "Hello from new optimization!\n";
  return PreservedAnalyses::none();
}

namespace {
struct USFELegacyPass : public ModulePass {
  static char ID;
  USFELegacyPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    if (skipModule(M))
      return false;
    UnusedStructureFieldsEliminationPass USFEP =
        UnusedStructureFieldsEliminationPass();
    ModuleAnalysisManager DummyMAM;
    PreservedAnalyses PA = USFEP.run(M, DummyMAM);
    return !PA.areAllPreserved();
  }
};
} // end of anonymous namespace

char USFELegacyPass::ID = 0;

static RegisterPass<USFELegacyPass>
    USFE("unused-struct-fields-elim", "Remove unused fields from structures.");
