/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * Python API for writing a set of data-blocks into a file.
 * Useful for writing out asset-libraries, defines: `bpy.data.libraries.write(...)`.
 */

#include <Python.h>
#include <cstddef>
#include <cstring>
#include <ctime>

#include "BLI_fileops.hh"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_blendfile.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLO_readfile.hh"
#include "BLO_writefile.hh"

#include "DNA_ID.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_types.hh"

#include "bpy_capi_utils.hh"
#include "bpy_library.hh" /* Declaration for #BPY_library_load_method_def */
#include "bpy_rna.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

using namespace blender::bke::blendfile;

PyDoc_STRVAR(
    /* Wrap. */
    bpy_lib_modify_external_doc,
    ".. method:: modify_external(filepath, id_type, id_name, properties, create_backup=True)\n"
    "\n"
    "   Modify a data-block in an external blend file without opening it.\n"
    "\n"
    "   :arg filepath: The path to the blend-file containing the data-block to modify.\n"
    "   :type filepath: str | bytes\n"
    "   :arg id_type: Type of the data-block (e.g. 'OBJECT', 'MATERIAL').\n"
    "   :type id_type: str\n"
    "   :arg id_name: Name of the data-block to modify.\n"
    "   :type id_name: str\n"
    "   :arg properties: Dictionary of properties to modify (RNA paths to values).\n"
    "   :type properties: dict[str, Any]\n"
    "   :arg create_backup: Create a timestamped backup before modifying.\n"
    "   :type create_backup: bool\n");
