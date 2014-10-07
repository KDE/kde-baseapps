add_subdirectory(tests)

set(kbookmarkmodel_SRCS
  commandhistory.cpp
  commands.cpp
  model.cpp
  treeitem.cpp
  view.cpp
)

add_library(kbookmarkmodel_private SHARED ${kbookmarkmodel_SRCS})
target_link_libraries(kbookmarkmodel_private KF5::KIOCore)
target_link_libraries(kbookmarkmodel_private LINK_INTERFACE_LIBRARIES KF5::KIOCore)

set_target_properties(kbookmarkmodel_private PROPERTIES
     VERSION ${GENERIC_LIB_VERSION}
     SOVERSION ${GENERIC_LIB_SOVERSION}
)

install(TARGETS kbookmarkmodel_private ${INSTALL_TARGETS_DEFAULT_ARGS})
