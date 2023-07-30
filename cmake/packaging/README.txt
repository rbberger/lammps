Static LAMMPS universal binaries for MacOS X (arm64/x86_64)
===========================================================

This package provides static binaries of LAMMPS that should run on most
current MacOS X systems.  Note the binaries are serial and only enable a subset
of the available packages.

After copying the LAMMPS folder into your Applications folder, please follow
these steps:

1. Open the Terminal app

2. Type the following command and press ENTER:

   open ~/.zprofile

   This will open a text editor for modifying the .zprofile file in your home
   directory. 

3. Add the following lines to the end of the file, save it, and close the editor

   export LAMMPS_INSTALL_DIR=/Applications/LAMMPS
   export LAMMPS_POTENTIALS=/Applications/LAMMPS/share/lammps/potentials
   export LAMMPS_BENCH_DIR=$LAMMPS_INSTALL_DIR/bench
   export PATH=${LAMMPS_INSTALL_DIR}/bin:$PATH

4. In your existing terminal, type the following command make the settings active

   source ~/.zprofile

   Note, you don't have to type this in new terminals, since they will apply
   the changes from .zprofile automatically.

   Note: the above assumes you use the default shell (zsh) that comes with
   MacOS. If you customized MacOS to use a different shell, you'll need to modify
   that shell's init file (.cshrc, .bashrc, etc.) instead with appropiate commands
   to modify the same environment variables.

5. Try running LAMMPS (which might fail, see step 6)

   lmp -i $LAMMPS_BENCH_DIR/in.lj

6. Allow lmp executable to run in MacOS Security settings

   MacOS will most likely block the initial run of the lmp executable, since it
   was downloaded from the internet and is missing a known signature from an
   identified developer. Go to "Settings" and search for "Security settings". It
   should display a message that "lmp" was blocked. Press "Open anyway", which
   might prompt you for your admin credentials. Afterwards lmp should now work as
   expected.
