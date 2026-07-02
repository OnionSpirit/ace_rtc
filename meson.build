project(
    'ace_rtc',
    'cpp',
    default_options: ['cpp_std=c++23'],
    version : '0.9.9'
)

# Declaring lib dir
include_dir = include_directories(['include'])

ace = dependency('ace')

# Declaring interfacial lib
ace_rtc_dep = declare_dependency(
    include_directories: include_dir,
    dependencies: ace,
)
meson.override_dependency('ace_rtc', ace_rtc_dep)

# Tests compilation option
tests_enabled = get_option('tests')

if tests_enabled
    message('Tests requested')
    # Объявляем mimalloc
    mimalloc_dep = dependency('mimalloc', fallback : ['mimalloc', 'mimalloc_dep'], required : true)
    gtest_dep = dependency('gtest', main: true, fallback: ['gtest', 'gtest_dep'], required : true)

    test_sources = files(
        'tests/units.h',
        'tests/tests.cpp',
        'tests/main.cpp',
    )

    # NOTE: Defining compiler-dependent args
    compiler = meson.get_compiler('cpp')
    compiler = compiler.get_argument_syntax()

    compile_args = []
    if compiler == 'gcc'
        compile_args = [
            '-std=c++23',
            #            '-Wno-invalid-offsetof',
            #            '-Mmodules',
            #            '-fconcepts-diagnostics-depth=2'
        ]
    elif compiler == 'clang'
        compile_args = [
            '-std=c++23',
            '-flto',
            #            '-fexperimental-library',
            #            '-Wno-invalid-offsetof'
        ]
    endif

    message('Arguments for @0@ compiler: '.format(compiler), compile_args)

    test_executable = executable (
        'ace_tests',
        sources: test_sources,
        cpp_args: compile_args,
        dependencies: [
            gtest_dep,
            ace_rtc_dep,
            mimalloc_dep
        ],
    )

    python = import('python').find_installation('python3')
    tests = run_command(
        python, 'discover_tests.py', test_executable.full_path(),
        check: false
    ).stdout().strip().split()

    message('Found tests: ', tests)

    foreach test_name : tests
        message('applying test - ', test_name)
        test(
            test_name,
            test_executable,
            args: [
                '--gtest_filter= "@0@"'.format(test_name),
                '--gtest_print_time=0',
                '--gtest_color=yes',
                '--gtest_catch_exceptions=0',
                '--gtest_break_on_failure=1'
            ]
        )
    endforeach

endif
