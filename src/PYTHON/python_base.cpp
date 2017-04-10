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

#include "python_base.h"
#include "force.h"
#include "input.h"
#include "variable.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;
using namespace std;

// Wrap API changes between Python 2 and 3 using macros
#if PY_MAJOR_VERSION == 2
#define PY_INT_FROM_LONG(X) PyInt_FromLong(X)
#define PY_INT_AS_LONG(X) PyInt_AsLong(X)
#define PY_STRING_FROM_STRING(X) PyString_FromString(X)
#define PY_VOID_POINTER(X) PyCObject_FromVoidPtr((void *) X, NULL)
#define PY_STRING_AS_STRING(X) PyString_AsString(X)

#elif PY_MAJOR_VERSION == 3
#define PY_INT_FROM_LONG(X) PyLong_FromLong(X)
#define PY_INT_AS_LONG(X) PyLong_AsLong(X)
#define PY_STRING_FROM_STRING(X) PyUnicode_FromString(X)
#define PY_VOID_POINTER(X) PyCapsule_New((void *) X, NULL, NULL)
#define PY_STRING_AS_STRING(X) PyUnicode_AsUTF8(X)
#endif

/* ---------------------------------------------------------------------- */

PythonBase::PythonBase(LAMMPS *lmp) : Pointers(lmp)
{
  pyMain = NULL;

  // one-time initialization of Python interpreter
  // pymain stores pointer to main module

  external_interpreter = Py_IsInitialized();

  Py_Initialize();
  PyEval_InitThreads();
  PyGILState_STATE gstate = PyGILState_Ensure();

  pyMain = PyImport_AddModule("__main__");
  if (!pyMain) error->all(FLERR,"Could not initialize embedded Python");
}

/* ---------------------------------------------------------------------- */

PythonBase::~PythonBase()
{
  PyGILState_STATE gstate = PyGILState_Ensure();

  for(map<string, PyFunc*>::iterator it = functions.begin(); it != functions.end(); ++it) {
    delete it->second;
    it->second = NULL;
  }

  // shutdown Python interpreter

  if (pyMain && !external_interpreter) {
    Py_Finalize();
  }
  else {
    PyGILState_Release(gstate);
  }
}

/* ---------------------------------------------------------------------- */

