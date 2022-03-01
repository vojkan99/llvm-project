#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {

    struct HelloWord : public FunctionPass {                    // FunctionPass operise nad jednom po jednom funkcijom u modulu i dok radi nad jednom funkcijom, samo nju vidi i sme da menja, nista van, pa ni globalne promenljive
        
        static char ID;                                          // identifikator prolaza
        HelloWord() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override {               // metoda koju nadjacavamo i koja sadrzi sustinski nas prolaz
            errs() << "Hello: ";
            errs().write_escaped(F.getName()) << '\n';
            return false;                                        // vraca se obicno iz ovih funkcija koje nadjacavamo false ako nismo menjali u nasoj optimizaciji nista u LLVM IR kodu (valjda), a true ako jesmo
        }

    };

}

char HelloWord::ID = 0;                                         // nebitna vrednost jer LLVM koristi adresu od tog polja kao identifikator prolaza

static RegisterPass<HelloWord> X("helloWord", "Hello Word Pass",        // helloWold je ime kojim se prolaz oznacava kada se zadaje kao argument na komandnoj liniji (recimo kod komande opt), Hello World Pass je ime prolaza, HelloWord je ime klase koja sadrzi prolaz
                             false /* Only looks at CFG */,      // true je ako samo prolazimo CFG-om, a da ga pritom ne modifikujemo
                             false /* Analysis Pass */);         // true je ako je prolaz tipa analyis pass

static RegisterStandardPasses Y(                                 // registracija prolaza unutar postojeceg pipeline-a na odredjenom mestu, ima ih nekoliko mogucih (PassManagerBuilder::EP_EarlyAsPossible kaze da se prolaz koji specificiramo radi pre bilo kojih drugih optimizacija, a PassManagerBuilder::EP_FullLinkTimeOptimizationList kaze da se izvrsi posle Link Time optimizacija)
    PassManagerBuilder::EP_EarlyAsPossible,
    [] (const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
        PM.add(new HelloWord());
    }
);
