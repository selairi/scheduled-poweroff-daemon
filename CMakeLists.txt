cmake_minimum_required(VERSION 3.21) 
project(scheduled-poweroff-daemon VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)

include(FindPkgConfig)

pkg_check_modules(SYSTEMD REQUIRED libsystemd>=0.34)

configure_file(src/configure.h.in configure.h)

set(SCRIPT_FILES
  src/scheduled-poweroff-send
)

set(SHARED_FILES
  shared/translations.txt
)

set(CONFIG_FILE
  config/scheduled-poweroff.conf
)

set(DBUS_FILE
  config/local.sheduledpoweroffdaemon.conf 
)

set(SERVICE_FILE
  config/scheduled-poweroff.service
)

add_executable(scheduled-poweroff-daemon 
  src/scheduled-poweroff-daemon.cpp
)

add_executable(scheduled-poweroff-client 
  src/scheduled-poweroff-client.cpp
)

include_directories(${EXTRA_INCLUDES} "${PROJECT_BINARY_DIR}")
target_link_libraries(scheduled-poweroff-daemon ${SYSTEMD_LIBRARIES})
target_link_libraries(scheduled-poweroff-client ${SYSTEMD_LIBRARIES})

install(TARGETS scheduled-poweroff-daemon DESTINATION bin)
install(TARGETS scheduled-poweroff-client DESTINATION bin)
install(
  FILES ${SCRIPT_FILES}
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION bin
)
install(FILES ${SHARED_FILES} DESTINATION share/scheduled-poweroff-daemon)
install(FILES ${CONFIG_FILE} DESTINATION /etc)
install(FILES ${DBUS_FILE} DESTINATION /etc/dbus-1/system.d/)
install(
  FILES ${SERVICE_FILE}
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION /etc/systemd/system/
)
