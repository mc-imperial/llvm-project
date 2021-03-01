//===---- tools/extra/ToolTemplate.cpp - Template for refactoring tool ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements an empty refactoring tool using the clang tooling.
//  The goal is to lower the "barrier to entry" for writing refactoring tools.
//
//  Usage:
//  tool-template <cmake-output-dir> <file1> <file2> ...
//
//  Where <cmake-output-dir> is a CMake build directory in which a file named
//  compile_commands.json exists (enable -DCMAKE_EXPORT_COMPILE_COMMANDS in
//  CMake to get this output).
//
//  <file1> ... specify the paths of files in the CMake source tree. This path
//  is looked up in the compile command database. If the path of a file is
//  absolute, it needs to point into CMake's source tree. If the path is
//  relative, the current working directory needs to be in the CMake source
//  tree and the file must be in a subdirectory of the current working
//  directory. "./" prefixes in the relative files will be automatically
//  removed, but the rest of a relative path must be a suffix of a path in
//  the compile command line database.
//
//  For example, to use tool-template on all files in a subtree of the
//  source tree, use:
//
//    /path/in/subtree $ find . -name '*.cpp'|
//        xargs tool-template /path/to/build
//
//===----------------------------------------------------------------------===//

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

#include <deque>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <unordered_set>
#include <unordered_map>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

namespace {

class AddAtomicVisitor : public RecursiveASTVisitor<AddAtomicVisitor> {
 public:
  using DeclWithIndirection = std::pair<DeclaratorDecl*, int>;

  explicit AddAtomicVisitor(CompilerInstance *CI) : AstContext(&(CI->getASTContext())) { }

  const std::vector<DeclaratorDecl*>& getDecls() const {
    return Decls;
  }

  const std::unordered_map<DeclaratorDecl*,
                 std::unordered_map<int, std::set<DeclWithIndirection>>>& getEquivalentTypes() const {
    return EquivalentTypes;
  }

  bool VisitDeclaratorDecl(DeclaratorDecl *DD) {
    assert(ObservedDecls.count(DD) == 0);
    assert(EquivalentTypes.count(DD) == 0);
    ObservedDecls.insert(DD);
    EquivalentTypes.insert({DD, {}});
    Decls.push_back(DD);
    return true;
  }

  bool VisitExpr(Expr *E) {
    assert(EquivalentTypesInternal.count(E) == 0);
    EquivalentTypesInternal.insert({E, {}});
    return true;
  }

  bool TraverseFunctionDecl(FunctionDecl *FD) {
    assert(EnclosingFunction == nullptr);
    EnclosingFunction = FD;
    bool Result = RecursiveASTVisitor::TraverseFunctionDecl(FD);
    assert(EnclosingFunction == FD);
    EnclosingFunction = nullptr;
    return Result;
  }

  bool TraverseVarDecl(VarDecl *VD) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseVarDecl(VD);
    if (VD->hasInit()) {
      handleAssignment({VD, 0}, *VD->getInit());
    }
    return true;
  }

