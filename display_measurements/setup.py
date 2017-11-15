from Cython.Distutils import build_ext
from distutils.extension import Extension
from distutils.core import setup
import numpy

setup(
    name='mine', description='Nothing',
    ext_modules=[Extension('quantiser', ['quantiser.pyx'],
                             include_dirs=[numpy.get_include()])],
    cmdclass = {'build_ext':build_ext},
    extra_compile_args=["-O3"]
)
