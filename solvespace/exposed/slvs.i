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

%inline %{

class System;

struct Param {
    friend class System;

    System* system;
    Slvs_hParam handle;

public:

    Param(System* system, Slvs_hParam handle) : system(system), handle(handle) { }

    // This can be used as value for a parameter to signify that a new
    // parameter should be created and used.
    static Param NEW;
    bool createNew() { return system == NEW.system && handle == NEW.handle; }

    void prepareFor(System* system, Slvs_hGroup group);
};

Param Param::NEW = Param(NULL, 0);

struct Entity {
    const System* system;
    const Slvs_hEntity handle;

    Entity(System* system, Slvs_hEntity handle)
        : system(system),   handle(handle)   { }
    Entity(const Entity& e)
        : system(e.system), handle(e.handle) { }
};

#define ENTITY(name) struct name : Entity { \
        name(const Entity& e) : Entity(e) { } \
    };
ENTITY(Point2d);
ENTITY(Workplane);

class System : public Slvs_System {
    int param_space, entity_space, constraint_space, failed_space;

    void init(int param_space, int entity_space, int constraint_space,
                int failed_space) {
        memset((Slvs_System *)this, 0, sizeof(Slvs_System));

        param      = (Slvs_Param       *) malloc(param_space      * sizeof(*param     ));
        entity     = (Slvs_Entity      *) malloc(entity_space     * sizeof(*entity    ));
        constraint = (Slvs_Constraint  *) malloc(constraint_space * sizeof(*constraint));
        failed     = (Slvs_hConstraint *) malloc(failed_space     * sizeof(*failed    ));
        faileds    = failed_space;

        this->param_space      = param_space;
        this->entity_space     = entity_space;
        this->constraint_space = constraint_space;
        this->failed_space     = failed_space;

        if(!param || !entity || !constraint || !failed) {
            printf("out of memory!\n");
            exit(-1);
        }

        default_group = 1;
    }
public:
    System(int param_space, int entity_space, int constraint_space,
                int failed_space) {
        init(param_space, entity_space, constraint_space, failed_space);
    }

    System(int space) {
        init(space, space, space, space);
    }

    System() {
        init(50, 50, 50, 50);
    }

    ~System() {
        free(param);
        free(entity);
        free(constraint);
        free(failed);
    }

    Slvs_hGroup default_group;

    void add_param(Slvs_Param p) {
        if (params >= param_space) {
            printf("too many params\n");
            exit(-1);
        }
        param[params++] = p;
    }

    Param add_param(Slvs_hGroup group, double val) {
        // index starts with 0, but handle starts with 1
        int h = params+1;
        add_param(Slvs_MakeParam(h, group, val));
        return Param(this, h);
    }

    Param add_param(double val) {
        return add_param(default_group, val);
    }

    void add_entity(Slvs_Entity p) {
        if (entities >= entity_space) {
            printf("too many entities\n");
            exit(-1);
        }
        entity[entities++] = p;
    }

    Entity add_entity_with_next_handle(Slvs_Entity p) {
        p.h = entities+1;
        add_entity(p);
        return Entity(this, p.h);
    }

    void add_constraint(Slvs_Constraint p) {
        if (constraints >= constraint_space) {
            printf("too many constraints\n");
            exit(-1);
        }
        constraint[constraints++] = p;
    }

    Slvs_Param *get_param(int i) {
        if (i >= params || i < 0)
            return NULL;
        else
            return &param[i];
    }

    void set_dragged(int i, Slvs_hParam param) {
        if (i >= 0 && i < 4)
            dragged[i] = param;
        else {
            printf("Cannot set dragged[%d]\n", i);
        }
    }

    int solve(Slvs_hGroup hg = 0) {
        if (hg == 0)
            hg = default_group;
        Slvs_Solve((Slvs_System *)this, hg);
        return result;
    }


    // entities

    Point2d add_point2d(Workplane workplane, Param u = Param::NEW,
            Param v = Param::NEW, Slvs_hGroup group = 0) {
        if (group == 0)
            group = default_group;
        u.prepareFor(this, group);
        v.prepareFor(this, group);
        Entity e = add_entity_with_next_handle(
            Slvs_MakePoint2d(0, group, workplane.handle, u.handle, v.handle));
        return Point2d(e);
    }
};

void Param::prepareFor(System* system, Slvs_hGroup group) {
    if (createNew()) {
        *this = system->add_param(group);
    } else {
        if (system != this->system) {
            //TODO use an exception
            printf("This param belongs to another system!");
            exit(-1);
        }
    }
}

%}
