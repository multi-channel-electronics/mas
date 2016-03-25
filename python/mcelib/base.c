/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

/*
  Python+numpy wrappers for libmce.
*/

#include <Python.h>
#include <numpy/arrayobject.h>

#include <mce_library.h>
#include <mce/defaults.h>

/*
  ptrobj

  Catch-all Python class for general MCE use.  Encapsulates a generic
  C pointer (often a mce_context_t*), or an mce_param_t.
*/

typedef struct {
    PyObject_HEAD
    void *p;
    mce_param_t param;
} ptrobj;

static PyTypeObject
ptrobjType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "ptrobj",                  /* tp_name */
    sizeof(ptrobj),            /* tp_basicsize */
    0,                         /* tp_itemsize */
    0,                         /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
    "ptrobj object",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

static int ptrobj_decode(PyObject *o, void **dest)
{
    *dest = ((ptrobj*)o)->p;
    return 1;
}

static int ptrobj_decode_param(PyObject *o, mce_param_t **dest)
{
    ptrobj *po = (ptrobj *)o;
    *dest = &(po->param);
    return 1;
}

static ptrobj *ptrobj_new(void *item)
{
    ptrobj* p = PyObject_New(ptrobj, &ptrobjType);
    p->p = item;
    return p;
}


/*
  Struct and methods for converting between python sequences and u32
  arrays.  This is primarily used for MCE command and reply payloads.
*/

#define DATALET_MAX 900
typedef struct {
    int count;
    u32 data[DATALET_MAX];
} datalet;

static int datalet_decode(PyObject *o, datalet *d)
{
    // Convert a PySequence to array of u32.
    // Returns 0 on failure, 1 on success.
    int i;
    int n = PySequence_Length(o);
    if (n >= DATALET_MAX)
        goto fail;
    d->count = n;
    for (i=0; i<n; i++) {
        PyObject *x = PySequence_GetItem(o, i);
        long y = PyInt_AsLong(x);
        d->data[i] = (u32)y;
    }
    return 1;
fail:
    return 0;
}

static PyObject* datalet_to_list(const datalet *d)
{
    int i;
    PyObject *o = PyList_New(d->count);
    for (i=0; i<d->count; i++)
        PyList_SetItem(o, i, PyInt_FromLong(d->data[i]));
    return o;
}


/*
  Uh, you can test things using this trace function.
*/

static PyObject *trace(PyObject *self, PyObject *args)
{
    PyArrayObject *array;
    double sum;
    int i, n;

    if (!PyArg_ParseTuple(args, "O!",
                          &PyArray_Type, &array))
        return NULL;
    if (array->nd != 2 || array->descr->type_num != PyArray_DOUBLE) {
        PyErr_SetString(PyExc_ValueError,
                        "array must be two-dimensional and of type float");
        return NULL;
    }

    n = array->dimensions[0];
    if (n > array->dimensions[1])
        n = array->dimensions[1];

    sum = 0.;
    for (i = 0; i < n; i++)
        sum += *(double *)(array->data + i*array->strides[0] + i*array->strides[1]);

    return PyFloat_FromDouble(sum);
}

/*
  MCE stuff
*/

/*
  mce_connect()

  Make a connection, return an mce_context_t.
*/

static PyObject *mce_connect(PyObject *self, PyObject *args) {
    int err;
    int device_index = MCE_DEFAULT_MCE;
    mce_context_t* mce = mcelib_create(device_index, "/etc/mce/mas.cfg", 0);
    if (mce==NULL) {
        fprintf(stderr, "pymce: failed to create.\n");
        Py_RETURN_NONE;
    }
    if ((err=mcecmd_open(mce)) != 0) {
        fprintf(stderr, "pymce: failed to open commander [err=%i].\n",
                err);
        Py_RETURN_NONE;
    }
        
    if ((err=mcedata_open(mce)) != 0) {
        fprintf(stderr, "pymce: failed to open data device [err=%i].\n",
                err);
    }
    if ((err=mceconfig_open(mce, NULL, NULL)) != 0) {
        fprintf(stderr, "pymce: failed to open config module [err=%i].\n",
                err);
    }

    ptrobj* p = PyObject_New(ptrobj, &ptrobjType);
    p->p = mce;
    return (PyObject *) p;
}


/*
  lookup

  This is just a helper for read, write, etc.  "ocard" and "oparam"
  must both be integers (corresponding to physical card+param), or
  both be strings (corresponding to whatever).

  Returns ptrobj(mce_param_t) on success, NULL on failure.
*/

static PyObject *lookup(mce_context_t *mce,
                        PyObject *ocard,
                        PyObject *oparam)
{
    ptrobj *o = ptrobj_new(NULL);
    mce_param_t *p = &o->param;
    // If these are both integers, 
    if (PyInt_Check(ocard) && PyInt_Check(oparam)) {
        p->card.id[0] = (int)PyInt_AsLong(ocard);
        p->card.card_count = 1;
        p->param.id = (int)PyInt_AsLong(oparam);
        p->param.count = 1;
    } else {
        if (mcecmd_load_param(mce, p,
                              PyString_AsString(ocard),
                              PyString_AsString(oparam)) != 0) {
            Py_DECREF(o);
            return NULL;
        }
    }
    return (PyObject *)o;
}

/*
  mce_lookup(context, card, param)

  Returns a ptrobj(mce_param_t), or None on failure.
*/

