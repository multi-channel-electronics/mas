from distutils.core import setup, Extension

module1 = Extension('mcelib',
                    sources = ['base.c'],
                    include_dirs=['/usr/mce/include'],
                    library_dirs=['/usr/mce/lib'],
                    libraries=['mcecmd', 'mcedsp', 'maslog', 'config'])

setup (name = 'mcelib',
       version = '1.0',
       description = 'Interface to the UBC MCE electronics.',
       ext_modules = [module1])
