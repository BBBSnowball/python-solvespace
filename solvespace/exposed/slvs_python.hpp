#ifndef SLVS_PYTHON_
#define SLVS_PYTHON_

#include <string>
#include <exception>

class str_exception : public std::exception {
    std::string _what;
public:
    explicit str_exception(const std::string& what) : _what(what) { }
    virtual ~str_exception() throw() {}

    virtual const char* what() const throw() {
        return _what.c_str();
    }
};

class out_of_memory_exception : public str_exception {
public:
    explicit out_of_memory_exception(const std::string& what)
        : str_exception(what) { }
};

class not_enough_space_exception : public str_exception {
public:
    explicit not_enough_space_exception(const std::string& what)
        : str_exception(what) { }
};

class wrong_system_exception : public str_exception {
public:
    explicit wrong_system_exception(const std::string& what)
        : str_exception(what) { }
};

class invalid_state_exception : public str_exception {
public:
    explicit invalid_state_exception(const std::string& what)
        : str_exception(what) { }
};

//NOTE This header doesn't follow usual C++ guidelines (e.g. seperate
//     declaration and implementation) because it is only used for the
//     Python library.

class System;

class Param {
    friend class System;
    friend class Entity;

    System* sys;
    // We either 
    union {
        Slvs_hParam h;
        double init_value;
    };

    Param(System* system, Slvs_hParam handle) : sys(system), h(handle) { }
public:
    // This can be used as value for a parameter to signify that a new
    // parameter should be created and used.
    Param(double value) : sys(NULL), init_value(value) { }

    // Makes sure the param belongs to the system or is a dummy param
    // (only a value), in which case it will create a real one
    void prepareFor(System* system, Slvs_hGroup group);


    int GetHandle() {
        if (sys == NULL)
            throw invalid_state_exception("param is virtual");
        return h;
    }

    double GetValue() ;
    void SetValue(double v);
};

class Entity {
protected:
    friend class System;

    System* sys;
    Slvs_hEntity h;

    Slvs_Entity* entity();
    Param param(int i) { return Param(sys, entity()->param[i]); }
public:
    Entity(System* system, Slvs_hEntity handle)
        : sys(system),   h(handle)   { }
    Entity(const Entity& e)
        : sys(e.sys), h(e.h) { }
};

#define ENTITY(name) struct name : public Entity { \
        friend class System; \
        private: name(const Entity& e) : Entity(e) { } \
    };
class Point3d : public Entity {
    friend class System;
    Point3d(const Entity& e) : Entity(e) { }
public:
    Param x() { return param(0); }
    Param y() { return param(1); }
    Param z() { return param(2); }
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
            throw out_of_memory_exception("out of memory!");
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

    Point2d add_point2d(Workplane workplane, Param u,
            Param v, Slvs_hGroup group = 0) {
        if (group == 0)
            group = default_group;
        u.prepareFor(this, group);
        v.prepareFor(this, group);
        Entity e = add_entity_with_next_handle(
            Slvs_MakePoint2d(0, group, workplane.h, u.h, v.h));
        return Point2d(e);
    }

    Point3d add_point3d(Param x, Param y, Param z, Slvs_hGroup group = 0) {
        if (group == 0)
            group = default_group;
        x.prepareFor(this, group);
        y.prepareFor(this, group);
        z.prepareFor(this, group);
        Entity e = add_entity_with_next_handle(
            Slvs_MakePoint3d(0, group, x.h, y.h, z.h));
        return Point3d(e);
    }
};

void Param::prepareFor(System* system, Slvs_hGroup group) {
    if (!sys) {
        *this = system->add_param(group, this->init_value);
    } else {
        if (system != this->sys) {
            throw wrong_system_exception(
                "This param belongs to another system!");
        }
    }
}


double Param::GetValue() {
    if (sys) {
        if (sys->param[h-1].h != h)
            throw invalid_state_exception("param not found at index (handle-1)");
        return sys->param[h-1].val;
    } else
        return init_value;
}
void Param::SetValue(double v) {
    if (sys) {
        if (sys->param[h-1].h != h)
            throw invalid_state_exception("param not found at index (handle-1)");
        sys->param[h-1].val = v;
    } else
        init_value = v;
}

Slvs_Entity* Entity::entity() {
    if (sys) {
        if (sys->entity[h-1].h != h)
            throw invalid_state_exception("entity not found at index (handle-1)");
        return &sys->entity[h-1];
    } else {
        throw invalid_state_exception("invalid system");
    }
}

#endif  // defined SLVS_PYTHON_