static PyObject *mce_lookup(PyObject *self, PyObject *args)
{
    mce_context_t *mce;
    PyObject *card, *param;

    if (!PyArg_ParseTuple(args, "O&OO",
                          ptrobj_decode, &mce,
                          &card, &param))
        Py_RETURN_NONE;

    PyObject *p = lookup(mce, card, param);
    if (p == NULL)
        Py_RETURN_NONE;
    return p;
}


/*
  mce_write(context, card, param, offset, vals)

  Returns True on success, False on failure.
*/

static PyObject *mce_write(PyObject *self, PyObject *args)
{
    int err;
    mce_context_t *mce;
    int offset;
    datalet vals;
    ptrobj *p;
    PyObject *card, *param;
    if (!PyArg_ParseTuple(args, "O&OOiO&",
                          ptrobj_decode, &mce,
                          &card,
                          &param,
                          &offset,
                          datalet_decode, &vals))
        Py_RETURN_FALSE;
    p = (ptrobj *)lookup(mce, card, param);
    if (p == NULL)
        Py_RETURN_FALSE;
    
    err = mcecmd_write_range(mce, &p->param, offset, vals.data, vals.count);
    Py_DECREF(p);
        
    if (err==0)
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

/*
  mce_read(context, card, param, offset, count)

  Returns list of data on success, None on failure.
*/

static PyObject *mce_read(PyObject *self, PyObject *args)
{
    int err = 0;
    mce_context_t *mce;
    PyObject *card, *param;
    ptrobj *p;
    int offset;
    datalet vals;

    if (!PyArg_ParseTuple(args, "O&OOii",
                          ptrobj_decode, &mce,
                          &card, &param,
                          &offset,
                          &vals.count))
        Py_RETURN_NONE;
    p = (ptrobj*)lookup(mce, card, param);
    if (p == NULL)
        Py_RETURN_NONE;
    if (vals.count < 0)
        vals.count = p->param.param.count - offset;
    vals.count = mcecmd_read_size(&p->param, vals.count);

    err = mcecmd_read_range(mce, &p->param, offset, vals.data, vals.count);
    Py_DECREF(p);

    if (err==0)
        return datalet_to_list(&vals);
    Py_RETURN_NONE;
}

typedef struct {
    int index;
    u32 *buf;
/*      int select; */
/*      u32 *channels; */
/*      int n_channels; */
} frame_handler_t;

static int frame_callback(unsigned long user_data, int size, u32 *data)
{
    frame_handler_t *f = (frame_handler_t*)user_data;
    memcpy(f->buf + f->index * size, data, size*sizeof(u32));
    f->index++;
    return 0;
}



/*
  mce_read_data(context, cards, count, dest)

  Reads count frames of data from cards.  Stores them in numpy array
  dest, which sure better be the right size.

  Returns True on success, False on failure.
*/

static PyObject *mce_read_data(PyObject *self, PyObject *args)
{
    mce_context_t *mce;
    int cards, count;
    PyArrayObject *array;

    if (!PyArg_ParseTuple(args, "O&iiO!",
                          ptrobj_decode, &mce,
                          &cards, &count,
                          &PyArray_Type, &array))
        return NULL;

    // Note how this totally doesn't do any array size checking.
    frame_handler_t f;
    f.buf = (void*)array->data;
    f.index = 0;
    mcedata_storage_t *ramb = mcedata_rambuff_create( frame_callback,
                                                      (unsigned long)&f );
     
    int err = 0;
    mce_acq_t acq;
    err = mcedata_acq_create(&acq, mce, 0, cards, -1, ramb);
    if (err != 0) goto fail;
    err = mcedata_acq_go(&acq, count);
    if (err != 0) goto fail;
    err = mcedata_acq_destroy(&acq);
    if (err != 0) goto fail;

    Py_RETURN_TRUE;

fail:
    Py_RETURN_FALSE;    
}



/*
 * Get / set / reset the driver data lock.
 *
 * Arguments are an mce_context_t and an integer.
 *
 *   0: query lock state (returns True if lock available)
 *   1: get lock (returns True if lock got)
 *   2: release lock (returns True always)
 *   3: reset lock (returns True always)
 */

static PyObject *lock_op(PyObject *self, PyObject *args)
{
    int err = 0;
    mce_context_t *mce;
    int operation;
    if (!PyArg_ParseTuple(args, "O&i",
                          ptrobj_decode, &mce,
                          &operation))
        Py_RETURN_FALSE;

    switch(operation) {
    case 0:
        err = mcedata_lock_query(mce);
        break;
    case 1:
        err = mcedata_lock_down(mce);
        break;
    case 2:
        mcedata_lock_up(mce);
        break;
    case 3:
        mcedata_lock_reset(mce);
    default:
        err = 1;
    }

    if (err==0)
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}


static PyMethodDef mceMethods[] = {
    {"trace",  trace, METH_VARARGS,
     "Return the trace of a matrix."},
    {"connect",  mce_connect, METH_VARARGS,
     "Connect."},
    {"lookup", mce_lookup, METH_VARARGS,
     "Lookup."},
    {"write",  mce_write, METH_VARARGS,
     "Write."},
    {"read",  mce_read, METH_VARARGS,
     "Read."},
    {"read_data",  mce_read_data, METH_VARARGS,
     "Read data."},
    {"lock_op", lock_op, METH_VARARGS,
     "Driver data lock operations."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
initmcelib(void)
{
    Py_InitModule("mcelib", mceMethods);
    ptrobjType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&ptrobjType) < 0)
        return;
    Py_INCREF(&ptrobjType);
    import_array();
}
