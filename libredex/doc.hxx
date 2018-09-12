
/**
  \mainpage

  \section intro Introduction

  This is the doxygen documentation of redex code. Unfortunately, the codebase is not documented in any
  useful way, although several pieces of code have documenting comments in them. 

  \subsection intro_exec_redex Executing redex

  The \b redex command is actually a self-extracting file, which extracts into a python wrapper and the
  \b redex-all binary. The python wrapper is responsible for unpacking an .apk file, exposing the .dex file(s) in it.
  Then, \b redex-all analyses and rewrites the .dex files, and finally the python wrapper reassemples the 
  .apk file. The complete logic of the python wrapper is contained in file redex.py and in the pyredex package.

  \subsection intro_redex_all The execution of redex-all

  The file containing the \c main() function is  tools/redex-all/main.cpp. At a high level, the tasks performed
  in the main function are the following

    1. Arguments are parsed.
    2. The RedexContext global object, `g_redex`, is created
    3. The %ProGuard configuration is read
    4. The root DexStore object is created. It corresponds to the first .dex file on the command line.
        If more .dex files are there on the command line (a so-called multidex .apk is processed), then more
        DexStore objects are created (one per .dex file). <br/>
		Note that the file on the command line must end in '.dex', or else it must be a so-called "metadata"
		file (see DexMetadata for more). This requires further examination...
	5. A list of external .jar files ("library jars") is also read into a Scope obect 
	6. Names are de-obfuscated by using ProGuard map files.
	7. A PassManager is created and passes are executed according to configuration
	8. Optimized .dex files are written to disk.

\section Passes

A pass is defined as a subclass of abstract class Pass. The execution of passes happens in two steps.
In the first step, method Pass::eval_pass() is executed for each pass. In the second step, method Pass::run_pass()
is executed. The idea is that during the first step, each pass can see the original input in the dex stores
whereas in the second pass the order of passes affects what each pass encounters.

Each Pass subclass is a singleton class and it is instantiated as a static. Note: care must be taken when linking, but
apparently the current build machinery has this nailed down.

\subsection Implementing a pass

\section The DEX file model

- DEX file model DexClass.h
  - DexClasses and Scope are both aliases for std::vector<DexClass*>
- walkers in Walkers.h
- dex refs and rebinding ReBindRefs



*/

/** @file tools/redex-all/main.cpp */

// These declarations declare header files in libredex as public

/** \file ApkManager.h */
/** \file CallGraph.h */
/** \file ClassHierarchy.h */
/** \file ConcurrentContainers.h */
/** \file ConfigFiles.h */
/** \file ControlFlow.h */
/** \file Creators.h */
/** \file Dataflow.h */
/* \file Debug.h */
/** \file DexAccess.h */
/** \file DexAnnotation.h */
/** \file DexAsm.h */
/** \file DexClass.h */
/** \file DexDebugInstruction.h */
/** \file DexIdx.h */
/** \file DexInstruction.h */
/** \file DexLoader.h */
/** \file DexMemberRefs.h */
/** \file DexOpcode.h */
/** \file DexOutput.h */
/** \file DexPosition.h */
/** \file DexStore.h */
/** \file DexUtil.h */
/** \file Gatherable.h */
/** \file ImmutableSubcomponentAnalyzer.h */
/** \file Inliner.h */
/** \file InstructionAnalyzer.h */
/** \file InstructionLowering.h */
/** \file IRAssembler.h */
/** \file IRCode.h */
/** \file IRInstruction.h */
/** \file IRList.h */
/** \file IROpcode.h */
/** \file IRTypeChecker.h */
/** \file JarLoader.h */
/** \file Match.h */
/** \file MethodDevirtualizer.h */
/** \file Mutators.h */
/** \file Pass.h */
/** \file PassManager.h */
/** \file PassRegistry.h */
/** \file PluginRegistry.h */
/** \file PointsToSemantics.h */
/** \file PointsToSemanticsUtils.h */
/** \file PrintSeeds.h */
/** \file ProguardConfiguration.h */
/** \file ProguardLexer.h */
/** \file ProguardMap.h */
/** \file ProguardMatcher.h */
/** \file ProguardParser.h */
/** \file ProguardPrintConfiguration.h */
/** \file ProguardRegex.h */
/** \file ProguardReporting.h */
/** \file ReachableClasses.h */
/** \file ReachableObjects.h */
/** \file RedexContext.h */
/** \file RedexResources.h */
/** \file ReferencedState.h */
/** \file Resolver.h */
/** \file Show.h */
/** \file SimpleReflectionAnalysis.h */
/** \file SimpleValueAbstractDomain.h */
/** \file StringBuilder.h */
/** \file Timer.h */
/** \file Trace.h */
/** \file Transform.h */
/** \file TypeSystem.h */
/** \file Vinfo.h */
/** \file VirtualScope.h */
/** \file Walkers.h */
/** \file Warning.h */
/** \file WorkQueue.h */

