%module slvs

#if defined(SWIGPYTHON)

// Some quaterion functions returns values with call-by-reference, so
// we have to tell SWIG to do the right thing.
// example: DLL void Slvs_QuaternionU(double qw, ...,
//                             double *x, double *y, double *z);
// for a more complex example, see http://stackoverflow.com/a/6180075

// The user should have to pass any argument (numinputs=0) and we pass
// a pointer to a temporary variable to store the value in.
%typemap(in, numinputs=0) double * (double temp) {
   $1 = &temp;
}

// We get the value, wrap it in a PyFloat and add it to the output. The
// user can use it like this:
// x, y, z = Slvs_QuaternionU(...)
%typemap(argout) double * {
    PyObject * fl = PyFloat_FromDouble(*$1);
    if (!fl) SWIG_fail;
    $result = SWIG_Python_AppendOutput($result, fl);
}

#endif


%ignore param;

%include "slvs.h"

//NOTE We must put it into the %begin section (instead of %header) because
//     it must be available for the stuff in %extends and that is put
//     directly before the header section...
%begin %{
    // include Python first because it would complain about redefined
    // symbols
#   include <Python.h>

    // we need malloc and memset
#   include <string.h>
#   include <stdlib.h>

    // finally, we include slvs - the library we want to wrap
#   include "slvs.h"
%}


%extend Slvs_System {
    Slvs_System() {
        Slvs_System* s = new Slvs_System();
        memset(s, 0, sizeof(*s));
        s->param      = (Slvs_Param       *) malloc(50*sizeof(*s->param     ));
        s->entity     = (Slvs_Entity      *) malloc(50*sizeof(*s->entity    ));
        s->constraint = (Slvs_Constraint  *) malloc(50*sizeof(*s->constraint));
        s->failed     = (Slvs_hConstraint *) malloc(50*sizeof(*s->failed    ));
        s->faileds    = 50;

        if(!s->param || !s->entity || !s->constraint || !s->failed) {
            printf("out of memory!\n");
            exit(-1);
        }

        return s;
    }

    ~Slvs_System() {
        free($self->param);
        free($self->entity);
        free($self->constraint);
        free($self->failed);
        delete $self;
    }

    int add_param(Slvs_Param p) {
        int i = $self->params;
        $self->param[$self->params++] = p;
        return i;
    }

    int add_entity(Slvs_Entity p) {
        int i = $self->entities;
        $self->entity[$self->entities++] = p;
        return i;
    }

    int add_constraint(Slvs_Constraint p) {
        int i = $self->constraints;
        $self->constraint[$self->constraints++] = p;
        return i;
    }

    Slvs_Param *get_param(int i) {
        if (i >= $self->params || i < 0)
            return NULL;
        else
            return &$self->param[i];
    }

    void set_dragged(int i, Slvs_hParam param) {
        if (i >= 0 && i < 4)
            $self->dragged[i] = param;
        else {
            printf("Cannot set $self->dragged[%d]\n", i);
        }
    }
};
