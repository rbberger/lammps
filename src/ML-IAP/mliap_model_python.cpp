/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: Nicholas Lubbers (LANL)
------------------------------------------------------------------------- */

#ifdef MLIAP_PYTHON

#include "mliap_model_python.h"

#include "error.h"
#include "lmppython.h"
#include "mliap_data.h"
#include "pair_mliap.h"
#include "python_compat.h"
#include "utils.h"

#include <Python.h>
#include <map>
#include <deque>
#include <algorithm>

#include "python_utils.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

static std::map<LAMMPS*, std::deque<MLIAPModelPython*>> UNLOADED_MODELS;

void MLIAPModelPython::register_model(LAMMPS * lmp, MLIAPModelPython * model)
{
  UNLOADED_MODELS[lmp].push_back(model);
}

void MLIAPModelPython::deregister_model(LAMMPS * lmp, MLIAPModelPython * model)
{
  auto it = std::find(UNLOADED_MODELS[lmp].begin(), UNLOADED_MODELS[lmp].end(), model);
  if (it != UNLOADED_MODELS[lmp].end()) {
    UNLOADED_MODELS[lmp].erase(it);
  }
}

void MLIAPModelPython::set_unloaded_model(LAMMPS * lmp, void * py_model)
{
  if (UNLOADED_MODELS[lmp].size() == 0) {
    lmp->error->all(FLERR, "No model in the waiting area.");
  } else if (UNLOADED_MODELS[lmp].size() > 1) {
    lmp->error->all(FLERR, "Model is amibguous, more than one model in waiting area.");
  }
  
  PyUtils::GIL lock;
  MLIAPModelPython* mliap_model_python = UNLOADED_MODELS[lmp].front();
  UNLOADED_MODELS[lmp].pop_front();
  PyObject * python_model = (PyObject*)py_model;
  Py_XINCREF(python_model);
  mliap_model_python->python_model = py_model;
  mliap_model_python->connect_param_counts();
}

/* ---------------------------------------------------------------------- */

MLIAPModelPython::MLIAPModelPython(LAMMPS *lmp, char *coefffilename) :
    MLIAPModel(lmp, coefffilename)
{
  python_model = nullptr;
  model_loaded = 0;
  python->init();
  PyUtils::GIL lock;

  PyObject *pyMain = PyImport_AddModule("__main__");

  if (!pyMain) {
    error->all(FLERR, "Could not initialize embedded Python");
  }

  mliap_module = PyImport_ImportModule("lammps.mliap");

  if (!mliap_module) {
    PyErr_Print();
    PyErr_Clear();
    error->all(FLERR, "Loading MLIAPPY module failure.");
  }
  // Recipe from lammps/src/pair_python.cpp :
  // add current directory to PYTHONPATH
  PyObject *py_path = PySys_GetObject((char *) "path");
  PyList_Append(py_path, PY_STRING_FROM_STRING("."));

  // if LAMMPS_POTENTIALS environment variable is set, add it to PYTHONPATH as well
  const char *potentials_path = getenv("LAMMPS_POTENTIALS");
  if (potentials_path != NULL) { PyList_Append(py_path, PY_STRING_FROM_STRING(potentials_path)); }

  if (coefffilename) read_coeffs(coefffilename);

  nonlinearflag = 1;
  MLIAPModelPython::register_model(lmp, this);
}

/* ---------------------------------------------------------------------- */

MLIAPModelPython::~MLIAPModelPython()
{
  PyUtils::GIL lock;
  PyObject * py_model = (PyObject*)python_model;
  Py_CLEAR(py_model);
  python_model = nullptr;
  MLIAPModelPython::deregister_model(lmp, this);
}

/* ----------------------------------------------------------------------
   get number of parameters
   ---------------------------------------------------------------------- */

int MLIAPModelPython::get_nparams()
{
  return nparams;
}

void MLIAPModelPython::read_coeffs(char *fname)
{
  PyUtils::GIL lock;
  bool loaded = false;

  PyObject * MLIAPPY_load_model = PyObject_GetAttrString((PyObject*)mliap_module, "MLIAPPY_load_model");
  PyObject * model = PyObject_CallFunction(MLIAPPY_load_model, (char *)"s", fname);

  if(!model || PyErr_Occurred()) {
    PyErr_Print();
    PyErr_Clear();
    error->all(FLERR, "Loading python model failure.");
  }

  if(model != Py_None) {
    loaded = true;
    python_model = model;
  }

  if (loaded) {
    this->connect_param_counts();
  } else {
    utils::logmesg(lmp, "Loading python model deferred.\n");
  }
}

// Finalize loading of the model.
void MLIAPModelPython::connect_param_counts()
{
  PyUtils::GIL lock;
  PyObject * mliap_python_model = (PyObject*)python_model;

  PyObject * py_n_elements = PyObject_GetAttrString(mliap_python_model, "n_elements");
  PyObject * py_n_params = PyObject_GetAttrString(mliap_python_model, "n_params");
  PyObject * py_n_descriptors = PyObject_GetAttrString(mliap_python_model, "n_descriptors");

  
  nelements = (int)PyLong_AsLong(py_n_elements);
  nparams = (int)PyLong_AsLong(py_n_params);
  ndescriptors = (int)PyLong_AsLong(py_n_descriptors);

  Py_CLEAR(py_n_elements);
  Py_CLEAR(py_n_params);
  Py_CLEAR(py_n_descriptors);


  if (PyErr_Occurred()) {
    PyErr_Print();
    PyErr_Clear();
    error->all(FLERR, "Loading python model failure.");
  }
  model_loaded = 1;
  utils::logmesg(lmp, "Loading python model complete.\n");
}

