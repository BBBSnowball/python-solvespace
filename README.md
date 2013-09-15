python-solvespace
=================

Geometry constraint solver of SolveSpace as a Python library

The solver has been written by Jonathan Westhues. You can find more information
[on his page][solvespace-lib]. It is part of [SolveSpace][solvespace-cad], which
is a parametric 3d CAD program.

My contributions:

* making it work on Linux (only the library, not the whole program!)
* Python wrapper

I'm going to use the Python library with [SolidPython][solidpython], so I have a
constraint solver for [OpenSCAD][openscad].

[solvespace-cad]: http://solvespace.com/index.pl
[solvespace-lib]: http://solvespace.com/library.pl
[solidpython]:    https://github.com/SolidCode/SolidPython
[openscad]:       http://www.openscad.org/


Build and install
-----------------

The repository contains all the code for SolveSpace. This is necessary because the
library referes to files in its parent directory. The library is in `solvespace/exposed`.
You can run `make` to build the library. You should put the files `slvs.py` and `_slvs.so`
into a Python library directory or next to your application, so Python can find them.

If you are not using Linux, you have to change the Makefile accordingly. You could also
use the build system that the SWIG documentation [suggests][distutils], as this should
work on all platforms.

[distutils]: http://www.swig.org/Doc1.3/SWIGDocumentation.html#Python_nn6

Usage
-----

Have a look at `CDemo.c` and `test.py`. The Python API is almost the same as the C API.
Unfortunately, I couldn't find any documentation for the C API (except for the example
program and the header file `slvs.h`).

I will only explain the differences to the C API. Unfortunately, you cannot directly use
the arrays in `Slvs_System`, so there are a few helpers:

* The constructor reserves memory for 50 items in each of the arrays.
* You can use `add_param`, `add_entity` and `add_constraint` to add items.
* You can use `set_dragged(i, p)` to set `sys.dragged[i] = p`.
* You can access params with `get_param(i)`.
* Furthermore, call-by-reference arguments are mapped to return values, e.g.
  `Slvs_QuaternionU(qw, qx, qy, qz, &x, &y, &z)` (in C) becomes
  `x, y, z = Slvs_QuaternionU(qw, qx, qy, qz)` (in Python).

The wrapper code is merely a hundred lines. If you think that something is missing, please
feel free to add it.

TODO
----

* Create a more pythonic interface or at least strip the `Slvs_` prefix.
* Integrate with SolidPython.
* Improve memory management: fake a private heap (see `solvespace/win32/w32util.cpp`)
* Clean up the code: remove all files that the library doesn't need
* Make it build on all (or most) platforms

License
-------

SolveSpace:<br>
"SolveSpace is distributed under the **GPLv3**, which permits most use in free software but
generally forbids linking the library with proprietary software. If you're interested in
the latter, then SolveSpace is also available for licensing under typical commercial terms;
please contact me for details."
(see [here](http://solvespace.com/library.pl))

You can use my parts of the code under GPLv3, as well. If the author of SolveSpace permits
use under another license (probably commercial), you may use my parts of the code under the
3-clause BSD license. In that case, you should add appropriate copyright headers to all files.
