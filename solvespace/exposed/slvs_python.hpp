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
    void prepareFor(System* system, Slvs_hGroup group)
        throw(wrong_system_exception, invalid_state_exception);


    Slvs_hParam GetHandle() throw(invalid_state_exception) {
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
    friend class Constraint;

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

#define throw_entity_constructor \
    throw(wrong_system_exception, invalid_state_exception, invalid_value_exception)

class Point : public Entity {
protected:
    Point() { }
public:
    explicit Point(const Entity& e) : Entity(e) { }
};

class Point3d : public Point {
    friend class System;
public:
    explicit Point3d(const Entity& e) : Point(e) { }
    Point3d(Param x, Param y, Param z,
            System* system = NULL, Slvs_hGroup group = USE_DEFAULT_GROUP)
            throw_entity_constructor {
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

class Workplane;

class Normal3d : public Entity {
public:
    explicit Normal3d(const Entity& e) : Entity(e) { }
    Normal3d(Param qw, Param qx, Param qy, Param qz,
            System* system = NULL,
            Slvs_hGroup group = USE_DEFAULT_GROUP)
            throw_entity_constructor {
        if (!system)
            system = qw.GetSystem();
        qw.prepareFor(system, group);
        qx.prepareFor(system, group);
        qy.prepareFor(system, group);
        qz.prepareFor(system, group);
        init(system,
            Slvs_MakeNormal3d(0, group, qw.handle(), qx.handle(), qy.handle(), qz.handle()));
    }
    Normal3d(Workplane wrkpl, Slvs_hGroup group = USE_DEFAULT_GROUP);

    //NOTE You can either use qw, ... OR workplane!

    bool isNormalIn3D() { return entity()->type == SLVS_E_NORMAL_IN_3D; }
    Param qw() { if (isNormalIn3D()) return param(0); else throw invalid_state_exception("2d normal doesn't have qw"); }
    Param qx() { if (isNormalIn3D()) return param(1); else throw invalid_state_exception("2d normal doesn't have qx"); }
    Param qy() { if (isNormalIn3D()) return param(2); else throw invalid_state_exception("2d normal doesn't have qy"); }
    Param qz() { if (isNormalIn3D()) return param(3); else throw invalid_state_exception("2d normal doesn't have qz"); }

    bool isNormalIn2D() { return entity()->type == SLVS_E_NORMAL_IN_2D; }
    Workplane workplane();
};

class Workplane : public Entity {
    friend class System;
    Workplane() : Entity(NULL, SLVS_FREE_IN_3D) { }
public:
    explicit Workplane(const Entity& e) : Entity(e) { }
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

Normal3d::Normal3d(Workplane wrkpl, Slvs_hGroup group) {
    init(wrkpl.system(),
        Slvs_MakeNormal2d(0, group, wrkpl.handle()));
}

Workplane Normal3d::workplane(){
    if (!isNormalIn2D())
        throw invalid_state_exception("3d normal doesn't have a workplane");
    return Workplane::forEntity(this);
}

class Point2d : public Point {
    friend class System;
public:
    explicit Point2d(const Entity& e) : Point(e) { }
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

class LineSegment : public Entity {
protected:
    LineSegment() { }
public:
    explicit LineSegment(const Entity& e) : Entity(e) { }
};

class LineSegment3d : public LineSegment {
public:
    LineSegment3d(Point3d a, Point3d b, Workplane wrkpl = Workplane::FreeIn3D,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeLineSegment(0, group, wrkpl.handle(), a.handle(), b.handle());
        init(a.system(), e);
    }

    Point3d a() { return Point3d( fromHandle(entity()->point[0])); }
    Point3d b() { return Point3d( fromHandle(entity()->point[1])); }
    //Workplane workplane() { return Workplane::forEntity(this); }
};

class LineSegment2d : public LineSegment {
public:
    LineSegment2d(Workplane wrkpl, Point2d a, Point2d b,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeLineSegment(0, group, wrkpl.handle(), a.handle(), b.handle());
        init(wrkpl.system(), e);
    }

    Point2d a() { return Point2d( fromHandle(entity()->point[0])); }
    Point2d b() { return Point2d( fromHandle(entity()->point[1])); }
    Workplane workplane() { return Workplane::forEntity(this); }
};

class ArcOfCircle : public Entity {
public:
    ArcOfCircle(Workplane wrkpl, Normal3d normal,
            Point2d center, Point2d start, Point2d end,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeArcOfCircle(0, group, wrkpl.handle(), normal.handle(), center.handle(), start.handle(), end.handle());
        init(wrkpl.system(), e);
    }

    Point2d  center() { return Point2d( fromHandle(entity()->point[0])); }
    Point2d  start()  { return Point2d( fromHandle(entity()->point[1])); }
    Point2d  end()    { return Point2d( fromHandle(entity()->point[2])); }
    Normal3d normal() { return Normal3d(fromHandle(entity()->normal  )); }
    Workplane workplane() { return Workplane::forEntity(this); }
};

class Distance : public Entity {
public:
    explicit Distance(const Entity& e) : Entity(e) { }
    Distance(Workplane wrkpl, Param distance,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        System* system = wrkpl.system();
        distance.prepareFor(system, group);
        Slvs_Entity e = Slvs_MakeDistance(0, group, wrkpl.handle(), distance.handle());
        init(wrkpl.system(), e);
    }

    Param     distance()  { return param(0); }
    Workplane workplane() { return Workplane::forEntity(this); }
};

class Circle : public Entity {
public:
    Circle(Workplane wrkpl, Normal3d normal,
            Point2d center, Distance radius,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeCircle(0, group, wrkpl.handle(), center.handle(), normal.handle(), radius.handle());
        init(wrkpl.system(), e);
    }

    Point2d   center()    { return Point2d( fromHandle(entity()->point[0])); }
    Distance  distance()  { return Distance(fromHandle(entity()->distance)); }
    Normal3d  normal()    { return Normal3d(fromHandle(entity()->normal  )); }
    Workplane workplane() { return Workplane::forEntity(this); }
};

class Cubic : public Entity {
public:
    //TODO can we use it in 2d and 3d ?
    //TODO implement getters
    Cubic(Workplane wrkpl, Point2d pt0, Point2d pt1,
            Point2d pt2, Point2d pt3,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeCubic(0, group, wrkpl.handle(), pt0.handle(), pt1.handle(), pt2.handle(), pt3.handle());
        init(wrkpl.system(), e);
    }
    Cubic(Point3d pt0, Point3d pt1,
            Point3d pt2, Point3d pt3,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        Slvs_Entity e = Slvs_MakeCubic(0, group, SLVS_FREE_IN_3D, pt0.handle(), pt1.handle(), pt2.handle(), pt3.handle());
        init(pt0.system(), e);
    }
};

class Constraint {
protected:
    friend class System;

    System* sys;
    Slvs_hConstraint h;

    Slvs_Constraint* constraint();

    Constraint(System* system, Slvs_hConstraint handle)
        : sys(system), h(handle) { }

    // uses add_constraint_with_next_handle
    static Constraint init(System* sys, Slvs_Constraint c);
public:
    Slvs_hEntity GetHandle() { return h; }
    Slvs_hEntity handle() { return GetHandle(); }

    System* system() { return sys; }

    Slvs_hGroup GetGroup() { return constraint()->group; }

    int         type()      { return constraint()->type;  }
    //Workplane   workplane() { return Workplane(Entity::fromHandle(sys, constraint()->wrkpl)); }

public:
    static Constraint coincident(double value,
            Point3d p1, Point3d p2,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(p1.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_POINTS_COINCIDENT,
            SLVS_FREE_IN_3D,
            value,
            p1.handle(), p2.handle(), 0, 0));
    }
    static Constraint coincident(double value,
            Workplane wrkpl, Point p1, Point p2,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(p1.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_POINTS_COINCIDENT,
            wrkpl.handle(),
            value,
            p1.handle(), p2.handle(), 0, 0));
    }
    static Constraint distance(double value,
            Point3d p1, Point3d p2,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(p1.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_PT_DISTANCE,
            SLVS_FREE_IN_3D,
            value,
            p1.handle(), p2.handle(), 0, 0));
    }
    static Constraint distance(double value,
            Workplane wrkpl, Point p1, Point p2,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_PT_DISTANCE,
            wrkpl.handle(),
            value,
            p1.handle(), p2.handle(), 0, 0));
    }
    static Constraint distance(double value,
            Workplane wrkpl, Point3d p,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_PLANE_DISTANCE,
            wrkpl.handle(),
            value,
            p.handle(), 0, 0, 0));
    }
    static Constraint distance(double value,
            Workplane wrkpl, Point p, LineSegment line,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_LINE_DISTANCE,
            wrkpl.handle(),
            value,
            p.handle(), 0, line.handle(), 0));
    }
    static Constraint distance(double value,
            Point3d p, LineSegment3d line,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(p.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_LINE_DISTANCE,
            SLVS_FREE_IN_3D,
            value,
            p.handle(), 0, line.handle(), 0));
    }
