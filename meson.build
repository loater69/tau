
project('engi', 'cpp')

add_project_arguments('/std:c++latest', language: 'cpp')

cmake = import('cmake')

glfw = cmake.subproject('glfw')
glfw_lib = glfw.dependency('glfw')

compiler = meson.get_compiler('cpp')

build_dir = meson.current_build_dir()

include = include_directories('include')

vulkan = dependency('vulkan', method : 'auto')

executable(
    'eng',
    [
        'src/main.cpp',
        'src/instance.cpp',
        'src/pipelines.cpp',
        'src/dom.cpp'
    ],
    dependencies: [
        vulkan,
        glfw_lib,
        declare_dependency(
            include_directories: include,
        ),
    ]
)