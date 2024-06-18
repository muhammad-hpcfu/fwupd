if gusb.found()
cargs = ['-DG_LOG_DOMAIN="FuPluginHpiCfu"']

plugins += {meson.current_source_dir().split('/')[-1]: true}

hpi_cfu_rs = custom_target('fu-hpi-cfu-rs',
  input: 'fu-hpi-cfu.rs',
  output: ['fu-hpi-cfu-struct.c', 'fu-hpi-cfu-struct.h'],
  command: [
    python3,
    join_paths(meson.project_source_root(), 'libfwupdplugin', 'rustgen.py'),
    '@INPUT@', '@OUTPUT0@', '@OUTPUT1@',
  ],
)

plugin_quirks += files('hpi-cfu.quirk')
plugin_builtin_hpi_cfu = static_library('fu_plugin_hpi_cfu',
  hpi_cfu_rs,
  sources: [
    'fu-hpi-cfu-device.c',
    'fu-hpi-cfu-plugin.c',
  ],
  include_directories: plugin_incdirs,
  link_with: plugin_libs,
  c_args: cargs,
  dependencies: plugin_deps,
)
plugin_builtins += plugin_builtin_hpi_cfu

endif



