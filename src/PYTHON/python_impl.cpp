/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://lammps.sandia.gov/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing authors: Richard Berger and Axel Kohlmeyer (Temple U)
------------------------------------------------------------------------- */

#include "python_impl.h"

#include "error.h"
#include "input.h"
#include "memory.h"
#include "python_compat.h"
#include "variable.h"
#include "utils.h"

#include <cstring>
#include <Python.h>  // IWYU pragma: export

#ifdef MLIAP_PYTHON
#include "mliap_model_python.h"
// The above should somehow really be included in the next file.
// We could get around this with cython --capi-reexport-cincludes
// However, that exposes -too many- headers.
#include "mliap_model_python_couple.h"
#endif

using namespace LAMMPS_NS;

enum{NONE,INT,DOUBLE,STRING,PTR};

#define VALUELENGTH 64               // also in variable.cpp


/* ---------------------------------------------------------------------- */

PythonImpl::PythonImpl(LAMMPS *lmp) : Pointers(lmp)
{
  ninput = noutput = 0;
  istr = nullptr;
  ostr = nullptr;
  format = nullptr;
  length_longstr = 0;

  // check for PYTHONUNBUFFERED environment variable
  const char * PYTHONUNBUFFERED = getenv("PYTHONUNBUFFERED");

  if (PYTHONUNBUFFERED != nullptr && strcmp(PYTHONUNBUFFERED, "1") == 0) {
    // Python Global configuration variable
    // Force the stdout and stderr streams to be unbuffered.
    Py_UnbufferedStdioFlag = 1;
  }

  // one-time initialization of Python interpreter
  // pyMain stores pointer to main module
  external_interpreter = Py_IsInitialized();

#ifdef MLIAP_PYTHON
  // Inform python intialization scheme of the mliappy module.
  // This -must- happen before python is initialized.
  int err = PyImport_AppendInittab("mliap_model_python_couple", PyInit_mliap_model_python_couple);
  if (err) error->all(FLERR,"Could not register MLIAPPY embedded python module.");
#endif

  Py_Initialize();

  // only needed for Python 2.x and Python 3 < 3.7
  // With Python 3.7 this function is now called by Py_Initialize()
  // Deprecated since version 3.9, will be removed in version 3.11
#if PY_MAJOR_VERSION < 3 || PY_MINOR_VERSION < 7
  if (!PyEval_ThreadsInitialized()) {
    PyEval_InitThreads();
  }
#endif

  PyGILState_STATE gstate = PyGILState_Ensure();

  PyObject *pModule = PyImport_AddModule("__main__");
  if (!pModule) error->all(FLERR,"Could not initialize embedded Python");

  pyMain = (void *) pModule;
  PyGILState_Release(gstate);
}

/* ---------------------------------------------------------------------- */

PythonImpl::~PythonImpl()
{
  if (pyMain) {
    // clean up
    PyGILState_STATE gstate = PyGILState_Ensure();

    for (int i = 0; i < pfuncs.size(); i++) {
      PyObject *pFunc = (PyObject *) pfuncs[i]->pFunc;
      pfuncs[i]->pFunc = nullptr;
      Py_XDECREF(pFunc);
    }

    // shutdown Python interpreter

    if (!external_interpreter) {
      Py_Finalize();
    }
    else {
      PyGILState_Release(gstate);
    }
  }
}

/* ---------------------------------------------------------------------- */

