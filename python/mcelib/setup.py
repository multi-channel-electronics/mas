from distutils.core import setup, Extension

import os
import _default_setup as defaults

includes = defaults.inc_dir.split()

import numpy
includes.append(numpy.get_include())

module1 = Extension('pymce.mcelib',
                    sources = ['base.c'],
                    include_dirs=includes,
                    library_dirs=defaults.lib_dir.split(),
                    libraries=defaults.lib.split())

setup (name = 'pymce',
       version = '1.0',
       description = 'Interface to the UBC MCE electronics.',
       ext_modules = [module1],
       packages=['pymce'])
