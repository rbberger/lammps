# LAMMPS regression test driver using Python's unittest
from __future__ import print_function
__author__ = 'Richard Berger'
__email__ = "richard.berger@temple.edu"

import os
import sys
from subprocess import call

# Before running any tests these two environment variables must be set

LAMMPS_DIR=os.environ['LAMMPS_DIR']          # full path of LAMMPS main directory
LAMMPS_BINARY=os.environ['LAMMPS_BINARY']    # full path of LAMMPS binary being tested

class LAMMPSTestCase:
    """ Mixin class for each LAMMPS test case. Defines utility function to run in serial or parallel"""
    def run_script(self, script_name, nprocs=1, nthreads=1, screen=True, log=None, launcher=[], force_openmp=True, force_mpi=True):
        if screen:
            output_options = []
        else:
            output_options = ["-screen", "none"]

        if log:
            output_options += ["-log", log]

        exe = launcher + [LAMMPS_BINARY]

        mpi_options = []
        lammps_options = ["-in", script_name] + output_options

        if nthreads > 1 or force_openmp:
            lammps_options += ["-sf", "omp"]

        if nprocs > 1 or force_mpi:
            mpi_options = ["mpirun", "-np", str(nprocs), "-x", "OMP_NUM_THREADS="+str(nthreads)]
        elif nthreads > 1 or force_openmp:
            lammps_options += ["-pk", "omp", str(nthreads)]

        outfile_path = os.path.join(self.cwd, "stdout.log")
        errfile_path = os.path.join(self.cwd, "stderr.log")

        with open(outfile_path, "w+") as outfile, open(errfile_path, "w+") as errfile:
            retcode = call(mpi_options + exe + lammps_options, cwd=self.cwd, stdout=outfile, stderr=errfile)
            outfile.seek(0)
            errfile.seek(0)
            out = outfile.read()
            err = errfile.read()

        # output to Python stdout and stderr so unittest framework captures it
        if out:
            print(out, file=sys.stdout)

        if err:
            print(err, file=sys.stderr)

        return retcode

def SkipTest(cls, func_name, reason):
    """ utility function to skip a specific test for a reason """
    setattr(cls, func_name, unittest.skip(reason)(getattr(cls, func_name)))
