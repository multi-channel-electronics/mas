from distutils.core import setup, Extension

module1 = Extension('pymce.mcelib',
                    sources = ['base.c'],
                    include_dirs=['/usr/mce/include'],
                    library_dirs=['/usr/mce/lib'],
                    libraries=['mcecmd', 'mcedsp', 'maslog', 'config'])

setup (name = 'pymce',
       version = '1.0',
       description = 'Interface to the UBC MCE electronics.',
       ext_modules = [module1],
       packages=['pymce'])
