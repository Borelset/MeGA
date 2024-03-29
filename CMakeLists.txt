cmake_minimum_required(VERSION 3.10)
project(MeGA)

set(CMAKE_CXX_STANDARD 14)

aux_source_directory(Utility Utility)
aux_source_directory(RollHash RollHash)
aux_source_directory(MetadataManager MetadataManager)
aux_source_directory(DedupPipeline DedupPipeline)
aux_source_directory(RestorePipeline RestorePipeline)
aux_source_directory(ArrangementPipeline ArrangementPipeline)
aux_source_directory(Rollhash Rollhash)
add_subdirectory(gflags-2.2.2)
add_subdirectory(xdelta)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

link_libraries(gflags::gflags isal_crypto pthread crypto jemalloc zstd xdelta)

add_executable(MeGA main.cpp ${Utility} ${RollHash} ${MetadataManager} ${Pipeline} ${RestorePipeline} ${ArrangementPipeline} ${Rollhash})