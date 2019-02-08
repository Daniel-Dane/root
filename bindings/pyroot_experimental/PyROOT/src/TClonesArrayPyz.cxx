// Author: Enric Tejedor CERN  02/2019
// Original PyROOT code by Wim Lavrijsen, LBL

/*************************************************************************
 * Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

// Bindings
#include "CPyCppyy.h"
#include "PyROOTPythonize.h"
#include "CPPInstance.h"
#include "MemoryRegulator.h"
#include "Utility.h"
#include "PyzCppHelpers.hxx"

// ROOT
#include "TClass.h"
#include "TClonesArray.h"

using namespace CPyCppyy;

// Helper: converts Python index into straight C index.
static PyObject *PyStyleIndex(PyObject *self, PyObject *index)
{
   Py_ssize_t idx = PyInt_AsSsize_t(index);
   if (idx == (Py_ssize_t)-1 && PyErr_Occurred())
      return nullptr;

   // To know the capacity of a TClonesArray, we need to invoke GetSize
   PyObject *pysize = CallPyObjMethod(self, "GetSize");
   if (!pysize) {
      PyErr_Clear();
      return nullptr;
   }

   Py_ssize_t size = PyInt_AsSsize_t(pysize);
   Py_DECREF(pysize);
   if (idx >= size || (idx < 0 && idx < -size)) {
      PyErr_SetString(PyExc_IndexError, "index out of range");
      return nullptr;
   }

   PyObject *pyindex = nullptr;
   if (idx >= 0) {
      Py_INCREF(index);
      pyindex = index;
   } else {
      pyindex = PyLong_FromSsize_t(size + idx);
   }

   return pyindex;
}

// Customize item setting
PyObject *SetItem(CPPInstance *self, PyObject *args)
{
   CPPInstance *pyobj = nullptr;
   PyObject *idx = nullptr;
   if (!PyArg_ParseTuple(args, const_cast<char *>("OO!:__setitem__"), &idx, &CPPInstance_Type, &pyobj))
      return nullptr;

   if (!self->GetObject()) {
      PyErr_SetString(PyExc_TypeError, "unsubscriptable object");
      return nullptr;
   }

   PyObject *pyindex = PyStyleIndex((PyObject *)self, idx);
   if (!pyindex)
      return nullptr;
   int index = (int)PyLong_AsLong(pyindex);
   Py_DECREF(pyindex);

   // Get hold of the actual TClonesArray
   auto cla = (TClonesArray *)GetTClass(self)->DynamicCast(TClonesArray::Class(), self->GetObject());

   if (!cla) {
      PyErr_SetString(PyExc_TypeError, "attempt to call with null object");
      return nullptr;
   }

   if (Cppyy::GetScope(cla->GetClass()->GetName()) != pyobj->ObjectIsA()) {
      PyErr_Format(PyExc_TypeError, "require object of type %s, but %s given", cla->GetClass()->GetName(),
                   Cppyy::GetFinalName(pyobj->ObjectIsA()).c_str());
   }

   // Destroy old object, if applicable
   if (((const TClonesArray &)*cla)[index]) {
      cla->RemoveAt(index);
   }

   if (pyobj->GetObject()) {
      // Accessing an entry will result in new, uninitialized memory (if properly used)
      TObject *object = (*cla)[index];
      pyobj->CppOwns();
      MemoryRegulator::RegisterPyObject(pyobj, object);
      memcpy((void *)object, pyobj->GetObject(), cla->GetClass()->Size());
   }

   Py_RETURN_NONE;
}

////////////////////////////////////////////////////////////////////////////
/// \brief Customize the setting of an item of a TClonesArray.
/// \param[in] self Always null, since this is a module function.
/// \param[in] args Pointer to a Python tuple object containing the arguments
/// received from Python.
///
/// Inject a __setitem__ implementation that customizes the setting of an item
/// into a TClonesArray.
///
/// The __setitem__ pythonization that TClonesArray inherits from TSeqCollection
/// does not apply in this case and a redefinition is required. The reason is
/// TClonesArray sets objects by constructing them in-place, which is impossible
/// to support as the Python object given as value must exist a priori. It can,
/// however, be memcpy'd and stolen.
PyObject *PyROOT::AddSetItemTCAPyz(PyObject * /* self */, PyObject *args)
{
   PyObject *pyclass = PyTuple_GetItem(args, 0);
   Utility::AddToClass(pyclass, "__setitem__", (PyCFunction)SetItem);
   Py_RETURN_NONE;
}