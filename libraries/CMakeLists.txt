add_subdirectory(razormount)
add_subdirectory(razorqt)
add_subdirectory(razorqxt)
add_subdirectory(qtxdg)
add_subdirectory(qtservice)

add_dependencies(razorqt qtxdg razorqxt razormount)

