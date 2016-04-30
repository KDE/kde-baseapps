add_subdirectory(tests)

set(kbookmarkmodel_SRCS
  commandhistory.cpp
  commands.cpp
  model.cpp
  treeitem.cpp
  view.cpp
)

add_library(kbookmarkmodel_private SHARED ${kbookmarkmodel_SRCS})
target_link_libraries(kbookmarkmodel_private
PUBLIC
   Qt5::Widgets
   KF5::XmlGui # KActionCollection
PRIVATE
   KF5::Bookmarks
   KF5::I18n
)

generate_export_header(kbookmarkmodel_private BASE_NAME kbookmarkmodel EXPORT_FILE_NAME kbookmarkmodel_export.h)

set_target_properties(kbookmarkmodel_private PROPERTIES
     VERSION ${KEDITBOOKMARKS_VERSION}
     SOVERSION "6"
)


install(TARGETS kbookmarkmodel_private ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
