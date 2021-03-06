cmake_minimum_required(VERSION 2.8.5 FATAL_ERROR)

PROJECT (TutorialVis)

find_package(OpenGL REQUIRED)
find_package(Qt5Widgets REQUIRED)

include_directories(
	${SHARED_LIB_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${PROJECT_BINARY_DIR}
	${CMAKE_BINARY_DIR}
	${CUDA_TOOLKIT_INCLUDE} 
	required)

set( Srcs 
	main.cpp 
	window.cpp 
	)
set( Hdrs  
	window.h 
    )


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

add_executable(${PROJECT_NAME} ${Srcs} ${Hdrs} )
qt5_use_modules(${PROJECT_NAME} OpenGL Gui Widgets)
target_link_libraries(${PROJECT_NAME} 
	Qt5::Widgets  
	${OPENGL_LIBRARIES} 
	deform
	io
	interact
	render
	dataModel
	)

if(USE_OSVR)
	target_link_libraries(${PROJECT_NAME} osvr::osvrClientKitCpp)
endif()


if(USE_LEAP)
	include_directories(${Leap_ROOT_DIR}/include)
	target_link_libraries(${PROJECT_NAME} ${Leap_IMPORT_LIB_DEBUG})
endif()