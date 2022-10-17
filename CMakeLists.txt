cmake_minimum_required(VERSION 2.8)

project(QPlayer)

# this line is for UNICODE release,which is required by DuiDesigner 
add_definitions(-DUNICODE -D_UNICODE)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/bin)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/include)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")

# add each CMake file
add_definitions(-DUILIB_STATIC)
add_subdirectory(duilib)
include_directories(. Duilib)

aux_source_directory(. source_files)
aux_source_directory(audio source_files)
aux_source_directory(dlna source_files)

add_executable(QPlayer ${source_files} QPlayer.rc)
set_target_properties(QPlayer PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
add_dependencies(QPlayer duilib)

target_link_libraries(QPlayer duilib avformat avcodec avutil swscale swresample)
#add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/bin/*.dll $<TARGET_FILE_DIR:${PROJECT_NAME}>)