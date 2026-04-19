cmake_minimum_required(VERSION 3.10)  
project(ftl_maker)

# Find the curl library
find_package(CURL REQUIRED)


# Find the jansson library
#find_package(Jansson REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED jansson)

if(JANSSON_FOUND)
    include_directories(${JANSSON_INCLUDE_DIRS})
    link_directories(${JANSSON_LIBRARY_DIRS})
    #add_executable(translate translate.c)
    #target_link_libraries(translate ${JANSSON_LIBRARIES})
else()
    message(FATAL_ERROR "Jansson library not found!")
endif()
  


# Add executable
add_executable(ftl_maker src/ftl_maker.c src/lib.c)

# Link libraries
target_link_libraries(ftl_maker
  CURL::libcurl
  jansson
)
