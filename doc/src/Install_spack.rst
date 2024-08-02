Download and build an executable for Linux or macOS via Spack
-------------------------------------------------------------

LAMMPS binaries are available for macOS and Linux via the
`Spack <spack_>`_ package manager.

First, one must set up Spack on your system.  Follow
the instructions to set up `Spack <spack_install_>`_, then
create a Spack environment (named `my-lammps-env` or whatever you
prefer) for your LAMMPS install:

.. code-block:: bash

   spack env create my-lammps-env

Then, you can install LAMMPS on your system with the following command:

.. code-block:: bash

   spack env activate my-lammps-env
   spack add lammps
   spack install

The LAMMPS binary will be built with a minimal set of dependencies and
packages. In order to enable more features and packages you must use the
available variants defined in the LAMMPS Spack package. You can find out about
all available versions and variants you can build with the following command:

.. code-block:: bash

   spack info lammps

If you have problems with the installation, you can post issues to `this
link <spack_issues_>`_ or reach out via the `Spack Slack <spack_slack_>`_.

.. _spack_issues: https://github.com/spack/spack/issues
.. _spack_slack: https://slack.spack.io/
.. _spack_install: https://spack.readthedocs.io/en/latest/getting_started.html

.. note::

   If you have questions about LAMMPS built via Spack, you need to contact the
   people preparing those packages. The LAMMPS developers have no control over
   their choices of how they configure and build their packages and when they
   update them.
