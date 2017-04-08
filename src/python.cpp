/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "python.h"
#include "force.h"
#include "input.h"
#include "variable.h"
#include "memory.h"
#include "error.h"

#if LMP_PYTHON
#if PY_MAJOR_VERSION == 2
#include "python2.h"
#elif PY_MAJOR_VERSION == 3
#include "python3.h"
#endif
#endif

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

Python::Python(LAMMPS *lmp) : Pointers(lmp)
{
  // implementation of Python interface is only loaded on demand
  // and only if PYTHON package has been installed and compiled into binary
  impl = NULL;
}

/* ---------------------------------------------------------------------- */

Python::~Python()
{
  delete impl;
}

/* ---------------------------------------------------------------------- */

PythonInterface::~PythonInterface()
{
}

/* ---------------------------------------------------------------------- */

void Python::init()
{
#if LMP_PYTHON
#if PY_MAJOR_VERSION == 2
  impl = new Python2(lmp);
#elif PY_MAJOR_VERSION == 3
  impl = new Python3(lmp);
#else
  error->all(FLERR,"Unsupported Python version!");
#endif
#else
  error->all(FLERR,"Python support missing! Compile with PYTHON package installed!");
#endif
}

/* ---------------------------------------------------------------------- */
bool Python::is_enabled() const {
#if LMP_PYTHON
  return true;
#else
  return false;
#endif
}

/* ---------------------------------------------------------------------- */

void Python::command(int narg, char **arg)
{
  if(!impl) init();
  impl->command(narg, arg);
}

/* ------------------------------------------------------------------ */

void Python::invoke_function(int ifunc, char *result)
{
  if(!impl) init();
  impl->invoke_function(ifunc, result);
}

/* ------------------------------------------------------------------ */

int Python::find(char *name)
{
  if(!impl) init();
  return impl->find(name);
}

/* ------------------------------------------------------------------ */

int Python::variable_match(char *name, char *varname, int numeric)
{
  if(!impl) init();
  return impl->variable_match(name, varname, numeric);
}

/* ------------------------------------------------------------------ */

char *Python::long_string(int ifunc)
{
  if(!impl) init();
  return impl->long_string(ifunc);
}
