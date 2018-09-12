AM_CXXFLAGS = --std=gnu++14
AM_CPPFLAGS = \
	-I$(top_srcdir)/test/googletest-release-1.7.0/include \
	-I$(top_srcdir)/abstract_interpretation/include \
	-I$(top_srcdir)/liblocator \
	-I$(top_srcdir)/libredex \
	-I$(top_srcdir)/libresource \
	-I$(top_srcdir)/libresource/android \
	-I$(top_srcdir)/libresource/androidfw \
	-I$(top_srcdir)/libresource/cutils \
	-I$(top_srcdir)/libresource/system \
	-I$(top_srcdir)/libresource/utils \
	-I$(top_srcdir)/opt \
	-I$(top_srcdir)/opt/access-marking \
	-I$(top_srcdir)/opt/add_redex_txt_to_apk \
	-I$(top_srcdir)/opt/analysis_ref_graph \
	-I$(top_srcdir)/opt/annoclasskill \
	-I$(top_srcdir)/opt/annokill \
	-I$(top_srcdir)/opt/bridge \
	-I$(top_srcdir)/opt/check_breadcrumbs \
	-I$(top_srcdir)/opt/constant-propagation \
	-I$(top_srcdir)/opt/dedup_blocks \
	-I$(top_srcdir)/opt/delinit \
	-I$(top_srcdir)/opt/delsuper \
	-I$(top_srcdir)/opt/final_inline \
	-I$(top_srcdir)/opt/hotness-score \
	-I$(top_srcdir)/opt/inlineinit \
	-I$(top_srcdir)/opt/instrument \
	-I$(top_srcdir)/opt/interdex \
	-I$(top_srcdir)/opt/local-dce \
	-I$(top_srcdir)/opt/obfuscate \
	-I$(top_srcdir)/opt/original_name \
	-I$(top_srcdir)/opt/outliner \
	-I$(top_srcdir)/opt/peephole \
	-I$(top_srcdir)/opt/print-members \
	-I$(top_srcdir)/opt/rebindrefs \
	-I$(top_srcdir)/opt/redundant_move_elimination \
	-I$(top_srcdir)/opt/regalloc \
	-I$(top_srcdir)/opt/remove-builders \
	-I$(top_srcdir)/opt/remove-unreachable \
	-I$(top_srcdir)/opt/remove_empty_classes \
	-I$(top_srcdir)/opt/remover \
	-I$(top_srcdir)/opt/remove_gotos \
	-I$(top_srcdir)/opt/renameclasses \
	-I$(top_srcdir)/opt/reorder-interfaces \
	-I$(top_srcdir)/opt/shorten-srcstrings \
	-I$(top_srcdir)/opt/simpleinline \
	-I$(top_srcdir)/opt/singleimpl \
	-I$(top_srcdir)/opt/static-sink \
	-I$(top_srcdir)/opt/staticrelo \
	-I$(top_srcdir)/opt/string_simplification \
	-I$(top_srcdir)/opt/strip-debug-info \
	-I$(top_srcdir)/opt/synth \
	-I$(top_srcdir)/opt/test_cfg \
	-I$(top_srcdir)/opt/track_resources \
	-I$(top_srcdir)/opt/unreferenced_interfaces \
	-I$(top_srcdir)/opt/unterface \
	-I$(top_srcdir)/opt/verifier \
	-I$(top_srcdir)/opt/virtual_scope \
	-I$(top_srcdir)/service/constant-propagation \
	-I$(top_srcdir)/service/dataflow \
	-I$(top_srcdir)/service/reference-update \
	-I$(top_srcdir)/service/switch-dispatch \
	-I$(top_srcdir)/shared \
	-I$(top_srcdir)/tools/common \
	-I$(top_srcdir)/tools/redexdump \
	-I$(top_srcdir)/util \
	-I/usr/include/jsoncpp

TESTS = \
	synth_test \
	propagation_test \
	constant_propagation_test

# vsam: This target has no files here, they were moved to the ../instr directory
#	empty_classes_test 