  bool TraverseMemberExpr(MemberExpr *ME) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseMemberExpr(ME);
    auto* FD = dyn_cast<FieldDecl>(ME->getMemberDecl());
    assert(FD != nullptr);
    EquivalentTypesInternal.at(ME).insert({FD, 0});
    return true;
  }

  bool TraverseDeclRefExpr(DeclRefExpr *DRE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseDeclRefExpr(DRE);
    if (auto* DD = dyn_cast<DeclaratorDecl>(DRE->getDecl())) {
      EquivalentTypesInternal.at(DRE).insert({DD, 0});
    }
    return true;
  }

  bool TraverseCallExpr(CallExpr* CE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseCallExpr(CE);
    if (auto* FD = CE->getDirectCallee()) {
      EquivalentTypesInternal.at(CE).insert({FD, 0});
      for (size_t I = 0; I < FD->getNumParams(); I++) {
        handleAssignment({FD->getParamDecl(I), 0}, *CE->getArg(I));
      }
    }
    return true;
  }

  bool TraverseReturnStmt(ReturnStmt* RS) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseReturnStmt(RS);
    assert(EnclosingFunction != nullptr);
    handleAssignment({EnclosingFunction, 0}, *RS->getRetValue());
    return true;
  }

  bool TraverseImplicitCastExpr(ImplicitCastExpr* ICE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseImplicitCastExpr(ICE);
    handlePassUp(ICE, ICE->getSubExpr());
    return true;
  }

  bool TraverseParenExpr(ParenExpr* PE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseParenExpr(PE);
    handlePassUp(PE, PE->getSubExpr());
    return true;
  }

  bool TraverseConditionalOperator(ConditionalOperator* CO) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseConditionalOperator(CO);
    handlePassUp(CO, CO->getTrueExpr());
    handlePassUp(CO, CO->getFalseExpr());
    return true;
  }

  bool TraverseArraySubscriptExpr(ArraySubscriptExpr* ASE) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseArraySubscriptExpr(ASE);
    for (auto& DDWithIndirection : EquivalentTypesInternal.at(ASE->getBase())) {
      EquivalentTypesInternal.at(ASE).insert({DDWithIndirection.first, DDWithIndirection.second + 1});
    }
    return true;
  }

  bool TraverseUnaryOperator(UnaryOperator* UO) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseUnaryOperator(UO);
    auto* SubExpr = UO->getSubExpr();
    switch(UO->getOpcode()) {
    case clang::UO_AddrOf: {
      for (auto& DDWithIndirection : EquivalentTypesInternal.at(SubExpr)) {
        EquivalentTypesInternal.at(UO).insert({DDWithIndirection.first, DDWithIndirection.second - 1});
      }
      break;
    }
    case clang::UO_Deref: {
      for (auto& DDWithIndirection : EquivalentTypesInternal.at(SubExpr)) {
        EquivalentTypesInternal.at(UO).insert({DDWithIndirection.first, DDWithIndirection.second + 1});
      }
      break;
    }
    default:
      break;
    }
    return true;
  }

  bool TraverseBinaryOperator(BinaryOperator *BO) {
    RecursiveASTVisitor<AddAtomicVisitor>::TraverseBinaryOperator(BO);
    switch(BO->getOpcode()) {
    case clang::BO_EQ:
    case clang::BO_GE:
    case clang::BO_GT:
    case clang::BO_LE:
    case clang::BO_LT: {
      makeEquivalent(*BO->getLHS(), *BO->getRHS());
      break;
    }
    case clang::BO_Assign: {
      makeEquivalent(*BO->getLHS(), *BO->getRHS());
      handlePassUp(BO, BO->getLHS());
      break;
    }
    default:
      break;
    }
    return true;
  }

 private:

   void handlePassUp(Expr* E, Expr *SubExpr) {
     EquivalentTypesInternal.at(E).insert(EquivalentTypesInternal.at(SubExpr).begin(), EquivalentTypesInternal.at(SubExpr).end());
   }

   void handleAssignment(const DeclWithIndirection& DDWithIndirection, const Expr& E) {
     if (auto* ILE = dyn_cast<InitListExpr>(&E)) {
       QualType QT = DDWithIndirection.first->getType();
       if (auto* RT = QT->getAs<RecordType>()) {
         auto FieldIterator = RT->getDecl()->field_begin();
         for (uint32_t I = 0; I < ILE->getNumInits(); I++) {
           handleAssignment({*FieldIterator, 0}, *ILE->getInit(I));
           ++FieldIterator;
         }
       } else if (QT->isArrayType()) {
         for (uint32_t I = 0; I < ILE->getNumInits(); I++) {
           handleAssignment({DDWithIndirection.first, DDWithIndirection.second + 1}, *ILE->getInit(I));
         }
       } else {
         errs() << "Unexpected initializer list\n";
         exit(1);
       }
     } else {
       for (auto& OtherDDWithIndirection : EquivalentTypesInternal.at(&E)) {
         addEquivalenceOneWay(DDWithIndirection, OtherDDWithIndirection);
         addEquivalenceOneWay(OtherDDWithIndirection, DDWithIndirection);
       }
     }
   }

   void addEquivalenceOneWay(const DeclWithIndirection& DWI1, const DeclWithIndirection& DWI2) {
     if (EquivalentTypes.at(DWI1.first).count(DWI1.second) == 0) {
       EquivalentTypes.at(DWI1.first).insert({DWI1.second, {}});
     }
     EquivalentTypes.at(DWI1.first).at(DWI1.second).insert(DWI2);
   }

   void makeEquivalent(const Expr& E1, const Expr& E2) {
     for (auto& DDWI1 : EquivalentTypesInternal.at(&E1)) {
       for (auto& DDWI2 : EquivalentTypesInternal.at(&E2)) {
         addEquivalenceOneWay(DDWI1, DDWI2);
         addEquivalenceOneWay(DDWI2, DDWI1);
       }
     }
   }

  ASTContext* AstContext;

  // The declarations we have processed, in the order in which we found them.
  std::vector<DeclaratorDecl*> Decls;

  std::unordered_map<DeclaratorDecl*,
                               std::unordered_map<int, std::set<DeclWithIndirection>>> EquivalentTypes;

  // Intermediate variables for bottom-up processing:

  // The declarations we have seen in a form that can be efficiently queried.
  // Used so that we do not add to |Decls| a declaration we already processed.
  std::unordered_set<DeclaratorDecl*> ObservedDecls;

  std::unordered_map<const Expr*, std::set<DeclWithIndirection>> EquivalentTypesInternal;

  // Tracks the function currently being traversed, if any, so that when we
  // process a 'return' statement we know which function it relates to.
  FunctionDecl* EnclosingFunction = nullptr;

  // End of intermediate variables.

};