static PyObject *bpy_lib_modify_external(BPy_PropertyRNA * /*self*/, PyObject *args, PyObject *kw)
{
  /* args */
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};
  char filepath_abs[FILE_MAX];
  const char *id_type_str = nullptr;
  const char *id_name = nullptr;
  PyObject *properties_dict = nullptr;
  bool create_backup = true;

  static const char *_keywords[] = {
      "filepath",
      "id_type",
      "id_name",
      "properties",
      "create_backup",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `filepath` */
      "s"  /* `id_type` */
      "s"  /* `id_name` */
      "O!" /* `properties` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `create_backup` */
      ":modify_external",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseUnicodeAsBytesAndSize,
                                        &filepath_data,
                                        &id_type_str,
                                        &id_name,
                                        &PyDict_Type,
                                        &properties_dict,
                                        PyC_ParseBool,
                                        &create_backup))
  {
    return nullptr;
  }

  /* Convert id_type string to ID_* code */
  short id_type = BKE_idtype_idcode_from_name(id_type_str);
  if (id_type == 0) {
    PyErr_Format(PyExc_ValueError, "Unknown ID type: %.200s", id_type_str);
    Py_XDECREF(filepath_data.value_coerce);
    return nullptr;
  }

  STRNCPY(filepath_abs, filepath_data.value);
  Py_XDECREF(filepath_data.value_coerce);

  BLI_path_abs(filepath_abs, BKE_main_blendfile_path_from_global());

  /* Create backup if requested */
  if (create_backup) {
    char backup_path[FILE_MAX];
    char timestamp[32];
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
    SNPRINTF(backup_path, "%s.backup_%s", filepath_abs, timestamp);

    if (BLI_copy(filepath_abs, backup_path) != 0) {
      PyErr_Format(PyExc_IOError, "Failed to create backup at: %.200s", backup_path);
      return nullptr;
    }
  }

  /* Load the external file */
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);

  BlendFileReadReport bf_reports = {&reports};
  BlendFileData *bfd = BLO_read_from_file(filepath_abs, eBLOReadSkip(0), &bf_reports);

  if (bfd == nullptr) {
    if (BPy_reports_to_error(&reports, PyExc_IOError, false) == 0) {
      PyErr_Format(PyExc_IOError, "Failed to read file: %.200s", filepath_abs);
    }
    BKE_reports_free(&reports);
    return nullptr;
  }

  Main *temp_main = bfd->main;
  bfd->main = nullptr;
  MEM_freeN(bfd);

  /* Find the target ID */
  ListBase *lb = which_libbase(temp_main, id_type);
  if (lb == nullptr) {
    PyErr_Format(PyExc_ValueError, "Invalid ID type code: %d", id_type);
    BKE_main_free(temp_main);
    BKE_reports_free(&reports);
    return nullptr;
  }

  ID *target_id = nullptr;
  LISTBASE_FOREACH (ID *, id_iter, lb) {
    if (STREQ(id_iter->name + 2, id_name)) {
      target_id = id_iter;
      break;
    }
  }

  if (target_id == nullptr) {
    PyErr_Format(
        PyExc_ValueError, "ID not found: %s '%s' in file: %.200s", id_type_str, id_name, filepath_abs);
    BKE_main_free(temp_main);
    BKE_reports_free(&reports);
    return nullptr;
  }

  /* Apply property modifications using RNA path resolution.
   *
   * We use RNA_path_resolve_full_maybe_null to resolve complex data-paths (including
   * dots, indices, and collection access), then delegate conversion to the existing
   * pyrna_py_to_prop/pyrna_py_to_array_index helpers (Option B1).
   *
   * This approach:
   * - Handles ALL RNA types (bool, int, float, string, enum, pointer, collection)
   * - Supports array indexing (location[0]) and dotted paths (location.x)
   * - Reuses 618 lines of tested conversion code instead of reimplementing
   * - Requires only ~60 lines vs ~370 lines for manual implementation
   */
  PointerRNA target_ptr = RNA_id_pointer_create(target_id);

  PyObject *key, *value;
  Py_ssize_t pos = 0;
  bool modification_failed = false;

  while (PyDict_Next(properties_dict, &pos, &key, &value)) {
    const char *path = PyUnicode_AsUTF8(key);
    if (path == nullptr) {
      PyErr_SetString(PyExc_TypeError, "Property keys must be strings");
      modification_failed = true;
      break;
    }

    /* Resolve the RNA path (supports dots, indices, collections) */
    PointerRNA resolved_ptr;
    PropertyRNA *resolved_prop = nullptr;
    int array_index = -1;

    if (!RNA_path_resolve_full_maybe_null(
            &target_ptr, path, &resolved_ptr, &resolved_prop, &array_index) ||
        resolved_prop == nullptr)
    {
      PyErr_Format(PyExc_AttributeError,
                   "Property '%.200s' not found in %s '%s'",
                   path,
                   id_type_str,
                   id_name);
      modification_failed = true;
      break;
    }

    /* Check if property is editable */
    if (array_index == -1) {
      if (!RNA_property_editable(&resolved_ptr, resolved_prop)) {
        PyErr_Format(
            PyExc_AttributeError, "Property '%.200s' is read-only", path);
        modification_failed = true;
        break;
      }
    }
    else {
      if (!RNA_property_editable_index(&resolved_ptr, resolved_prop, array_index)) {
        PyErr_Format(PyExc_AttributeError,
                     "Property '%.200s' index %d is read-only",
                     path,
                     array_index);
        modification_failed = true;
        break;
      }
    }

    /* Apply the value: full property or array index */
    if (array_index == -1) {
      /* Set entire property (scalar, full array, pointer, etc.) */
      if (pyrna_py_to_prop(&resolved_ptr, resolved_prop, nullptr, value, "modify_external") == -1)
      {
        /* Error already set by pyrna_py_to_prop */
        modification_failed = true;
        break;
      }
    }
    else {
      /* Set specific array element (location[0], rotation_euler[2], etc.) */
      if (pyrna_py_to_array_index(&resolved_ptr,
                                  resolved_prop,
                                  0,            /* arraydim (0 for 1D vectors) */
                                  0,            /* arrayoffset (0 for standard access) */
                                  array_index,
                                  value,
                                  "modify_external") == -1)
      {
        /* Error already set by pyrna_py_to_array_index */
        modification_failed = true;
        break;
      }
    }
  }

  if (modification_failed) {
    BKE_main_free(temp_main);
    BKE_reports_free(&reports);
    return nullptr;
  }

  /* Write the modified file back */
  BlendFileWriteParams params{};
  params.remap_mode = BLO_WRITE_PATH_REMAP_NONE;
  params.use_save_versions = false;
  params.use_save_as_copy = false;
  params.use_userdef = false;
  params.thumb = nullptr;

  bool success = BLO_write_file(temp_main, filepath_abs, 0, &params, &reports);

  BKE_main_free(temp_main);

  PyObject *py_return_value;
  if (success) {
    BKE_reports_print(&reports, RPT_ERROR_ALL);
    py_return_value = Py_None;
    Py_INCREF(py_return_value);
  }
  else {
    if (BPy_reports_to_error(&reports, PyExc_IOError, false) == 0) {
      PyErr_SetString(PyExc_IOError, "Unknown error modifying library data");
    }
    py_return_value = nullptr;
  }

  BKE_reports_free(&reports);

  return py_return_value;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_lib_write_doc,
    ".. method:: write(filepath, datablocks, path_remap=False, fake_user=False, compress=False)\n"
    "\n"
    "   Write data-blocks into a blend file.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      Indirectly referenced data-blocks will be expanded and written too.\n"
    "\n"
    "   :arg filepath: The path to write the blend-file.\n"
    "   :type filepath: str | bytes\n"
    "   :arg datablocks: set of data-blocks.\n"
    "   :type datablocks: set[:class:`bpy.types.ID`]\n"
    "   :arg path_remap: Optionally remap paths when writing the file:\n"
    "\n"
    "      - ``NONE`` No path manipulation (default).\n"
    "      - ``RELATIVE`` Remap paths that are already relative to the new location.\n"
    "      - ``RELATIVE_ALL`` Remap all paths to be relative to the new location.\n"
    "      - ``ABSOLUTE`` Make all paths absolute on writing.\n"
    "\n"
    "   :type path_remap: str\n"
    "   :arg fake_user: When True, data-blocks will be written with fake-user flag enabled.\n"
    "   :type fake_user: bool\n"
    "   :arg compress: When True, write a compressed blend file.\n"
    "   :type compress: bool\n");
