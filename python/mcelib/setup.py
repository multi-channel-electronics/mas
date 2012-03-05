from distutils.core import setup, Extension

module1 = Extension('mce',
                    sources = ['pymce.c'])

setup (name = 'pymce',
       version = '1.0',
       description = 'Interface to the UBC MCE electronics.',
       ext_modules = [module1])
