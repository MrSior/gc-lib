# Script which find or install Catch2 lib

include(FetchContent)

if (NOT TARGET Catch2::Catch2)
    message(STATUS "Catch2 is not found, downloading ...")
    FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)

    message(STATUS "Catch2 is installed")
else ()
    message(STATUS "Catch2 is found, using it")
endif ()
