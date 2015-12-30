#include <gtest/gtest.h>
#include <mpi.h>
#include "neighbor.h"
#include "update.h"
#include "domain.h"
#include "atom.h"
#include "input.h"

using namespace LAMMPS_NS;

TEST(Core, lennardJonesDefaults) {
  const char * argv[3] = {"lammps", "-screen", "off"};
  LAMMPS lammps(3, const_cast<char**>(argv), MPI_COMM_WORLD);
  auto L = [&](const char * line) { lammps.input->one(line); };

  L("units lj");
  L("atom_style	atomic");
  L("lattice fcc 0.8442");
  L("region box block 0 10 0 10 0 10");
  L("create_box	1 box");
  L("create_atoms	1 box");

  ASSERT_EQ(3, lammps.domain->dimension);

  ASSERT_TRUE(lammps.domain->xperiodic);
  ASSERT_TRUE(lammps.domain->yperiodic);
  ASSERT_TRUE(lammps.domain->zperiodic);

  ASSERT_EQ(4000, lammps.atom->natoms);
  ASSERT_EQ(2000, lammps.neighbor->oneatom);

  ASSERT_DOUBLE_EQ(0.005, lammps.update->dt);
}