void PythonBase::command(int narg, char **arg)
{
  if (narg < 2) error->all(FLERR,"Invalid python command");

  // if invoke is only keyword, invoke the previously defined function

  if (narg == 2 && strcmp(arg[1],"invoke") == 0) {
    PyFunc * pfunc = find(arg[0]);
    if (!pfunc) error->all(FLERR,"Python invoke of undefined function");

    char *str = NULL;
    if (noutput) {
      str = input->variable->pythonstyle((char*)pfunc->ovarname.c_str(),
                                         (char*)pfunc->name.c_str());
      if (!str)
        error->all(FLERR,"Python variable does not match Python function");
    }

    invoke_function(pfunc, str);
    return;
  }

  // parse optional args, invoke is not allowed in this mode

  ninput = noutput = 0;
  istr = NULL;
  ostr = NULL;
  format = NULL;
  char *pyfile = NULL;
  char *herestr = NULL;
  int existflag = 0;

  int iarg = 1;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"input") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      ninput = force->inumeric(FLERR,arg[iarg+1]);
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
      int n = strlen(arg[iarg+1]) + 1;
      format = new char[n];
      strcpy(format,arg[iarg+1]);
      iarg += 2;
    } else if (strcmp(arg[iarg],"length") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      length_longstr = force->inumeric(FLERR,arg[iarg+1]);
      if (length_longstr <= 0) error->all(FLERR,"Invalid python command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"file") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid python command");
      delete[] pyfile;
      int n = strlen(arg[iarg+1]) + 1;
      pyfile = new char[n];
      strcpy(pyfile,arg[iarg+1]);
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

  PyFunc * pfunc = create_entry(arg[0]);

  PyGILState_STATE gstate = PyGILState_Ensure();

  // send Python code to Python interpreter
  // file: read the file via PyRun_SimpleFile()
  // here: process the here string directly
  // exist: do nothing, assume code has already been run

  if (pyfile) {
    FILE *fp = fopen(pyfile,"r");

    if (fp == NULL) {
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

  // pyFunc = function object for requested function

  PyObject *pyModule = (PyObject *) pyMain;
  PyObject *pyFunc = PyObject_GetAttrString(pyModule,pfunc->name.c_str());

  if (!pyFunc) {
    PyGILState_Release(gstate);
    error->all(FLERR,"Could not find Python function");
  }

  if (!PyCallable_Check(pyFunc)) {
    PyGILState_Release(gstate);
    error->all(FLERR,"Python function is not callable");
  }

  pfunc->pyFunc = pyFunc;

  // clean-up input storage

  delete [] istr;
  delete [] format;
  delete [] pyfile;
  PyGILState_Release(gstate);
}

/* ------------------------------------------------------------------ */

void PythonBase::invoke_function(PyFunc * pfunc, char *result)
{
  PyGILState_STATE gstate = PyGILState_Ensure();

  PyObject *pyFunc  = pfunc->pyFunc;
  PyObject *pyValue = NULL;

  // create Python tuple of input arguments

  int ninput = pfunc->ninput;
  PyObject *pArgs = PyTuple_New(ninput);

  if (!pArgs) {
    PyGILState_Release(gstate);
    error->all(FLERR,"Could not create Python function arguments");
  }

  for (int i = 0; i < ninput; i++) {
    int itype = pfunc->itype[i];
    bool is_variable = pfunc->ivarflag[i];
    char * str = NULL;

    if(is_variable)
    {
      str = input->variable->retrieve((char*)pfunc->svalue[i].c_str());

      if (!str) {
        PyGILState_Release(gstate);
        error->all(FLERR,"Could not evaluate Python function input variable");
      }
    }

    switch(itype) {
    case INT:
      if (is_variable) {
        pyValue = PY_INT_FROM_LONG(atoi(str));
      } else {
        pyValue = PY_INT_FROM_LONG(pfunc->ivalue[i]);
      }
      break;

    case DOUBLE:
      if (is_variable) {
        pyValue = PyFloat_FromDouble(atof(str));
      } else {
        pyValue = PyFloat_FromDouble(pfunc->dvalue[i]);
      }
      break;

    case STRING:
      if (is_variable) {
        pyValue = PY_STRING_FROM_STRING(str);
      } else {
        pyValue = PY_STRING_FROM_STRING(pfunc->svalue[i].c_str());
      }
      break;

    case PTR:
      pyValue = PY_VOID_POINTER(lmp);
      break;
    }
    PyTuple_SetItem(pArgs,i,pyValue);
  }

  // call the Python function
  // error check with one() since only some procs may fail

  pyValue = PyObject_CallObject(pyFunc,pArgs);

  if (!pyValue) {
    PyGILState_Release(gstate);
    error->one(FLERR,"Python function evaluation failed");
  }

  Py_DECREF(pArgs);

  // function returned a value
  // assign it to result string stored by python-style variable
  // or if user specified a length, assign it to longstr

  if (pfunc->noutput) {
    int otype = pfunc->otype;
    switch(otype) {
    case INT:
      sprintf(result,"%ld",PY_INT_AS_LONG(pyValue));
      break;

    case DOUBLE:
      sprintf(result,"%.15g",PyFloat_AsDouble(pyValue));
      break;

    case STRING:
    case LONG_STRING:
      char *pystr = PY_STRING_AS_STRING(pyValue);
    
      if (otype == LONG_STRING) {
        pfunc->longstr = string(pystr);
      } else {
        strncpy(result,pystr,VALUELENGTH-1);
      }
      break;
    }
    Py_DECREF(pyValue);
  }

  PyGILState_Release(gstate);
}

/* ------------------------------------------------------------------ */

PyFunc * PythonBase::find(char *name)
{
  map<string, PyFunc*>::iterator it = functions.find(name);

  if(it != functions.end()) {
    return it->second;
  }

  return NULL;
}

/* ------------------------------------------------------------------ */

PyFunc * PythonBase::variable_match(char *name, char *varname, int numeric)
{
  PyFunc * pfunc = find(name);
  if (!pfunc) return NULL;
  if (pfunc->noutput == 0) return NULL;
  if (strcmp(pfunc->ovarname.c_str(), varname) != 0) return NULL;
  if (numeric && (pfunc->otype == STRING || pfunc->otype == LONG_STRING)) return NULL;
  return pfunc;
}

/* ------------------------------------------------------------------ */

PyFunc * PythonBase::create_entry(char *name)
{
  PyFunc * pfunc = new PyFunc(name, ninput, noutput);

  map<string, PyFunc*>::iterator it = functions.find(name);

  if(it != functions.end())
  {
    delete it->second;
    it->second = pfunc;
  } else {
    functions[name] = pfunc;
  }

  if (!format && ninput+noutput)
    error->all(FLERR,"Invalid python command");
  else if (format && strlen(format) != ninput+noutput)
    error->all(FLERR,"Invalid python command");

  // process inputs as values or variables

  for (int i = 0; i < ninput; i++) {
    char type = format[i];
    bool is_variable = (strstr(istr[i],"v_") == istr[i]);

    if(is_variable) {
      pfunc->ivarflag[i] = 1;
      pfunc->svalue[i] = string(&istr[i][2]);
    } else {
      pfunc->ivarflag[i] = 0;
    }

    switch(type) {
    case 'i':
      pfunc->itype[i] = INT;
      if (!is_variable)
      {
        pfunc->ivalue[i] = force->inumeric(FLERR,istr[i]);
      }
      break;

    case 'f':
      pfunc->itype[i] = DOUBLE;
      if (!is_variable)
      {
        pfunc->dvalue[i] = force->numeric(FLERR,istr[i]);
      }
      break;

    case 's':
      pfunc->itype[i] = STRING;
      if (!is_variable)
      {
        pfunc->svalue[i] = istr[i];
      }
      break;

    case 'p':
      pfunc->itype[i] = PTR;
      if (is_variable || (strcmp(istr[i],"SELF") != 0))
        error->all(FLERR,"Invalid python command");
      break;

    default:
      error->all(FLERR,"Invalid python command");
    }
  }

  // process output as value or variable

  if (!noutput) return pfunc;

  char type = format[ninput];
  switch(type){
  case 'i':
    pfunc->otype = INT;
    break;

  case 'f':
    pfunc->otype = DOUBLE;
    break;

  case 's':
    if(length_longstr) {
      pfunc->otype = LONG_STRING;
    } else {
      pfunc->otype = STRING;
    }
    break;
  
  default:
    error->all(FLERR,"Invalid python command");
  }

  if (strstr(ostr,"v_") != ostr) error->all(FLERR,"Invalid python command");
  
  pfunc->ovarname = string(&ostr[2]);

  return pfunc;
}

/* ------------------------------------------------------------------ */

PyFunc::PyFunc(const string & name, int ninput, int noutput) :
  name(name), ninput(ninput), noutput(noutput)
{
  itype = new DataType[ninput];
  ivarflag = new int[ninput];
  ivalue = new int[ninput];
  dvalue = new double[ninput];
  svalue = new string[ninput];

  for(int i = 0; i < ninput; ++i) {
    ivarflag[i] = 0;
  }
  
  pyFunc = NULL;
}

PyFunc::~PyFunc()
{
  delete [] itype;
  delete [] ivarflag;
  delete [] ivalue;
  delete [] dvalue;
  delete [] svalue;
  Py_XDECREF(pyFunc);
}

bool PyFunc::returns_long_string()
{
  return otype == LONG_STRING;
}

const char* PyFunc::get_long_string()
{
  return longstr.c_str();
}
