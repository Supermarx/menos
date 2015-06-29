project(menos)

find_package(yaml-cpp REQUIRED)
find_package(Boost COMPONENTS system program_options regex locale date_time filesystem REQUIRED)

include_directories(SYSTEM
                    ${Boost_INCLUDE_DIRS}
					${yaml-cpp_INCLUDE_DIRS}
                    ${SuperMarx_INCLUDE_DIRS})
                    
add_library(menoscore scraper.cpp)
target_link_libraries(menoscore
					  supermarx-scraper
					  ${yaml-cpp_LIBRARIES}
                      ${Boost_LIBRARIES}) 

add_executable(menos main.cpp)
target_link_libraries(menos
					  menoscore
					  supermarx-api)

include_directories(${menos_INCLUDE_DIRS})
