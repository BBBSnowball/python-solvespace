#ifndef SLVS_PYTHON_
#define SLVS_PYTHON_

#include <string>
#include <exception>

#include <stdio.h>
#include <stdarg.h>

class str_exception : public std::exception {
protected:
    std::string _what;

    explicit str_exception(const std::string& what) : _what(what) { }
    explicit str_exception() { }
public:
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

class invalid_value_exception : public str_exception {
public:
    explicit invalid_value_exception(const std::string& what)
        : str_exception(what) { }
    explicit invalid_value_exception(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        char* buf;
        if (vasprintf(&buf, fmt, args) >= 0) {
            _what = std::string(buf);
            free(buf);
        } else {
            // some error occurred, so we do the best we can
            _what = std::string(fmt);
        }

        va_end(args);
    }
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


    Slvs_hParam GetHandle() {
        if (sys == NULL)
            throw invalid_state_exception("param is virtual");
        return h;
    }
    Slvs_hParam handle() { return GetHandle(); }

    double GetValue() ;
    void SetValue(double v);
    System* GetSystem() {
        if (!sys)
            throw invalid_state_exception("system is NULL");
        return sys;
    }

    Slvs_hGroup GetGroup();
};

#define USE_DEFAULT_GROUP 0

class Entity {
protected:
    friend class System;
    friend class Workplane;

    System* sys;
    Slvs_hEntity h;

    Slvs_Entity* entity();
    Param param(int i) { return Param(sys, entity()->param[i]); }
    Entity fromHandle(Slvs_hEntity handle) { return Entity(sys, handle); }

    Entity() : sys(NULL), h(0) { }
    void init(const Entity& e) {
        sys = e.sys;
        h   = e.h;
    }
    // uses add_entity_with_next_handle
    void init(System* sys, Slvs_Entity e);
public:
    Entity(System* system, Slvs_hEntity handle)
        : sys(system),   h(handle)   { }
    Entity(const Entity& e)
        : sys(e.sys), h(e.h) { }

    Slvs_hEntity GetHandle() { return h; }
    Slvs_hEntity handle() { return GetHandle(); }

    System* system() { return sys; }

    Slvs_hGroup GetGroup() { return entity()->group; }
};

class Point3d : public Entity {
    friend class System;
public:
    Point3d(const Entity& e) : Entity(e) { }
    Point3d(Param x, Param y, Param z,
            System* system = NULL, Slvs_hGroup group = USE_DEFAULT_GROUP) {
        if (!system)
            system = x.GetSystem();
        x.prepareFor(system, group);
        y.prepareFor(system, group);
        z.prepareFor(system, group);
        init(system,
            Slvs_MakePoint3d(0, group, x.handle(), y.handle(), z.handle()));
    }

    Param x() { return param(0); }
    Param y() { return param(1); }
    Param z() { return param(2); }
};

class Normal3d : public Entity {
public:
    Normal3d(const Entity& e) : Entity(e) { }
    Normal3d(Param qw, Param qx, Param qy, Param qz,
            System* system = NULL,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        if (!system)
            system = qw.GetSystem();
        qw.prepareFor(system, group);
        qx.prepareFor(system, group);
        qy.prepareFor(system, group);
        qz.prepareFor(system, group);
        init(system,
            Slvs_MakeNormal3d(0, group, qw.handle(), qx.handle(), qy.handle(), qz.handle()));
    }

    Param qw() { return param(0); }
    Param qx() { return param(1); }
    Param qy() { return param(2); }
    Param qz() { return param(3); }
};

class Workplane : public Entity {
    friend class System;
    Workplane() : Entity(NULL, SLVS_FREE_IN_3D) { }
public:
    Workplane(const Entity& e) : Entity(e) { }
    Workplane(Point3d origin, Normal3d normal,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeWorkplane(0, group, origin.handle(), normal.handle());
        init(origin.system(), e);
    }

    static Workplane FreeIn3D;

    static Workplane forEntity(Entity* e) {
        return Workplane(Entity(e->sys, e->entity()->wrkpl));
    }

    Point3d  origin() { return Point3d( fromHandle(entity()->point[0])); }
    Normal3d normal() { return Normal3d(fromHandle(entity()->normal  )); }
};

Workplane Workplane::FreeIn3D = Workplane();

class Point2d : public Entity {
    friend class System;
    Point2d(const Entity& e) : Entity(e) { }
public:
    Point2d(Workplane workplane, Param u,
            Param v, System* system = NULL, Slvs_hGroup group = USE_DEFAULT_GROUP) {
        if (!system)
            system = workplane.system();
        u.prepareFor(system, group);
        v.prepareFor(system, group);
        init(system,
            Slvs_MakePoint2d(0, group, workplane.handle(),
                u.handle(), v.handle()));
    }

