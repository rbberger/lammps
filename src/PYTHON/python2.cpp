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

#include <Python.h>

#if PY_MAJOR_VERSION == 2

#include "python2.h"
#include "force.h"
#include "input.h"
#include "variable.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

Python2::Python2(LAMMPS *lmp) : PythonBase(lmp)
{
}

/* ---------------------------------------------------------------------- */

Python2::~Python2()
{
}

/* ------------------------------------------------------------------ */

void Python2::invoke_function(int ifunc, char *result)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject *pValue;
  char *str;

  PyObject *pFunc = (PyObject *) pfuncs[ifunc].pFunc;

  // create Python tuple of input arguments

  int ninput = pfuncs[ifunc].ninput;
  PyObject *pArgs = PyTuple_New(ninput);

  if (!pArgs) {
    PyGILState_Release(gstate);
    error->all(FLERR,"Could not create Python function arguments");
  }

  for (int i = 0; i < ninput; i++) {
    int itype = pfuncs[ifunc].itype[i];
    if (itype == INT) {
      if (pfuncs[ifunc].ivarflag[i]) {
        str = input->variable->retrieve(pfuncs[ifunc].svalue[i]);

        if (!str) {
          PyGILState_Release(gstate);
          error->all(FLERR,"Could not evaluate Python function input variable");
        }

        pValue = PyInt_FromLong(atoi(str));
      } else pValue = PyInt_FromLong(pfuncs[ifunc].ivalue[i]);
    } else if (itype == DOUBLE) {
      if (pfuncs[ifunc].ivarflag[i]) {
        str = input->variable->retrieve(pfuncs[ifunc].svalue[i]);

        if (!str) {
          PyGILState_Release(gstate);
          error->all(FLERR,"Could not evaluate Python function input variable");
        }

        pValue = PyFloat_FromDouble(atof(str));
      } else pValue = PyFloat_FromDouble(pfuncs[ifunc].dvalue[i]);
    } else if (itype == STRING) {
      if (pfuncs[ifunc].ivarflag[i]) {
        str = input->variable->retrieve(pfuncs[ifunc].svalue[i]);
        if (!str) {
          PyGILState_Release(gstate);
          error->all(FLERR,"Could not evaluate Python function input variable");
        }
        pValue = PyString_FromString(str);
      } else pValue = PyString_FromString(pfuncs[ifunc].svalue[i]);
    } else if (itype == PTR) {
      pValue = PyCObject_FromVoidPtr((void *) lmp,NULL);
    }
    PyTuple_SetItem(pArgs,i,pValue);
  }

  // call the Python function
  // error check with one() since only some procs may fail

  pValue = PyObject_CallObject(pFunc,pArgs);

  if (!pValue) {
    PyGILState_Release(gstate);
    error->one(FLERR,"Python function evaluation failed");
  }

  Py_DECREF(pArgs);

  // function returned a value
  // assign it to result string stored by python-style variable
  // or if user specified a length, assign it to longstr

  if (pfuncs[ifunc].noutput) {
    int otype = pfuncs[ifunc].otype;
    if (otype == INT) {
      sprintf(result,"%ld",PyInt_AsLong(pValue));
    } else if (otype == DOUBLE) {
      sprintf(result,"%.15g",PyFloat_AsDouble(pValue));
    } else if (otype == STRING) {
      char *pystr = PyString_AsString(pValue);
      if (pfuncs[ifunc].longstr) 
        strncpy(pfuncs[ifunc].longstr,pystr,pfuncs[ifunc].length_longstr);
      else strncpy(result,pystr,VALUELENGTH-1);
    }
    Py_DECREF(pValue);
  }

  PyGILState_Release(gstate);
}

#endif
