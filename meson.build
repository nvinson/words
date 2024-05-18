project(
  'words',
  'c',
  version:'1.0.0',
  default_options:['b_lto=true', 'b_lto_mode=thin']
)

word_list_dep = declare_dependency(extra_files:['resources/words.txt'])
install_data('resources/words.txt', install_dir: get_option('datadir') / 'words')

executable('words',
  'src/words.c',
  install: true,
  gnu_symbol_visibility: 'hidden')
