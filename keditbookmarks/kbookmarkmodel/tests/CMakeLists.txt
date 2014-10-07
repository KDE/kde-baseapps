include(ECMAddTests)

set(QT_REQUIRED_VERSION "5.2.0")
find_package(Qt5Test ${QT_REQUIRED_VERSION} CONFIG REQUIRED)

set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

###### kbookmarkmodeltest ######

set(kbookmarkmodeltest_SRCS kbookmarkmodeltest.cpp)
add_executable(kbookmarkmodeltest ${kbookmarkmodeltest_SRCS})
add_test(kbookmarkmodeltest kbookmarkmodeltest)
ecm_mark_as_test(kbookmarkmodeltest)
target_link_libraries(kbookmarkmodeltest kbookmarkmodel_private KF5::KIOCore Qt5::Test)
