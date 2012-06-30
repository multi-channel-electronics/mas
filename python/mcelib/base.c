/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <Python.h>
#include <numpy/arrayobject.h>

#include <mce_library.h>
#include <mce/defaults.h>

/*
  Catch-all Python class for general MCE use.  Holds a generic C
  pointer (mce_context_t*), or an mce_param_t.
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
    0, //     (destructor)CountDict_dealloc, /* tp_dealloc */
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
    0, //     CountDict_methods,         /* tp_methods */
    0, //     CountDict_members,         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0, //     (initproc)CountDict_init,  /* tp_init */
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
  Struct and methods for converting between python sequences and u32 arrays.
*/

#define DATALET_MAX 900
typedef struct {
    int count;
    u32 data[DATALET_MAX];
} datalet;

static int datalet_decode(PyObject *o, datalet *d)
{
    // Convert a PySequence to array of u32.
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


static PyObject *mce_connect(PyObject *self, PyObject *args) {
    int device_index = -1;
    mce_context_t* mce = mcelib_create(device_index, "/etc/mce/mas.cfg", 0);

    mcecmd_open(mce);
    mcedata_open(mce);
    mceconfig_open(mce, NULL, NULL);

    ptrobj* p = PyObject_New(ptrobj, &ptrobjType);
    p->p = mce;
    return (PyObject *) p;
}


/*
  lookup
*/

static PyObject *lookup(mce_context_t *mce,
                        PyObject *ocard,
                        PyObject *oparam)
{
    ptrobj *o = ptrobj_new(NULL);
    mce_param_t *p = &o->param;
    if (PyInt_Check(ocard) && PyInt_Check(oparam)) {
        p->card.id[0] = (int)PyInt_AsLong(ocard);
        p->card.card_count = 1;
        p->param.id = (int)PyInt_AsLong(oparam);
        p->param.count = 1;
    } else {
        mcecmd_load_param(mce, p,
                          PyString_AsString(ocard),
                          PyString_AsString(oparam));
    }
    return (PyObject *)o;
}

/*
  mce_lookup
  args: (mce_context_t, card, param)
*/

static PyObject *mce_lookup(PyObject *self, PyObject *args)
{
    mce_context_t *mce;
    PyObject *card, *param;

    if (!PyArg_ParseTuple(args, "O&OO",
                          ptrobj_decode, &mce,
                          &card, &param))
        return NULL;
    return (PyObject *)lookup(mce, card, param);
}


/*
  mce_write
  args: (mce_context_t, card, param, offset, vals)
*/

static PyObject *mce_write(PyObject *self, PyObject *args)
{
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
        return NULL;
    p = (ptrobj *)lookup(mce, card, param);
    mcecmd_write_range(mce, &p->param, offset, vals.data, vals.count);
    Py_RETURN_NONE;
}

/*
  mce_read
  args: (mce_context_t, card, param, offset, count)
*/

static PyObject *mce_read(PyObject *self, PyObject *args)
{
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
        return NULL;
    p = (ptrobj*)lookup(mce, card, param);

    if (vals.count < 0)
        vals.count = p->param.param.count - offset;
    vals.count = mcecmd_read_size(&p->param, vals.count);

    mcecmd_read_range(mce, &p->param, offset, vals.data, vals.count);

    return datalet_to_list(&vals);
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
  mce_read_data
  args: (mce_context_t, card, count, dest)
*/

/* Untested!! */
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
     
    mce_acq_t acq;
    mcedata_acq_create(&acq, mce, 0, cards, -1, ramb, NULL);
    mcedata_acq_go(&acq, count);
    mcedata_acq_destroy(&acq);

    Py_RETURN_NONE;
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