TEST_LIBS = $(top_builddir)/test/libgtest_main.la $(top_builddir)/libredex.la \
	$(top_srcdir)/libredex/DexAsm.o \
	$(top_srcdir)/opt/access-marking/AccessMarking.o \
	$(top_srcdir)/opt/add_redex_txt_to_apk/AddRedexTxtToApk.o \
	$(top_srcdir)/opt/analysis_ref_graph/ReferenceGraphCreator.o \
	$(top_srcdir)/opt/annokill/AnnoKill.o \
	$(top_srcdir)/opt/bridge/Bridge.o \
	$(top_srcdir)/opt/check_breadcrumbs/CheckBreadcrumbs.o \
	$(top_srcdir)/opt/constant-propagation/ConstantPropagation.o \
	$(top_srcdir)/opt/constant-propagation/ConstantPropagationRuntimeAssert.o \
	$(top_srcdir)/opt/constant-propagation/ConstantPropagationTransform.o \
	$(top_srcdir)/opt/constant-propagation/IPConstantPropagation.o \
	$(top_srcdir)/opt/copy-propagation/AliasedRegisters.o \
	$(top_srcdir)/opt/copy-propagation/CopyPropagationPass.o \
	$(top_srcdir)/opt/dedup_blocks/DedupBlocksPass.o \
	$(top_srcdir)/opt/delinit/DelInit.o \
	$(top_srcdir)/opt/delsuper/DelSuper.o \
	$(top_srcdir)/opt/final_inline/FinalInline.o \
	$(top_srcdir)/opt/hotness-score/HotnessScore.o \
	$(top_srcdir)/opt/inlineinit/InlineInit.o \
	$(top_srcdir)/opt/instrument/Instrument.o \
	$(top_srcdir)/opt/interdex/InterDex.o \
	$(top_srcdir)/opt/local-dce/LocalDce.o \
	$(top_srcdir)/opt/obfuscate/Obfuscate.o \
	$(top_srcdir)/opt/obfuscate/ObfuscateUtils.o \
	$(top_srcdir)/opt/obfuscate/VirtualRenamer.o \
	$(top_srcdir)/opt/original_name/OriginalNamePass.o \
	$(top_srcdir)/opt/outliner/Outliner.o \
	$(top_srcdir)/opt/peephole/Peephole.o \
	$(top_srcdir)/opt/peephole/RedundantCheckCastRemover.o \
	$(top_srcdir)/opt/print-members/PrintMembers.o \
	$(top_srcdir)/opt/reachability_graph/ReachabilityGraphPrinter.o \
	$(top_srcdir)/opt/rebindrefs/ReBindRefs.o \
	$(top_srcdir)/opt/regalloc/GraphColoring.o \
	$(top_srcdir)/opt/regalloc/Interference.o \
	$(top_srcdir)/opt/regalloc/RegAlloc.o \
	$(top_srcdir)/opt/regalloc/RegisterType.o \
	$(top_srcdir)/opt/regalloc/Split.o \
	$(top_srcdir)/opt/regalloc/VirtualRegistersFile.o \
	$(top_srcdir)/opt/remove-builders/RemoveBuilders.o \
	$(top_srcdir)/opt/remove-builders/RemoveBuildersHelper.o \
	$(top_srcdir)/opt/remove-unreachable/RemoveUnreachable.o \
	$(top_srcdir)/opt/remover/Remover.o \
	$(top_srcdir)/opt/remove-unread-fields/RemoveUnreadFields.o \
	$(top_srcdir)/opt/remove_empty_classes/RemoveEmptyClasses.o \
	$(top_srcdir)/opt/remove_gotos/RemoveGotos.o \
	$(top_srcdir)/opt/renameclasses/RenameClasses.o \
	$(top_srcdir)/opt/renameclasses/RenameClassesV2.o \
	$(top_srcdir)/opt/reorder-interfaces/ReorderInterfaces.o \
	$(top_srcdir)/opt/shorten-srcstrings/Shorten.o \
	$(top_srcdir)/opt/simpleinline/Deleter.o \
	$(top_srcdir)/opt/simpleinline/SimpleInline.o \
	$(top_srcdir)/opt/singleimpl/SingleImpl.o \
	$(top_srcdir)/opt/singleimpl/SingleImplAnalyze.o \
	$(top_srcdir)/opt/singleimpl/SingleImplOptimize.o \
	$(top_srcdir)/opt/singleimpl/SingleImplStats.o \
	$(top_srcdir)/opt/singleimpl/SingleImplUtil.o \
	$(top_srcdir)/opt/static-sink/StaticSink.o \
	$(top_srcdir)/opt/staticrelo/StaticRelo.o \
	$(top_srcdir)/opt/string_simplification/StringIterator.o \
	$(top_srcdir)/opt/string_simplification/StringSimplification.o \
	$(top_srcdir)/opt/strip-debug-info/StripDebugInfo.o \
	$(top_srcdir)/opt/synth/Synth.o \
	$(top_srcdir)/opt/test_cfg/TestCFG.o \
	$(top_srcdir)/opt/track_resources/TrackResources.o \
	$(top_srcdir)/opt/unreferenced_interfaces/UnreferencedInterfaces.o \
	$(top_srcdir)/opt/unterface/Unterface.o \
	$(top_srcdir)/opt/unterface/UnterfaceOpt.o \
	$(top_srcdir)/opt/verifier/Verifier.o \
	$(top_srcdir)/opt/virtual_scope/MethodDevirtualizationPass.o \
	$(top_srcdir)/service/constant-propagation/ConstantEnvironment.o \
	$(top_srcdir)/service/constant-propagation/ConstantPropagationAnalysis.o \
	$(top_srcdir)/service/constant-propagation/ConstantPropagationWholeProgramState.o \
	$(top_srcdir)/service/constant-propagation/IPConstantPropagationAnalysis.o \
	$(top_srcdir)/service/constant-propagation/ObjectDomain.o \
	$(top_srcdir)/service/constant-propagation/SignDomain.o \
	$(top_srcdir)/service/dataflow/LiveRange.o \
	$(top_srcdir)/service/reference-update/MethodReference.o \
	$(top_srcdir)/service/reference-update/TypeReference.o \
	$(top_srcdir)/service/switch-dispatch/SwitchDispatch.o \
	$(BOOST_FILESYSTEM_LIB) \
	$(BOOST_SYSTEM_LIB) \
	$(BOOST_REGEX_LIB) \
	$(BOOST_PROGRAM_OPTIONS_LIB) \
	$(BOOST_THREAD_LIB) \
	-lpthread

