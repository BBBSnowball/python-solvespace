%module slvs

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

    // and one more: a few helpers to make the API more pythonic
    // (Well, actually it grew to be a bit more than 'a few' *g*)
#   include "slvs_python.hpp"
%}


// Before we include anything that is visible by SWIG, we must set the
// table. This means we tell SWIG how it should map certain types.

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

/*%exception System::System {
   try {
      $action
   } catch (out_of_memory_exception &e) {
      PyErr_SetString(PyExc_MemoryError, const_cast<char*>(e.what()));
      return NULL;
   } catch (not_enough_space_exception &e) {

   }
}*/

%include "exception.i"

%typemap(throws) out_of_memory_exception {
    SWIG_exception(SWIG_MemoryError, const_cast<char*>($1.what()));
}

%typemap(throws) not_enough_space_exception {
    SWIG_exception(SWIG_RuntimeError, const_cast<char*>($1.what()));
}

%typemap(throws) wrong_system_exception {
    SWIG_exception(SWIG_RuntimeError, const_cast<char*>($1.what()));
}

%typemap(throws) invalid_state_exception {
    SWIG_exception(SWIG_RuntimeError, const_cast<char*>($1.what()));
}

%typemap(throws) invalid_value_exception {
    SWIG_exception(SWIG_RuntimeError, const_cast<char*>($1.what()));
}

/*
%except(python) {
    try {
        $function
    } catch (invalid_value_exception& e) {
        SWIG_exception(SWIG_RuntimeError, const_cast<char*>(e.what()));
    }
}
*/

// %ignore Param::prepareFor;
// %ignore Entity::Entity;

// %include "slvs_python.hpp"

class Param {
public:
    // This can be used as value for a parameter to signify that a new
    // parameter should be created and used.
    Param(double value);

    Slvs_hParam GetHandle() throw(invalid_state_exception);
    Slvs_hGroup GetGroup()  throw(invalid_state_exception);
    double GetValue()       throw(invalid_state_exception);
    void SetValue(double v) throw(invalid_state_exception);

    // see http://stackoverflow.com/a/4750081
    %pythoncode %{
        __swig_getmethods__["handle"] = GetHandle
        if _newclass: handle = property(GetHandle)

        __swig_getmethods__["group"] = GetHandle
        if _newclass: group = property(GetGroup)

        __swig_getmethods__["value"] = GetValue
        __swig_setmethods__["value"] = SetValue
        if _newclass: value = property(GetValue, SetValue)
    %}
};

class Entity {
private: Entity();
public:
    Slvs_hEntity GetHandle();
    Slvs_hGroup  GetGroup()  throw(invalid_state_exception);

    %pythoncode %{
        __swig_getmethods__["handle"] = GetHandle
        if _newclass: handle = property(GetHandle)

        __swig_getmethods__["group"] = GetHandle
        if _newclass: group = property(GetGroup)
    %}
};

#define throw_entity_constructor \
    throw(wrong_system_exception, invalid_state_exception, invalid_value_exception)

class Point3d : public Entity {
public:
    Point3d(Param x, Param y, Param z,
            System* system = NULL,
            Slvs_hGroup group = USE_DEFAULT_GROUP)
        throw_entity_constructor;

    Param x() throw(invalid_state_exception);
    Param y() throw(invalid_state_exception);
    Param z() throw(invalid_state_exception);
};

class Normal3d : public Entity {
public:
    Normal3d(Param qw, Param qx, Param qy, Param qz,
            System* system = NULL,
            Slvs_hGroup group = USE_DEFAULT_GROUP)
        throw_entity_constructor;

    Param qw() throw(invalid_state_exception);
    Param qx() throw(invalid_state_exception);
    Param qy() throw(invalid_state_exception);
    Param qz() throw(invalid_state_exception);
};

class Workplane : public Entity {
public:
    static Workplane FreeIn3D;

    Workplane(Point3d origin, Normal3d normal,
            Slvs_hGroup group = USE_DEFAULT_GROUP)
        throw_entity_constructor;

    Point3d  origin() throw(invalid_state_exception);
    Normal3d normal() throw(invalid_state_exception);
};

class Point2d : public Entity {
public:
    Point2d(Workplane workplane, Param u,
            Param v, System* system = NULL,
            Slvs_hGroup group = USE_DEFAULT_GROUP)
        throw_entity_constructor;

    Param u();
    Param v();
    Workplane workplane();
};

class LineSegment3d : public Entity {
public:
    LineSegment3d(Point3d a, Point3d b, Workplane wrkpl = Workplane::FreeIn3D,
            Slvs_hGroup group = USE_DEFAULT_GROUP)
        throw_entity_constructor;
};

class LineSegment2d : public Entity {
public:
    LineSegment2d(Workplane wrkpl, Point2d a, Point2d b,
            Slvs_hGroup group = USE_DEFAULT_GROUP)
        throw_entity_constructor;
};

class System : public Slvs_System {
public:
    System(int param_space, int entity_space, int constraint_space,
                int failed_space);

    System(int space);

    System();

    ~System();

    Slvs_hGroup default_group;

    void add_param(Slvs_Param p) throw(invalid_value_exception);

    Param add_param(Slvs_hGroup group, double val) throw(invalid_value_exception);

    Param add_param(double val) throw(invalid_value_exception);

    void add_entity(Slvs_Entity p) throw(invalid_value_exception);

    Entity add_entity_with_next_handle(Slvs_Entity p) throw(invalid_value_exception);

    void add_constraint(Slvs_Constraint p) throw(invalid_value_exception);

    Slvs_Param *get_param(int i);

    void set_dragged(int i, Slvs_hParam param);

    int solve(Slvs_hGroup hg = 0);


    // entities

    Point2d add_point2d(Workplane workplane, Param u,
            Param v, Slvs_hGroup group = 0)
        throw_entity_constructor;

    Point3d add_point3d(Param x, Param y, Param z,
            Slvs_hGroup group = 0)
        throw_entity_constructor;
};
