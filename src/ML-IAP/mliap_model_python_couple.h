/* -*- c -*- ------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifndef LAMMPS_MLIAP_COUPLE_LIBRARY_H
#define LAMMPS_MLIAP_COUPLE_LIBRARY_H


/* Ifdefs to allow this file to be included in C and C++ programs */

#ifdef __cplusplus
extern "C" {
#endif

void lammps_mliap_load_model(void *handle, void * model);

#ifdef __cplusplus
}
#endif

#endif /* LAMMPS_MLIAP_COUPLE_LIBRARY_H */