// SLVS_C_PT_FACE_DISTANCE   
    static Constraint on(double value,
            Workplane wrkpl, Point3d p,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_IN_PLANE,
            wrkpl.handle(),
            value,
            p.handle(), 0, 0, 0));
    }
    static Constraint on(double value,
            Workplane wrkpl, Point p, LineSegment line,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_ON_LINE,
            wrkpl.handle(),
            value,
            p.handle(), 0, line.handle(), 0));
    }
    static Constraint on(double value,
            Point3d p, LineSegment3d line,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(p.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_ON_LINE,
            SLVS_FREE_IN_3D,
            value,
            p.handle(), 0, line.handle(), 0));
    }
    static Constraint on(double value,
            Workplane wrkpl, Point p, Circle circle,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_PT_ON_CIRCLE,
            wrkpl.handle(),
            value,
            p.handle(), 0, circle.handle(), 0));
    }
// SLVS_C_PT_ON_FACE         
// SLVS_C_EQUAL_LENGTH_LINES 
// SLVS_C_LENGTH_RATIO       
// SLVS_C_EQ_LEN_PT_LINE_D   
// SLVS_C_EQ_PT_LN_DISTANCES 
// SLVS_C_EQUAL_ANGLE        
// SLVS_C_EQUAL_LINE_ARC_LEN 
// SLVS_C_SYMMETRIC          
// SLVS_C_SYMMETRIC_HORIZ    
// SLVS_C_SYMMETRIC_VERT     
// SLVS_C_SYMMETRIC_LINE     
// SLVS_C_AT_MIDPOINT        
    static Constraint horizontal(
            Workplane wrkpl, LineSegment line,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_HORIZONTAL,
            wrkpl.handle(),
            0,
            0, 0, line.handle(), 0));
    }
    static Constraint vertical(
            Workplane wrkpl, LineSegment line,
            Slvs_hGroup group = USE_DEFAULT_GROUP) {
        return init(wrkpl.system(), Slvs_MakeConstraint(
            0, group,
            SLVS_C_VERTICAL,
            wrkpl.handle(),
            0,
            0, 0, line.handle(), 0));
    }