class AddAtomicASTConsumer : public ASTConsumer {
 public:
  explicit AddAtomicASTConsumer(CompilerInstance *CI, std::mt19937 *MT, const std::string& NameToUpgrade, const std::string& OutputFile) : CI(CI), MT(MT), NameToUpgrade(NameToUpgrade), OutputFile(OutputFile), Visitor(std::make_unique<AddAtomicVisitor>(CI)) {}

  void showDeclWithIndirection(const AddAtomicVisitor::DeclWithIndirection& DDWI) {
    if (DDWI.second == -1) {
      errs() << "&";
    } else {
      assert (DDWI.second >= 0);
      for (int i = 0; i < DDWI.second; i++) {
        errs() << "*";
      }
    }
    errs() << DDWI.first->getDeclName();
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    if (Context.getDiagnostics().hasErrorOccurred()) {
      // There has been an error, so we don't do any processing.
      return;
    }
    Visitor->TraverseDecl(Context.getTranslationUnitDecl());

    for (auto *DD : Visitor->getDecls()) {
      errs() << DD->getNameAsString() << "\n";
      for (auto& Entry : Visitor->getEquivalentTypes().at(DD)) {
        for (auto& InnerEntry : Entry.second) {
          errs() << "   ";
          showDeclWithIndirection({DD, Entry.first});
          errs() << " ~ ";
          showDeclWithIndirection(InnerEntry);
          errs() << "\n";
        }
      }
    }

    DeclaratorDecl* InitialUpgrade = nullptr;
    if (NameToUpgrade.empty()) {
      while (true) {
        int Index = std::uniform_int_distribution<size_t>(
            0, Visitor->getDecls().size() - 1)(*MT);
        auto *DD = Visitor->getDecls()[Index];
        if (CI->getSourceManager().getFileID(DD->getBeginLoc()) ==
            CI->getSourceManager().getMainFileID()) {
          InitialUpgrade = DD;
          break;
        }
      }
    } else {
      for (auto* DD : Visitor->getDecls()) {
        if (DD->getNameAsString() == NameToUpgrade) {
          InitialUpgrade = DD;
          break;
        }
      }
      if (InitialUpgrade == nullptr) {
        errs() << "Did not find a declarator declaration named " << NameToUpgrade << "\n";
        exit(1);
      }
    }

    std::unordered_map<DeclaratorDecl*, size_t> Upgrades;
    errs() << "Initially upgrading " << InitialUpgrade->getDeclName() << "\n";
    Upgrades.insert({InitialUpgrade, 0});
    std::deque<std::pair<DeclaratorDecl*, size_t>> UpgradesToPropagate;
    UpgradesToPropagate.push_back({InitialUpgrade, 0});
    while (!UpgradesToPropagate.empty()) {
      std::pair<DeclaratorDecl*, size_t> Current = UpgradesToPropagate.front();
      UpgradesToPropagate.pop_front();
      errs() << "Propagating upgrade " << Current.first->getDeclName() << " " << Current.second << "\n";
      for (auto& Entry : Visitor->getEquivalentTypes().at(Current.first)) {
        int ReconciledIndirectionLevel = static_cast<int>(Current.second) - Entry.first;
        if (ReconciledIndirectionLevel <= 0) {
          continue;
        }
        for (auto& DDWI : Entry.second) {
          int Temp = static_cast<int>(Current.second) + (DDWI.second - Entry.first);
          assert (Temp >= 0);
          size_t NewLevel = static_cast<size_t>(Temp);
          if (Upgrades.count(DDWI.first) != 0) {
            assert (Upgrades.at(DDWI.first) == NewLevel);
          } else {
            Upgrades.insert({DDWI.first, NewLevel});
            UpgradesToPropagate.push_back({DDWI.first, NewLevel});
          }
        }
      }
    }
    errs() << "Upgrades:\n";
    for (auto& Entry : Upgrades) {
      errs() << Entry.first->getDeclName() << " " << Entry.second << "\n";
    }

    TheRewriter.setSourceMgr(CI->getSourceManager(), CI->getLangOpts());
    for (auto& Entry : Upgrades) {
      rewriteType(Entry.first->getTypeSourceInfo()->getTypeLoc(), Entry.second);
    }

    const RewriteBuffer *RewriteBuf =
        TheRewriter.getRewriteBufferFor(CI->getSourceManager().getMainFileID());

    std::ofstream OutputFileStream (OutputFile, std::ofstream::out);
    OutputFileStream << std::string(RewriteBuf->begin(), RewriteBuf->end());
    OutputFileStream.close();

  }