void PythonImpl::command(int narg, char **arg)
{
  if (narg < 2) error->all(FLERR,"Invalid python command");

  // if invoke is only keyword, invoke the previously defined function

  if (narg == 2 && strcmp(arg[1],"invoke") == 0) {
    int ifunc = find(arg[0]);
    if (ifunc < 0) error->all(FLERR,"Python invoke of undefined function");

    char *str = nullptr;
    if (noutput) {
      str = input->variable->pythonstyle(pfuncs[ifunc]->ovarname,
                                         pfuncs[ifunc]->name);
      if (!str)
        error->all(FLERR,"Python variable does not match Python function");
    }

    invoke_function(ifunc,str);
    return;
  }

  // if source is only keyword, execute the python code

  if (narg == 3 && strcmp(arg[1],"source") == 0) {
    int err;

    FILE *fp = fopen(arg[2],"r");
    if (fp == nullptr)
      err = execute_string(arg[2]);
    else
      err = execute_file(arg[2]);

    if (fp) fclose(fp);
    if (err) error->all(FLERR,"Could not process Python source command");

    return;
  }

  // parse optional args, invoke is not allowed in this mode

  ninput = noutput = 0;
  istr = nullptr;
  ostr = nullptr;
  format = nullptr;
  length_longstr = 0;
  char *pyfile = nullptr;
  char *herestr = nullptr;
  int existflag = 0;

  int iarg = 1;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"input") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      ninput = utils::inumeric(FLERR,arg[iarg+1],false,lmp);
      if (ninput < 0) error->all(FLERR,"Invalid python command");
      iarg += 2;
      istr = new char*[ninput];
      if (iarg+ninput > narg) error->all(FLERR,"Invalid python command");
      for (int i = 0; i < ninput; i++) istr[i] = arg[iarg+i];
      iarg += ninput;
    } else if (strcmp(arg[iarg],"return") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      noutput = 1;
      ostr = arg[iarg+1];
      iarg += 2;
    } else if (strcmp(arg[iarg],"format") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      format = utils::strdup(arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"length") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      length_longstr = utils::inumeric(FLERR,arg[iarg+1],false,lmp);
      if (length_longstr <= 0) error->all(FLERR,"Invalid python command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"file") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      delete[] pyfile;
      pyfile = utils::strdup(arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"here") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      herestr = arg[iarg+1];
      iarg += 2;
    } else if (strcmp(arg[iarg],"exists") == 0) {
      existflag = 1;
      iarg++;
    } else error->all(FLERR,"Invalid python command");
  }

  if (pyfile && herestr) error->all(FLERR,"Invalid python command");
  if (pyfile && existflag) error->all(FLERR,"Invalid python command");
  if (herestr && existflag) error->all(FLERR,"Invalid python command");

  // create or overwrite entry in pfuncs vector with name = arg[0]

  int ifunc = create_entry(arg[0]);

  PyGILState_STATE gstate = PyGILState_Ensure();

  // send Python code to Python interpreter
  // file: read the file via PyRun_SimpleFile()
  // here: process the here string directly
  // exist: do nothing, assume code has already been run

  if (pyfile) {
    FILE *fp = fopen(pyfile,"r");

    if (fp == nullptr) {
      PyGILState_Release(gstate);
      error->all(FLERR,"Could not open Python file");
    }

    int err = PyRun_SimpleFile(fp,pyfile);

    if (err) {
      PyGILState_Release(gstate);
      error->all(FLERR,"Could not process Python file");
    }

    fclose(fp);
  } else if (herestr) {
    int err = PyRun_SimpleString(herestr);

    if (err) {
      PyGILState_Release(gstate);
      error->all(FLERR,"Could not process Python string");
    }
  }

  // pFunc = function object for requested function

  PyObject *pModule = (PyObject *) pyMain;
  PyObject *pFunc = PyObject_GetAttrString(pModule,pfuncs[ifunc]->name);

  if (!pFunc) {
    PyGILState_Release(gstate);
    error->all(FLERR,fmt::format("Could not find Python function {} {}",
                                 ifunc, pfuncs[ifunc]->name));
  }

  if (!PyCallable_Check(pFunc)) {
    PyGILState_Release(gstate);
    error->all(FLERR,fmt::format("Python function {} is not callable",
                                 pfuncs[ifunc]->name));
  }

  pfuncs[ifunc]->pFunc = (void *) pFunc;

  // clean-up input storage

  delete [] istr;
  delete [] format;
  delete [] pyfile;
  PyGILState_Release(gstate);
}

/* ------------------------------------------------------------------ */

