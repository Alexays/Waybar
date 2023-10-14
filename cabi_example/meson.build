project(
    'waybar_cabi_c_example', 'c',
    version: '0.1.0',
    license: 'MIT',
)

compiler = meson.get_compiler('c')


shared_library('waybar_cabi_c_example',
    [
        'main.c',
    ],
    dependencies: [
    	dependency('gtk+-3.0', version : ['>=3.22.0'])
    ]
    # include_directories: include_directories(['../../../include']),
    # dependencies: [],
    # install: true,
    # install_dir: 'plugins',
)