 private:

   void rewriteType(const TypeLoc& TL, size_t IndirectionLevel) {
     if (TL.getTypeLocClass() == TypeLoc::FunctionProto) {
       rewriteType(TL.castAs<FunctionProtoTypeLoc>().getReturnLoc(), IndirectionLevel);
       return;
     }
     if (TL.getTypeLocClass() == TypeLoc::FunctionNoProto) {
       rewriteType(TL.castAs<FunctionNoProtoTypeLoc>().getReturnLoc(), IndirectionLevel);
       return;
     }
     if (IndirectionLevel == 0) {
       TheRewriter.InsertTextAfterToken(TL.getEndLoc(), " _Atomic ");
       return;
     }
     if (TL.getTypeLocClass() == TypeLoc::Pointer) {
       rewriteType(TL.castAs<PointerTypeLoc>().getPointeeLoc(), IndirectionLevel - 1);
       return;
     }
     if (TL.getTypeLocClass() == TypeLoc::ConstantArray) {
       rewriteType(TL.castAs<ConstantArrayTypeLoc>().getElementLoc(), IndirectionLevel - 1);
       return;
     }
     errs() << "Unhandled type loc " << TL.getTypeLocClass() << "\n";
     exit(1);
   }

   CompilerInstance *CI;
   std::mt19937 *MT;
  const std::string& NameToUpgrade;
   const std::string& OutputFile;
   std::unique_ptr<AddAtomicVisitor> Visitor;
   Rewriter TheRewriter;
};

class AddAtomicFrontendAction : public ASTFrontendAction {
 public:
   explicit AddAtomicFrontendAction(std::mt19937 *MT, const std::string& NameToUpgrade, const std::string& OutputFile) : MT(MT), NameToUpgrade(NameToUpgrade), OutputFile(OutputFile) { }

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    return std::make_unique<AddAtomicASTConsumer>(&CI, MT, NameToUpgrade, OutputFile);
  }

private:
  std::mt19937 *MT;
  const std::string& NameToUpgrade;
  const std::string& OutputFile;
};

std::unique_ptr<FrontendActionFactory> newAddAtomicFrontendActionFactory(std::mt19937 * MT, const std::string& NameToUpgrade, const std::string& OutputFilename) {
  class AddAtomicFrontendActionFactory : public FrontendActionFactory {
  public:
    explicit AddAtomicFrontendActionFactory(std::mt19937 *MT, const std::string& NameToUpgrade, const std::string& OutputFilename) : MT(MT), NameToUpgrade(NameToUpgrade), OutputFilename(OutputFilename) {}

    std::unique_ptr<FrontendAction> create() override {
      return std::make_unique<AddAtomicFrontendAction>(MT, NameToUpgrade, OutputFilename);
    }

  private:
    std::mt19937 * MT;
    const std::string& NameToUpgrade;
    const std::string& OutputFilename;
  };

  return std::make_unique<AddAtomicFrontendActionFactory>(
      MT, NameToUpgrade, OutputFilename);
}

} // end anonymous namespace

// Set up the command line options
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::OptionCategory AddAtomicCategory("add-atomic options");
static cl::opt<std::string> OutputFilename("o", cl::desc("Specify output filename"), cl::value_desc("filename"));
static cl::opt<std::string> Seed("seed", cl::desc("Specify seed for random number generation"), cl::value_desc("seed"));
static cl::opt<std::string> Name("name", cl::desc("Specify name of declaration to upgrade"), cl::value_desc("name"));


int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  Expected<CommonOptionsParser> Op = CommonOptionsParser::create(argc, argv, AddAtomicCategory);
  if (!Op) {
    return 1;
  }

  if (OutputFilename.empty()) {
    errs() << "Please specify an output filename using the -o option.";
    return 1;
  }


  ClangTool Tool(Op.get().getCompilations(), Op.get().getSourcePathList());

  int64_t SeedValue = Seed.empty() ? static_cast<int64_t>(std::random_device()()) : std::atoll(Seed.c_str());

  std::string NameValue = Name.empty() ? "" : Name.getValue();

  std::mt19937 MT(SeedValue);

  std::unique_ptr<FrontendActionFactory> Factory = newAddAtomicFrontendActionFactory(&MT, Name, OutputFilename.getValue());

  int Result = Tool.run(Factory.get());

  return Result;
}
