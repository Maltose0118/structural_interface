add_rules("plugin.vsxmake.autoupdate")
add_rules("mode.debug", "mode.release")

set_project("structural_interface")
set_languages("c++latest")
set_encodings("utf-8")

add_cxxflags("-freflection", {force = true})
add_includedirs("include", {public = true})

add_requires("boost_ut")

target("structural_interface_lib")
    set_kind("headeronly")
    add_headerfiles("include/(**.hpp)")
target_end()

target("structural_interface_tests")
    set_kind("binary")
    add_deps("structural_interface_lib")
    add_packages("boost_ut")
    add_files("tests/*.cpp")
    add_tests("default", {realtime_output = true})
target_end()

target("structural_interface_example_basic")
    set_kind("binary")
    add_deps("structural_interface_lib")
    add_files("examples/basic.cpp")
target_end()
