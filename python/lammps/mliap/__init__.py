import numpy as np
import pickle
import sys
from ctypes import POINTER, c_void_p, c_char_p, c_double, c_int, c_int32, c_int64, cast, py_object, pythonapi


def load_model(lmp, model):
    lmp.lib.lammps_mliap_load_model.argtypes = [c_void_p, py_object]
    lmp.lib.lammps_mliap_load_model.restype = None
    lmp.lib.lammps_mliap_load_model(lmp.lmp, model)

def MLIAPPY_load_model(fname):
    if fname == "LATER":
        return None

    if fname.endswith(".pt") or fname.endswith('.pth'):
        import torch
        model = torch.load(fname)
    else:
        with open(fname,'rb') as pfile:
            model = pickle.load(pfile)
    return model

def int_array(ptr, nelem, dim=1):
  pythonapi.PyCapsule_GetPointer.restype = c_void_p
  pythonapi.PyCapsule_GetPointer.argtypes = [py_object, c_char_p]
  raw_ptr = c_void_p(pythonapi.PyCapsule_GetPointer(ptr, None))
  import numpy as np
  ptr = cast(raw_ptr, POINTER(c_int * nelem * dim))
  a = np.frombuffer(ptr.contents, dtype=np.int32)

  if dim > 1:
    a.shape = (nelem, dim)
  else:
    a.shape = (nelem)
  return a

# -------------------------------------------------------------------------

def double_array(ptr, nelem, dim=1):
  pythonapi.PyCapsule_GetPointer.restype = c_void_p
  pythonapi.PyCapsule_GetPointer.argtypes = [py_object, c_char_p]
  raw_ptr = c_void_p(pythonapi.PyCapsule_GetPointer(ptr, None))
  import numpy as np
  ptr = cast(raw_ptr, POINTER(c_double * nelem * dim))
  a = np.frombuffer(ptr.contents)

  if dim > 1:
    a.shape = (nelem, dim)
  else:
    a.shape = (nelem)
  return a