void PythonImpl::invoke_function(int ifunc, char *result)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  PyObject *pValue;
  char *str;

  PyFunc & pfunc = *pfuncs[ifunc];
  PyObject *pFunc = (PyObject *) pfunc.pFunc;

  // create Python tuple of input arguments

  int ninput = pfunc.ninput;
  PyObject *pArgs = PyTuple_New(ninput);

  if (!pArgs) {
    PyGILState_Release(gstate);
    error->all(FLERR,"Could not create Python function arguments");
  }

  for (int i = 0; i < ninput; i++) {
    int itype = pfunc.itype[i];
    if (itype == INT) {
      if (pfunc.ivarflag[i]) {
        str = input->variable->retrieve(pfunc.svalue[i]);

        if (!str) {
          PyGILState_Release(gstate);
          error->all(FLERR,"Could not evaluate Python function input variable");
        }

        pValue = PY_INT_FROM_LONG(atoi(str));
      } else {
        pValue = PY_INT_FROM_LONG(pfunc.ivalue[i]);
      }
    } else if (itype == DOUBLE) {
      if (pfunc.ivarflag[i]) {
        str = input->variable->retrieve(pfunc.svalue[i]);

        if (!str) {
          PyGILState_Release(gstate);
          error->all(FLERR,"Could not evaluate Python function input variable");
        }

        pValue = PyFloat_FromDouble(atof(str));
      } else {
        pValue = PyFloat_FromDouble(pfunc.dvalue[i]);
      }
    } else if (itype == STRING) {
      if (pfunc.ivarflag[i]) {
        str = input->variable->retrieve(pfunc.svalue[i]);
        if (!str) {
          PyGILState_Release(gstate);
          error->all(FLERR,"Could not evaluate Python function input variable");
        }

        pValue = PY_STRING_FROM_STRING(str);
      } else {
        pValue = PY_STRING_FROM_STRING(pfunc.svalue[i]);
      }
    } else if (itype == PTR) {
      pValue = PY_VOID_POINTER(lmp);
    } else {
      PyGILState_Release(gstate);
      error->all(FLERR,"Unsupported variable type");
    }
    PyTuple_SetItem(pArgs,i,pValue);
  }

  // call the Python function
  // error check with one() since only some procs may fail

  pValue = PyObject_CallObject(pFunc,pArgs);

  if (!pValue) {
    PyErr_Print();
    PyGILState_Release(gstate);
    error->one(FLERR,"Python function evaluation failed");
  }

  Py_DECREF(pArgs);

  // function returned a value
  // assign it to result string stored by python-style variable
  // or if user specified a length, assign it to longstr

  if (pfunc.noutput) {
    int otype = pfunc.otype;
    if (otype == INT) {
      sprintf(result,"%ld",PY_INT_AS_LONG(pValue));
    } else if (otype == DOUBLE) {
      sprintf(result,"%.15g",PyFloat_AsDouble(pValue));
    } else if (otype == STRING) {
      const char *pystr = PY_STRING_AS_STRING(pValue);
      if (pfunc.longstr)
        strncpy(pfunc.longstr,pystr,pfunc.length_longstr);
      else strncpy(result,pystr,VALUELENGTH-1);
    }
    Py_DECREF(pValue);
  }

  PyGILState_Release(gstate);
}

/* ------------------------------------------------------------------ */

int PythonImpl::find(const char *name)
{
  for (int i = 0; i < pfuncs.size(); i++)
    if (strcmp(name,pfuncs[i]->name) == 0) return i;
  return -1;
}

/* ------------------------------------------------------------------ */

int PythonImpl::variable_match(const char *name, const char *varname,
                               int numeric)
{
  int ifunc = find(name);
  if (ifunc < 0) return -1;
  PyFunc & pfunc = *pfuncs[ifunc];
  if (pfunc.noutput == 0) return -1;
  if (strcmp(pfunc.ovarname,varname) != 0) return -1;
  if (numeric && pfunc.otype == STRING) return -1;
  return ifunc;
}

/* ------------------------------------------------------------------ */

char *PythonImpl::long_string(int ifunc)
{
  return pfuncs[ifunc]->longstr;
}

/* ------------------------------------------------------------------ */

