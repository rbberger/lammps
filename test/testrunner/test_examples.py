# LAMMPS regression test driver using Python's unittest
#
# Run with "nosetests -v" in main LAMMPS folder
# Run with "nosetests  --with-xunit" to generate xUnit report file
__author__ = 'Richard Berger'
__email__ = "richard.berger@temple.edu"

import unittest
import os
import glob
from subprocess import call

# Before running any tests these two environment variables must be set

LAMMPS_DIR=os.environ['LAMMPS_DIR']          # full path of LAMMPS main directory
LAMMPS_BINARY=os.environ['LAMMPS_BINARY']    # full path of LAMMPS binary being tested


class LAMMPSTestCase:
    """ Mixin class for each LAMMPS test case. Defines utility function to run in serial or parallel"""
    def run_script(self, script_name, nprocs=1, nthreads=1, screen=True, launcher=[]):
        if screen:
            output_options = []
        else:
            output_options = ["-screen", "none"]

        exe = launcher + [LAMMPS_BINARY]

        if nprocs > 1:
            return call(["mpirun", "-np", str(nprocs)] + exe + ["-in", script_name] + output_options, cwd=self.cwd)
        elif nthreads > 1:
            return call(["mpirun", "-np", str(nprocs)] + exe + ["-sf", "omp", "-pk", "omp", str(nthreads),  "-in", script_name] + output_options, cwd=self.cwd)
        return call(exe + ["-in", script_name] + output_options, cwd=self.cwd)




# One tedious way of defining test cases is to fully define each test case as a class
# The benefit being to customize each test individually, or marking individual tests for being skipped.


# class BalanceTestCase(LAMMPSTestCase, unittest.TestCase):
#     def setUp(self):
#         self.cwd = os.path.join(LAMMPS_DIR, "examples", 'balance')
#
#     def test_balance_serial(self):
#         rc = self.run_script("in.balance")
#         self.assertEqual(rc, 0)
#
#     def test_balance_parallel(self):
#         rc = self.run_script("in.balance", nprocs=4)
#         self.assertEqual(rc, 0)
#
#     def test_balance_bond_fast_serial(self):
#         rc = self.run_script("in.balance.bond.fast")
#         self.assertEqual(rc, 0)
#
#     @unittest.skip("Randomly crashes")
#     def test_balance_bond_fast_parallel(self):
#         rc = self.run_script("in.balance.bond.fast", nprocs=4)
#         self.assertEqual(rc, 0)
#
#     def test_balance_bond_slow_serial(self):
#         rc = self.run_script("in.balance.bond.slow")
#         self.assertEqual(rc, 0)
#
#     def test_balance_bond_slow_parallel(self):
#         rc = self.run_script("in.balance.bond.slow", nprocs=4)
#         self.assertEqual(rc, 0)

# To save typing and quickly set this up, I've created a helper function to generate the
# boilerplate code quickly


def CreateLAMMPSTestCase(testcase_name, script_names):
    """ Utility function to generate LAMMPS test case classes with both serial and parallel
        testing functions for each input script"""
    def setUp(self):
        self.cwd = os.path.join(LAMMPS_DIR, "examples", testcase_name)

    def test_serial(script_name):
        def test_serial_run(self):
            rc = self.run_script(script_name)
            self.assertEqual(rc, 0)
        return test_serial_run

    def test_parallel(script_name):
        def test_parallel_run(self):
            rc = self.run_script(script_name, nprocs=4)
            self.assertEqual(rc, 0)
        return test_parallel_run

    def test_parallel_omp(script_name):
        def test_parallel_omp_run(self):
            rc = self.run_script(script_name, nthreads=4)
            self.assertEqual(rc, 0)
        return test_parallel_omp_run

    def test_serial_valgrind(name, script_name):
        supp_path = os.path.join(LAMMPS_DIR, 'tools', 'valgrind', 'lammps.supp')
        valgrind_exec = ["valgrind", "--leak-check=full", "--xml=yes", "--xml-file=" + name + ".memcheck", "--suppressions=" + supp_path]
        valgrind_exec += ["--suppressions=/usr/share/openmpi/openmpi-valgrind.supp"]

        def test_serial_valgrind_run(self):
            rc = self.run_script(script_name,launcher=valgrind_exec)
            self.assertEqual(rc, 0)
        return test_serial_valgrind_run

    methods = {"setUp": setUp}

    for script_name in script_names:
        name = '_'.join(script_name.split('.')[1:])
        methods["test_" + name + "_serial"] = test_serial(script_name)
        methods["test_" + name + "_parallel"] = test_parallel(script_name)
        methods["test_" + name + "_parallel_omp"] = test_parallel_omp(script_name)
        methods["test_" + name + "_serial_valgrind"] = test_serial_valgrind(name, script_name)

    return type(testcase_name.title() + "TestCase", (LAMMPSTestCase, unittest.TestCase), methods)

# This makes simple test case definitions much easier:

# BodyTestCase = CreateLAMMPSTestCase("body", ["in.body"])
# ColloidTestCase = CreateLAMMPSTestCase("colloid", ["in.colloid"])
# CombTestCase = CreateLAMMPSTestCase("comb", ["in.comb.Cu", "in.comb.Cu2O.elastic", "in.comb.HfO2",
#                                              "in.comb.Si", "in.comb.Si.elastic", "in.comb3"])
# CrackTestCase = CreateLAMMPSTestCase("crack", ["in.crack"])
# FlowTestCase = CreateLAMMPSTestCase("flow", ["in.flow.couette", "in.flow.pois"])
# FrictionTestCase = CreateLAMMPSTestCase("friction", ["in.friction"])
# HugoniostatTestCase = CreateLAMMPSTestCase("hugoniostat", ["in.hugoniostat"])
# IndentTestCase = CreateLAMMPSTestCase("indent", ["in.indent", "in.indent.min"])
# MeltTestCase = CreateLAMMPSTestCase("melt", ["in.melt"])
# MinTestCase = CreateLAMMPSTestCase("min", ["in.min", "in.min.box"])
# ObstacleTestCase = CreateLAMMPSTestCase("obstacle", ["in.obstacle"])
# PeptideTestCase = CreateLAMMPSTestCase("peptide", ["in.peptide"])


# Last, but not least, I'm lazy... so I'll collect all the script files and generate the tests
# automatically by a recursive search and skipping a selection of folders

examples_dir = os.path.join(LAMMPS_DIR, 'examples')

# skipping "balance" to not redefine it (see above)
# everything else I still have to test myself or needs adjustment (e.g. number of timesteps)
skip_list = ['accelerate', 'hugoniostat', 'kim', 'neb', 'python', 'reax', 'rerun', 'tad']

for name in os.listdir(examples_dir):
    path = os.path.join(examples_dir, name)
    print(name)

    if name in skip_list:
        continue

    # for now only use the lower case examples (=simple ones)

    if name.islower() and os.path.isdir(path):
        script_names = map(os.path.basename, glob.glob(os.path.join(path, 'in.*')))
        vars()[name.title() + "TestCase"] = CreateLAMMPSTestCase(name, script_names)

def SkipTest(cls, func_name, reason):
    setattr(cls, func_name, unittest.skip(reason)(getattr(cls, func_name)))

SkipTest(CombTestCase, "test_comb3_parallel_omp", "comb3 currently not supported by USER-OMP")
SkipTest(BalanceTestCase, "test_balance_bond_fast_parallel", "Crashes randomly")

if __name__ == '__main__':
    unittest.main()
