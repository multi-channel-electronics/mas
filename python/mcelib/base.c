#include <Python.h>
#include <numpy/arrayobject.h>

#include <mce_library.h>
#include <mce/defaults.h>

/*
  Type for holding a C pointer.  Used for mce_context.
*/

typedef struct {
     PyObject_HEAD
     void *p;
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

int ptrobj_decode(PyObject *o, void **dest)
{
     *dest = ((ptrobj*)o)->p;
     return 1;
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
     mce_context_t* mce = mcelib_create(device_index);

     mcecmd_open(mce, mcelib_cmd_device(device_index));
     mcedata_open(mce, mcelib_data_device(device_index));
     mceconfig_open(mce, mcelib_default_hardwarefile(device_index), "hardware");

     ptrobj* p = PyObject_New(ptrobj, &ptrobjType);
     p->p = mce;
     return (PyObject *) p;
}


/*
  mce_write
    args: (mce_context_t, card, param, offset, vals)
*/

static PyObject *mce_write(PyObject *self, PyObject *args)
{
     mce_context_t *mce;
     char *card, *param;
     int offset;
     datalet vals;

     if (!PyArg_ParseTuple(args, "O&ssiO&",
			   ptrobj_decode, &mce,
			   &card, &param,
			   &offset,
			   datalet_decode, &vals))
	  return NULL;

     mce_param_t p;
     mcecmd_load_param(mce, &p, card,  param);

     mcecmd_write_range(mce, &p, offset, vals.data, vals.count);

     return PyFloat_FromDouble(0.);
}

/*
  mce_read
    args: (mce_context_t, card, param, offset, count)
*/

static PyObject *mce_read(PyObject *self, PyObject *args)
{
     mce_context_t *mce;
     char *card, *param;
     int offset;
     datalet vals;

     if (!PyArg_ParseTuple(args, "O&ssii",
			   ptrobj_decode, &mce,
			   &card, &param,
			   &offset,
			   &vals.count))
	  return NULL;

     mce_param_t p;
     mcecmd_load_param(mce, &p, card,  param);

     if (vals.count < 0)
	  vals.count = p.param.count - offset;
     vals.count = mcecmd_read_size(&p, vals.count);

     mcecmd_read_range(mce, &p, offset, vals.data, vals.count);

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
     char *card;
     int count;
     PyArrayObject *array;

     if (!PyArg_ParseTuple(args, "O&siO!",
			   ptrobj_decode, &mce,
			   &card,
			   &count,
			   &PyArray_Type, &array))
	  return NULL;

     // Numpy object
     printf("%i\n", array->nd);

     frame_handler_t f;
     f.buf = (void*)array->data;
     f.index = 0;
     mcedata_storage_t *ramb = mcedata_rambuff_create( frame_callback, (unsigned long)&f );
     
     mce_acq_t acq;
     mcedata_acq_create(&acq, mce, 0, -1, -1, ramb);
     mcedata_acq_go(&acq, count);
     mcedata_acq_destroy(&acq);
     return NULL;
}





static PyMethodDef mceMethods[] = {
     {"trace",  trace, METH_VARARGS,
     "Return the trace of a matrix."},
    {"connect",  mce_connect, METH_VARARGS,
     "Connect."},
    {"write",  mce_write, METH_VARARGS,
     "Write."},
    {"read",  mce_read, METH_VARARGS,
     "Read."},
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