int PythonImpl::create_entry(char *name)
{
  // ifunc = index to entry by name in pfuncs vector, can be old or new
  // free old vectors if overwriting old pfunc

  int ifunc = find(name);

  if (ifunc < 0) {
    ifunc = pfuncs.size();
    pfuncs.emplace_back(std::make_shared<PyFunc>(name, ninput, noutput));
  } else {
    pfuncs[ifunc] = std::make_shared<PyFunc>(name, ninput, noutput);
  }

  if (!format && ninput+noutput)
    error->all(FLERR,"Invalid python command");
  else if (format && ((int) strlen(format) != ninput+noutput))
    error->all(FLERR,"Invalid python command");

  // process inputs as values or variables
  PyFunc & pfunc = *pfuncs[ifunc];

  for (int i = 0; i < ninput; i++) {
    pfunc.svalue[i] = nullptr;
    char type = format[i];
    if (type == 'i') {
      pfunc.itype[i] = INT;
      if (utils::strmatch(istr[i],"^v_")) {
        pfunc.ivarflag[i] = 1;
        pfunc.svalue[i] = utils::strdup(istr[i]+2);
      } else {
        pfunc.ivarflag[i] = 0;
        pfunc.ivalue[i] = utils::inumeric(FLERR,istr[i],false,lmp);
      }
    } else if (type == 'f') {
      pfunc.itype[i] = DOUBLE;
      if (utils::strmatch(istr[i],"^v_")) {
        pfunc.ivarflag[i] = 1;
        pfunc.svalue[i] = utils::strdup(istr[i]+2);
      } else {
        pfunc.ivarflag[i] = 0;
        pfunc.dvalue[i] = utils::numeric(FLERR,istr[i],false,lmp);
      }
    } else if (type == 's') {
      pfunc.itype[i] = STRING;
      if (utils::strmatch(istr[i],"^v_")) {
        pfunc.ivarflag[i] = 1;
        pfunc.svalue[i] = utils::strdup(istr[i]+2);
      } else {
        pfunc.ivarflag[i] = 0;
        pfunc.svalue[i] = utils::strdup(istr[i]);
      }
    } else if (type == 'p') {
      pfunc.ivarflag[i] = 0;
      pfunc.itype[i] = PTR;
      if (strcmp(istr[i],"SELF") != 0)
        error->all(FLERR,"Invalid python command");

    } else error->all(FLERR,"Invalid python command");
  }

  // process output as value or variable

  if (!noutput) return ifunc;

  char type = format[ninput];
  if (type == 'i') pfunc.otype = INT;
  else if (type == 'f') pfunc.otype = DOUBLE;
  else if (type == 's') pfunc.otype = STRING;
  else error->all(FLERR,"Invalid python command");

  if (length_longstr) {
    if (pfunc.otype != STRING)
      error->all(FLERR,"Python command length keyword "
                 "cannot be used unless output is a string");
    pfunc.length_longstr = length_longstr;
    pfunc.longstr = new char[length_longstr+1];
    pfunc.longstr[length_longstr] = '\0';
  }

  if (strstr(ostr,"v_") != ostr) error->all(FLERR,"Invalid python command");
  pfunc.ovarname = utils::strdup(ostr+2);

  return ifunc;
}

/* ---------------------------------------------------------------------- */

int PythonImpl::execute_string(char *cmd)
{
  PyGILState_STATE gstate = PyGILState_Ensure();
  int err = PyRun_SimpleString(cmd);
  PyGILState_Release(gstate);

  return err;
}

/* ---------------------------------------------------------------------- */

int PythonImpl::execute_file(char *fname)
{
  FILE *fp = fopen(fname,"r");
  if (fp == nullptr) return -1;

  PyGILState_STATE gstate = PyGILState_Ensure();
  int err = PyRun_SimpleFile(fp,fname);
  PyGILState_Release(gstate);

  if (fp) fclose(fp);
  return err;
}

/* ------------------------------------------------------------------ */

PythonImpl::PyFunc::PyFunc(const char * name, int ninput, int noutput)
  : name(utils::strdup(name)), ninput(ninput), noutput(noutput)
{
  itype = new int[ninput];
  ivarflag = new int[ninput];
  ivalue = new int[ninput];
  dvalue = new double[ninput];
  svalue = new char*[ninput];
  ovarname = nullptr;
  longstr = nullptr;
  length_longstr = 0;
}

/* ------------------------------------------------------------------ */

PythonImpl::PyFunc::~PyFunc()
{
  delete [] name;
  delete [] itype;
  delete [] ivarflag;
  delete [] ivalue;
  delete [] dvalue;
  if (svalue) {
    for (int j = 0; j < ninput; j++)
      delete [] svalue[j];
  }
  delete [] svalue;
  delete [] ovarname;
  delete [] longstr;
}

/* ------------------------------------------------------------------ */

bool PythonImpl::has_minimum_version(int major, int minor)
{
    return (PY_MAJOR_VERSION == major && PY_MINOR_VERSION >= minor) || (PY_MAJOR_VERSION > major);
}
