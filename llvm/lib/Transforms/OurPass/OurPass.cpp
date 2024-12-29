#include "llvm/CodeGen/Register.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"


using namespace llvm;
namespace {
  struct OurPass : public FunctionPass {
    static char ID;
    OurPass() : FunctionPass(ID) {}
    
    bool runOnFunction(Function &F) override {
      errs() << "OurPass: ";
      errs().write_escaped(F.getName()) << '\n';
      return false;
    }

  };

} // end anonymous namespace
