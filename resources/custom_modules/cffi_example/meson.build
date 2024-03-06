project(
    'waybar_cffi_example', 'c',
    version: '0.1.0',
    license: 'MIT',
)

shared_library('wb_cffi_example',
    ['main.c'],
    dependencies: [
        dependency('gtk+-3.0', version : ['>=3.22.0'])
    ],
    name_prefix: ''
)
