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

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

Python::Python(LAMMPS *lmp) : Pointers(lmp)
{
}

/* ---------------------------------------------------------------------- */

Python::~Python()
{
}

/* ---------------------------------------------------------------------- */

PythonDummy::PythonDummy(LAMMPS *lmp) : Python(lmp)
{
  python_exists = false;
}

/* ---------------------------------------------------------------------- */

PythonDummy::~PythonDummy()
{
}

/* ---------------------------------------------------------------------- */

void PythonDummy::command(int narg, char **arg)
{
}

/* ------------------------------------------------------------------ */

void PythonDummy::invoke_function(int ifunc, char *result)
{
}

/* ------------------------------------------------------------------ */

int PythonDummy::find(char *name)
{
  return 0;
}

/* ------------------------------------------------------------------ */

int PythonDummy::variable_match(char *name, char *varname, int numeric)
{
  return 0;
}

/* ------------------------------------------------------------------ */

char *PythonDummy::long_string(int ifunc)
{
  return NULL;
}