synth_test_SOURCES = SynthTest.cpp
synth_test_LDADD = $(TEST_LIBS)
EXTRA_synth_test_DEPENDENCIES = synth-test-class.dex

#empty_classes_test_SOURCES = EmptyClassesTest.cpp
#empty_classes_test_LDADD = $(TEST_LIBS)
#EXTRA_empty_classes_test_DEPENDENCIES = empty-classes-test-class.dex

propagation_test_SOURCES = PropagationTest.cpp
propagation_test_LDADD = $(TEST_LIBS)
EXTRA_propagation_test_DEPENDENCIES = propagation-test-class.dex

constant_propagation_test_SOURCES = ConstantPropagationTest.cpp
constant_propagation_test_LDADD = $(TEST_LIBS)
EXTRA_constant_propagation_test_DEPENDENCIES = constant-propagation-test-class.dex

check_PROGRAMS = $(TESTS)

synth-test-class.jar: Alpha.java SynthTest.java
	mkdir -p synth-test-class
	$(JAVAC) -d synth-test-class $^
	$(JAR) cf $@ -C synth-test-class .

synth-test-class.dex: synth-test-class.jar
	$(DX) --dex --output=$@ $^

#empty-classes-test-class.jar: EmptyClasses.java EmptyClassesTest.java
#	mkdir -p empty-classes-test-class
#	$(JAVAC) -d empty-classes-test-class $^
#	$(JAR) cf $@ -C empty-classes-test-class .

#empty-classes-test-class.dex: empty-classes-test-class.jar
#	$(DX) --dex --output=$@ $^

propagation-test-class.jar: Propagation.java
	mkdir -p propagation-test-class
	$(JAVAC) -d propagation-test-class $^
	$(JAR) cf $@ -C propagation-test-class .

propagation-test-class.dex: propagation-test-class.jar
	$(DX) --dex --output=$@ $^

constant-propagation-test-class.jar: ConstantPropagation.java
	mkdir -p constant-propagation-test-class
	$(JAVAC) -d constant-propagation-test-class $^
	$(JAR) cf $@ -C constant-propagation-test-class .

constant-propagation-test-class.dex: constant-propagation-test-class.jar
	$(DX) --dex --output=$@ $^