// SLVS_C_DIAMETER           
// SLVS_C_SAME_ORIENTATION   
// SLVS_C_ANGLE              
// SLVS_C_PARALLEL           
// SLVS_C_PERPENDICULAR      
// SLVS_C_ARC_LINE_TANGENT   
// SLVS_C_CUBIC_LINE_TANGENT 
// SLVS_C_EQUAL_RADIUS       
// SLVS_C_PROJ_PT_DISTANCE   
// SLVS_C_WHERE_DRAGGED      
// SLVS_C_CURVE_CURVE_TANGENT
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
        if (params >= param_space)
            throw not_enough_space_exception(
                "too many params");
        if (ENABLE_SAFETY) {
            for (int i=0;i<params;i++)
                if (param[i].h == p.h)
                    throw invalid_value_exception(
                        "duplicate value for param handle: %lu",
                        p.h);
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
    void check_unique_constraint_handle(Slvs_hConstraint h) {
        for (int i=0;i<constraints;i++)
            if (constraint[i].h == h)
                throw invalid_value_exception(
                    "duplicate value for constraint handle: %lu", h);
    }
    void check_group(Slvs_hGroup group) {
        if (group < 1)
            throw invalid_value_exception("invalid group: %d", group);
    }
    void check_entity_type(int type)     { /* TODO */ }
    void check_constraint_type(int type) { /* TODO */ }
    int check_entity_handle(Slvs_hEntity h, bool allow_none = false) {
        if (allow_none && !h)
            return 0;
        for (int i=0;i<entities;i++)
            if (entity[i].h == h)
                return entity[i].type;
        throw invalid_value_exception("invalid entity handle: %lu", h);
    }
    void check_entity_handle_workplane(Slvs_hEntity h, bool allow_none) {
        if (allow_none && !h)
            return;
        int type = check_entity_handle(h);
        if (type != SLVS_E_WORKPLANE)
            throw invalid_value_exception("entity of handle %lu must be a workplane", h);
    }
    void check_entity_handle_point(Slvs_hEntity h, bool allow_none) {
        if (allow_none && !h)
            return;
        int type = check_entity_handle(h);
        if (type != SLVS_E_POINT_IN_2D && type != SLVS_E_POINT_IN_3D)
            throw invalid_value_exception("entity of handle %lu must be a point", h);
    }
    void check_entity_handle_normal(Slvs_hEntity h, bool allow_none) {
        if (allow_none && !h)
            return;
        int type = check_entity_handle(h);
        if (type != SLVS_E_NORMAL_IN_2D && type != SLVS_E_NORMAL_IN_3D)
            throw invalid_value_exception("entity of handle %lu must be a normal", h);
    }
    void check_entity_handle_distance(Slvs_hEntity h, bool allow_none) {
        if (allow_none && !h)
            return;
        int type = check_entity_handle(h);
        if (type != SLVS_E_DISTANCE)
            throw invalid_value_exception("entity of handle %lu must be a distance", h);
    }
    void check_param_handle(Slvs_hParam h, bool allow_none) {
        if (allow_none && !h)
            return;
        for (int i=0;i<params;i++)
            if (param[i].h == h)
                return;
        throw invalid_value_exception(
            "invalid param handle (not found in system): %lu", h);
    }
public:

    void add_entity(Slvs_Entity p) {
        if (entities >= entity_space)
            throw not_enough_space_exception(
                "too many entities");
        if (ENABLE_SAFETY) {
            check_unique_entity_handle(p.h);
            check_group(p.group);
            check_entity_type(p.type);
            check_entity_handle_workplane(p.wrkpl, true);
            check_entity_handle_point(p.point[0], true);
            check_entity_handle_point(p.point[1], true);
            check_entity_handle_point(p.point[2], true);
            check_entity_handle_point(p.point[3], true);
            check_entity_handle_normal(p.normal, true);
            check_entity_handle_distance(p.distance, true);
            check_param_handle(p.param[0], true);
            check_param_handle(p.param[1], true);
            check_param_handle(p.param[2], true);
            check_param_handle(p.param[3], true);
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
        if (constraints >= constraint_space)
            throw not_enough_space_exception(
                "too many constraints");
        if (ENABLE_SAFETY) {
            check_unique_constraint_handle(p.h);
            check_group(p.group);
            check_constraint_type(p.type);
            check_entity_handle_workplane(p.wrkpl, true);
            check_entity_handle_point(p.ptA, true);
            check_entity_handle_point(p.ptB, true);
            check_entity_handle(p.entityA, true);
            check_entity_handle(p.entityB, true);
            check_entity_handle(p.entityC, true);
            check_entity_handle(p.entityD, true);
        }
        constraint[constraints++] = p;
    }

    Constraint add_constraint_with_next_handle(Slvs_Constraint p) {
        p.h = constraints+1;
        if (p.group == USE_DEFAULT_GROUP)
            p.group = default_group;
        add_constraint(p);
        return Constraint(this, p.h);
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
            throw invalid_value_exception(
                "Cannot set dragged[%d]\n", i);
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

void Param::prepareFor(System* system, Slvs_hGroup group)
        throw(wrong_system_exception, invalid_state_exception) {
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

Slvs_Constraint* Constraint::constraint() {
    if (sys) {
        if (sys->constraint[h-1].h != h)
            throw invalid_state_exception("constraint not found at index (handle-1)");
        return &sys->constraint[h-1];
    } else {
        throw invalid_state_exception("invalid system");
    }
}

Constraint Constraint::init(System* sys, Slvs_Constraint c) {
    return sys->add_constraint_with_next_handle(c);
}

#endif  // defined SLVS_PYTHON_