    Param u() { return param(0); }
    Param v() { return param(1); }
    Workplane workplane() { return Workplane::forEntity(this); }
};

class LineSegment3d : public Entity {
public:
    LineSegment3d(Point3d a, Point3d b, Workplane wrkpl = Workplane::FreeIn3D,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeLineSegment(0, group, wrkpl.handle(), a.handle(), b.handle());
        init(a.system(), e);
    }
};

class LineSegment2d : public Entity {
public:
    LineSegment2d(Workplane wrkpl, Point2d a, Point2d b,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeLineSegment(0, group, wrkpl.handle(), a.handle(), b.handle());
        init(wrkpl.system(), e);
    }
};

#define ENABLE_SAFETY 1

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
        if (ENABLE_SAFETY) {
            for (int i=0;i<params;i++)
                if (param[i].h == p.h)
                    throw invalid_value_exception(
                        "duplicate value for param handle: %lu", p.h);
        }
        param[params++] = p;
    }

    Param add_param(Slvs_hGroup group, double val) {
        if (group == USE_DEFAULT_GROUP)
            group = default_group;
        // index starts with 0, but handle starts with 1
        int h = params+1;
        add_param(Slvs_MakeParam(h, group, val));
        return Param(this, h);
    }

    Param add_param(double val) {
        return add_param(default_group, val);
    }

private:
    void check_unique_entity_handle(Slvs_hEntity h) {
        for (int i=0;i<entities;i++)
            if (entity[i].h == h)
                throw invalid_value_exception(
                    "duplicate value for entity handle: %lu", h);
    }
    void check_group(Slvs_hGroup group) {
        if (group < 1)
            throw invalid_value_exception("invalid group: %d", group);
    }
    void check_type(int type) { /* TODO */ }
    int check_entity_handle(Slvs_hEntity h) {
        for (int i=0;i<entities;i++)
            if (entity[i].h == h)
                return entity[i].type;
        throw invalid_value_exception("invalid entity handle: %lu", h);
    }
    void check_entity_handle_workplane(Slvs_hEntity h) {
        int type = check_entity_handle(h);
        if (type != SLVS_E_WORKPLANE)
            throw invalid_value_exception("entity of handle %lu must be a workplane", h);
    }
    void check_entity_handle_point(Slvs_hEntity h) {
        int type = check_entity_handle(h);
        if (type != SLVS_E_POINT_IN_2D && type != SLVS_E_POINT_IN_3D)
            throw invalid_value_exception("entity of handle %lu must be a point", h);
    }
    void check_entity_handle_normal(Slvs_hEntity h) {
        int type = check_entity_handle(h);
        if (type != SLVS_E_NORMAL_IN_2D && type != SLVS_E_NORMAL_IN_3D)
            throw invalid_value_exception("entity of handle %lu must be a normal", h);
    }
    void check_entity_handle_distance(Slvs_hEntity h) {
        int type = check_entity_handle(h);
        if (type != SLVS_E_DISTANCE)
            throw invalid_value_exception("entity of handle %lu must be a distance", h);
    }
    void check_param_handle(Slvs_hParam h) {
        for (int i=0;i<params;i++)
            if (param[i].h == h)
                return;
        throw invalid_value_exception(
            "invalid param handle (not found in system): %lu", h);
    }
public:

    void add_entity(Slvs_Entity p) {
        if (entities >= entity_space) {
            printf("too many entities\n");
            exit(-1);
        }
        if (ENABLE_SAFETY) {
            check_unique_entity_handle(p.h);
            check_group(p.group);
            check_type(p.type);
            check_entity_handle_workplane(p.wrkpl);
            check_entity_handle_point(p.point[0]);
            check_entity_handle_point(p.point[1]);
            check_entity_handle_point(p.point[2]);
            check_entity_handle_point(p.point[3]);
            check_entity_handle_normal(p.normal);
            check_entity_handle_distance(p.distance);
            check_param_handle(p.param[0]);
            check_param_handle(p.param[1]);
            check_param_handle(p.param[2]);
            check_param_handle(p.param[3]);
        }
        entity[entities++] = p;
    }

    Entity add_entity_with_next_handle(Slvs_Entity p) {
        p.h = entities+1;
        if (p.group == USE_DEFAULT_GROUP)
            p.group = default_group;
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
            Param v, Slvs_hGroup group = USE_DEFAULT_GROUP) {
        if (group == 0)
            group = default_group;
        u.prepareFor(this, group);
        v.prepareFor(this, group);
        Entity e = add_entity_with_next_handle(
            Slvs_MakePoint2d(0, group, workplane.h, u.h, v.h));
        return Point2d(e);
    }

    Point3d add_point3d(Param x, Param y, Param z, Slvs_hGroup group = USE_DEFAULT_GROUP) {
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
    if (!system)
        throw invalid_state_exception("system is NULL!");
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

Slvs_hGroup Param::GetGroup() {
    if (!sys)
        throw invalid_state_exception("virtual param doesn't have a group");
    if (sys->param[h-1].h != h)
        throw invalid_state_exception("param not found at index (handle-1)");
    return sys->param[h-1].group;
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

void Entity::init(System* sys, Slvs_Entity e) {
    init(sys->add_entity_with_next_handle(e));
}

#endif  // defined SLVS_PYTHON_
