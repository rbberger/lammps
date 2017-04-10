/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifndef LMP_PYTHON_BASE_H
#define LMP_PYTHON_BASE_H

#include <Python.h>
#include "python.h"
#include <map>
#include <string>

namespace LAMMPS_NS {

enum DataType {NONE,INT,DOUBLE,STRING,PTR,LONG_STRING};

#define VALUELENGTH 64               // also in variable.cpp

struct PyFunc {
  PyFunc(const std::string & name, int ninput, int noutput);
  ~PyFunc();

  std::string name;
  int ninput;
  int noutput;

  DataType *itype;
  int *ivarflag;
  int *ivalue;
  double *dvalue;
  std::string * svalue;

  DataType otype;
  std::string ovarname;
  std::string longstr;

  PyObject * pyFunc;
  
  bool returns_long_string();
  const char* get_long_string();
};

class PythonBase : public PythonInterface, protected Pointers {
 public:
  PythonBase(class LAMMPS *);
  virtual ~PythonBase();
  void command(int, char **);
  void invoke_function(PyFunc*, char *);
  PyFunc* find(char *);
  PyFunc* variable_match(char *, char *, int);

 protected:
  bool external_interpreter;
  int ninput,noutput,length_longstr;
  char **istr;
  char *ostr,*format;
  PyObject *pyMain;

  std::map<std::string, PyFunc*> functions;

  PyFunc* create_entry(char *);
};

}

#endif

/* ERROR/WARNING messages:

E: Invalid python command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Python invoke of undefined function

Cannot invoke a function that has not been previously defined.

E: Python variable does not match Python function

This matching is defined by the python-style variable and the python
command.

E: Cannot embed Python when also extending Python with LAMMPS

When running LAMMPS via Python through the LAMMPS library interface
you cannot also user the input script python command.

E: Could not initialize embedded Python

The main module in Python was not accessible.

E: Could not open Python file

The specified file of Python code cannot be opened.  Check that the
path and name are correct.

E: Could not process Python file

The Python code in the specified file was not run successfully by
Python, probably due to errors in the Python code.

E: Could not process Python string

The Python code in the here string was not run successfully by Python,
probably due to errors in the Python code.

E: Could not find Python function

The provided Python code was run successfully, but it not
define a callable function with the required name.

E: Python function is not callable

The provided Python code was run successfully, but it not
define a callable function with the required name.

E: Could not create Python function arguments

This is an internal Python error, possibly because the number
of inputs to the function is too large.

E: Could not evaluate Python function input variable

Self-explanatory.

E: Python function evaluation failed

The Python function did not run successfully and/or did not return a
value (if it is supposed to return a value).  This is probably due to
some error condition in the function.

*/
