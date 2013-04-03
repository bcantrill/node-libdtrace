{
  'targets': [
    {
      'target_name': 'libdtrace',
      'cflags_cc': ['-fexceptions'],
      'ldflags': ['-ldtrace'],
      'sources': [ 
        'libdtrace.cc'
      ], 
      'libraries': ['-ldtrace'],
      'xcode_settings': {
          'OTHER_CPLUSPLUSFLAGS': [
              '-fexceptions',
              '-Wunused-variable',
          ],
      }
    },
  ]
}