static PyObject *bpy_lib_write(BPy_PropertyRNA *self, PyObject *args, PyObject *kw)
{
  /* args */
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};
  char filepath_abs[FILE_MAX];
  PyObject *datablocks = nullptr;

  const PyC_StringEnumItems path_remap_items[] = {
      {BLO_WRITE_PATH_REMAP_NONE, "NONE"},
      {BLO_WRITE_PATH_REMAP_RELATIVE, "RELATIVE"},
      {BLO_WRITE_PATH_REMAP_RELATIVE_ALL, "RELATIVE_ALL"},
      {BLO_WRITE_PATH_REMAP_ABSOLUTE, "ABSOLUTE"},
      {0, nullptr},
  };
  PyC_StringEnum path_remap = {path_remap_items, BLO_WRITE_PATH_REMAP_NONE};

  bool use_fake_user = false, use_compress = false;

  static const char *_keywords[] = {
      "filepath",
      "datablocks",
      "path_remap",
      "fake_user",
      "compress",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `filepath` */
      "O!" /* `datablocks` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `path_remap` */
      "O&" /* `fake_user` */
      "O&" /* `compress` */
      ":write",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseUnicodeAsBytesAndSize,
                                        &filepath_data,
                                        &PySet_Type,
                                        &datablocks,
                                        PyC_ParseStringEnum,
                                        &path_remap,
                                        PyC_ParseBool,
                                        &use_fake_user,
                                        PyC_ParseBool,
                                        &use_compress))
  {
    return nullptr;
  }

  Main *bmain_src = static_cast<Main *>(self->ptr->data); /* Typically #G_MAIN */
  int write_flags = 0;

  if (use_compress) {
    write_flags |= G_FILE_COMPRESS;
  }

  STRNCPY(filepath_abs, filepath_data.value);
  Py_XDECREF(filepath_data.value_coerce);

  BLI_path_abs(filepath_abs, BKE_main_blendfile_path_from_global());

  PartialWriteContext partial_write_ctx{bmain_src->filepath};
  const PartialWriteContext::IDAddOptions add_options{
      (PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES |
       PartialWriteContext::IDAddOperations(
           use_fake_user ? PartialWriteContext::IDAddOperations::SET_FAKE_USER : 0))};

  if (PySet_GET_SIZE(datablocks) > 0) {
    PyObject *it = PyObject_GetIter(datablocks);
    PyObject *key;
    while ((key = PyIter_Next(it))) {
      /* Borrow from the set. */
      Py_DECREF(key);
      ID *id;
      if (!pyrna_id_FromPyObject(key, &id)) {
        PyErr_Format(PyExc_TypeError, "Expected an ID type, not %.200s", Py_TYPE(key)->tp_name);
        break;
      }
      partial_write_ctx.id_add(id, add_options, nullptr);
    }
    Py_DECREF(it);
    if (key) {
      return nullptr;
    }
  }

  BLI_assert(partial_write_ctx.is_valid());

  /* write blend */
  ReportList reports;

  BKE_reports_init(&reports, RPT_STORE);
  bool success = partial_write_ctx.write(
      filepath_abs, write_flags, path_remap.value_found, reports);

  PyObject *py_return_value;
  if (success) {
    BKE_reports_print(&reports, RPT_ERROR_ALL);
    py_return_value = Py_None;
    Py_INCREF(py_return_value);
  }
  else {
    if (BPy_reports_to_error(&reports, PyExc_IOError, false) == 0) {
      PyErr_SetString(PyExc_IOError, "Unknown error writing library data");
    }
    py_return_value = nullptr;
  }

  BKE_reports_free(&reports);

  return py_return_value;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

PyMethodDef BPY_library_write_method_def = {
    "write",
    (PyCFunction)bpy_lib_write,
    METH_VARARGS | METH_KEYWORDS,
    bpy_lib_write_doc,
};

PyMethodDef BPY_library_modify_external_method_def = {
    "modify_external",
    (PyCFunction)bpy_lib_modify_external,
    METH_VARARGS | METH_KEYWORDS,
    bpy_lib_modify_external_doc,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif
