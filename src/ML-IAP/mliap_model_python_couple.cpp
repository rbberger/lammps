// clang-format off
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

// C style library interface to LAMMPS.
// See the manual for detailed documentation.

#define LAMMPS_LIB_MPI 1
#include "library.h"
#include <mpi.h>

#include "accelerator_kokkos.h"
#include "atom.h"
#include "atom_vec.h"
#include "comm.h"
#include "compute.h"
#include "domain.h"
#include "dump.h"
#include "error.h"
#include "fix.h"
#include "fix_external.h"
#include "force.h"
#include "group.h"
#include "info.h"
#include "input.h"
#include "lmppython.h"
#include "memory.h"
#include "modify.h"
#include "molecule.h"
#include "neigh_list.h"
#include "neighbor.h"
#include "output.h"
#if defined(LMP_PLUGIN)
#include "plugin.h"
#endif
#include "region.h"
#include "respa.h"
#include "thermo.h"
#include "timer.h"
#include "universe.h"
#include "update.h"
#include "variable.h"

#include <cstring>
#include <vector>

#if defined(LAMMPS_EXCEPTIONS)
#include "exceptions.h"
#endif

#include  "mliap_model_python.h"
#include  "mliap_model_python_couple.h"

using namespace LAMMPS_NS;

// ----------------------------------------------------------------------
// utility macros
// ----------------------------------------------------------------------

/* ----------------------------------------------------------------------
   macros for optional code path which captures all exceptions
   and stores the last error message. These assume there is a variable lmp
   which is a pointer to the current LAMMPS instance.

   Usage:

   BEGIN_CAPTURE
   {
     // code paths which might throw exception
     ...
   }
   END_CAPTURE
------------------------------------------------------------------------- */

#ifdef LAMMPS_EXCEPTIONS
#define BEGIN_CAPTURE \
  Error *error = lmp->error; \
  try

#define END_CAPTURE \
  catch(LAMMPSAbortException &ae) { \
    int nprocs = 0; \
    MPI_Comm_size(ae.universe, &nprocs ); \
    \
    if (nprocs > 1) { \
      error->set_last_error(ae.message, ERROR_ABORT); \
    } else { \
      error->set_last_error(ae.message, ERROR_NORMAL); \
    } \
  } catch(LAMMPSException &e) { \
    error->set_last_error(e.message, ERROR_NORMAL); \
  }
#else
#define BEGIN_CAPTURE
#define END_CAPTURE
#endif

void lammps_mliap_load_model(void *handle, void *model)
{
  LAMMPS *lmp = (LAMMPS *) handle;

  BEGIN_CAPTURE
  {
    MLIAPModelPython::set_unloaded_model(lmp, model);
  }
  END_CAPTURE
}
