add_subdirectory(kcm)

set(kdepasswd_SRCS
    kdepasswd.cpp
    passwd.cpp
    passwddlg.cpp)

add_executable(kdepasswd ${kdepasswd_SRCS})

target_link_libraries(kdepasswd KF5::KIOWidgets KF5::Su KF5::KDE4Support)

install(TARGETS kdepasswd ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS kdepasswd.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