/* ----------------------------------------------------------------------
   Calculate model gradients w.r.t descriptors
   for each atom beta_i = dE(B_i)/dB_i
   ---------------------------------------------------------------------- */

void MLIAPModelPython::compute_gradients(MLIAPData *data)
{
  if (not model_loaded) { error->all(FLERR, "Model not loaded."); }

  PyUtils::GIL lock;

  PyObject * py_model = (PyObject*)python_model;

  int n_d = data->ndescriptors;
  int n_a = data->nlistatoms;

  // Make numpy arrays from pointers
  PyObject * beta_np = (PyObject*)np_darray_from_buffer_2D(n_a, n_d, &data->betas[0][0]);
  PyObject * desc_np = (PyObject*)np_darray_from_buffer_2D(n_a, n_d, &data->descriptors[0][0]);
  PyObject * elem_np = (PyObject*)np_iarray_from_buffer_1D(n_a, &data->ielems[0]);
  PyObject * en_np = (PyObject*)np_darray_from_buffer_1D(n_a, &data->eatoms[0]);

  // Invoke python model on numpy arrays.
  PyObject_CallFunction(py_model, (char *)"OOOO", elem_np, desc_np, beta_np, en_np);

  // Get the total energy from the atom energy.
  double total_energy = 0.0;
  const double * eatoms = data->eatoms;

  for(int i = 0; i < n_a; ++i) {
    total_energy += eatoms[i]; 
  }

  data->energy = total_energy;

  Py_CLEAR(beta_np);
  Py_CLEAR(desc_np);
  Py_CLEAR(elem_np);
  Py_CLEAR(en_np);

  if (PyErr_Occurred()) {
    PyErr_Print();
    PyErr_Clear();
    error->all(FLERR, "Running python model failure.");
  }
}

void * MLIAPModelPython::np_darray_from_buffer_2D(size_t m, size_t n, double * buffer) {
  PyObject * mliappy = (PyObject*)mliap_module;
  PyObject * bufferPtr = PY_VOID_POINTER(buffer);
  PyObject * double_array = PyObject_GetAttrString(mliappy, "double_array");
  PyObject * result = PyObject_CallFunction(double_array, (char *)"Oii", bufferPtr, m, n);
  Py_CLEAR(bufferPtr);
  Py_CLEAR(double_array);
  return result;
}

void * MLIAPModelPython::np_darray_from_buffer_1D(size_t n, double * buffer) {
  PyObject * mliappy = (PyObject*)mliap_module;
  PyObject * bufferPtr = PY_VOID_POINTER(buffer);
  PyObject * double_array = PyObject_GetAttrString(mliappy, "double_array");
  PyObject * result = PyObject_CallFunction(double_array, (char *)"Oi", bufferPtr, n);
  Py_CLEAR(bufferPtr);
  Py_CLEAR(double_array);
  return result;
}

void * MLIAPModelPython::np_iarray_from_buffer_2D(size_t m, size_t n, int * buffer) {
  PyObject * mliappy = (PyObject*)mliap_module;
  PyObject * bufferPtr = PY_VOID_POINTER(buffer);
  PyObject * int_array = PyObject_GetAttrString(mliappy, "int_array");
  PyObject * result = PyObject_CallFunction(int_array, (char *)"Oii", bufferPtr, m, n);
  Py_CLEAR(bufferPtr);
  Py_CLEAR(int_array);
  return result;
}

void * MLIAPModelPython::np_iarray_from_buffer_1D(size_t n, int * buffer) {
  PyObject * mliappy = (PyObject*)mliap_module;
  PyObject * bufferPtr = PY_VOID_POINTER(buffer);
  PyObject * int_array = PyObject_GetAttrString(mliappy, "int_array");
  PyObject * result = PyObject_CallFunction(int_array, (char *)"Oi", bufferPtr, n);
  Py_CLEAR(bufferPtr);
  Py_CLEAR(int_array);
  return result;
}


/* ----------------------------------------------------------------------
   Calculate model double gradients w.r.t descriptors and parameters
   for each atom energy gamma_lk = d2E(B)/dB_k/dsigma_l,
   where sigma_l is a parameter, B_k a descriptor,
   and atom subscript i is omitted

   gamma is in CSR format:
      nnz = number of non-zero values
      gamma_row_index[inz] = l indices, 0 <= l < nparams
      gamma_col_indexiinz] = k indices, 0 <= k < ndescriptors
      gamma[i][inz] = non-zero values, 0 <= inz < nnz

   egradient is derivative of energy w.r.t. parameters
   ---------------------------------------------------------------------- */

void MLIAPModelPython::compute_gradgrads(class MLIAPData *)
{
  error->all(FLERR, "compute_gradgrads not implemented");
}

/* ----------------------------------------------------------------------
   calculate gradients of forces w.r.t. parameters
   egradient is derivative of energy w.r.t. parameters
   ---------------------------------------------------------------------- */

void MLIAPModelPython::compute_force_gradients(class MLIAPData *)
{
  error->all(FLERR, "compute_force_gradients not implemented");
}

/* ----------------------------------------------------------------------
   count the number of non-zero entries in gamma matrix
   ---------------------------------------------------------------------- */

int MLIAPModelPython::get_gamma_nnz(class MLIAPData *)
{
  // todo: get_gamma_nnz
  return 0;
}

double MLIAPModelPython::memory_usage()
{
  // todo: get approximate memory usage in coupling code.
  return 0;
}

#endif
