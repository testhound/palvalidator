set(LIB_NAME pasearchalgo)
set(SRC_LIST
ComparableBar.h
PalStrategyAlwaysOn.h
ComparisonToPalStrategy.h
ComparisonsGenerator.h
UniqueSinglePAMatrix.h
BacktestResultBaseGenerator.h
ShortcutSearchAlgoBacktester.h
OriginalSearchAlgoBacktester.h
ForwardStepwiseSelector.h
BacktestProcessor.h
SteppingPolicy.h
SurvivalPolicy.h
)
add_library(${LIB_NAME} STATIC ${SRC_LIST})
SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_NAME} priceaction2 ${CMAKE_THREAD_LIBS_INIT})
