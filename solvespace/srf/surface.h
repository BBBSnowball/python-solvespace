//-----------------------------------------------------------------------------
// Functions relating to rational polynomial surfaces, which are trimmed by
// curves (either rational polynomial curves, or piecewise linear
// approximations to curves of intersection that can't be represented
// exactly in ratpoly form), and assembled into watertight shells.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __SURFACE_H
#define __SURFACE_H

// Utility functions, Bernstein polynomials of order 1-3 and their derivatives.
double Bernstein(int k, int deg, double t);
double BernsteinDerivative(int k, int deg, double t);

class SSurface;
class SCurvePt;

// Utility data structure, a two-dimensional BSP to accelerate polygon
// operations.
class SBspUv {
public:
    Point2d  a, b;

    SBspUv  *pos;
    SBspUv  *neg;

    SBspUv  *more;

    static const int INSIDE            = 100;
    static const int OUTSIDE           = 200;
    static const int EDGE_PARALLEL     = 300;
    static const int EDGE_ANTIPARALLEL = 400;
    static const int EDGE_OTHER        = 500;

    static SBspUv *Alloc(void);
    static SBspUv *From(SEdgeList *el, SSurface *srf);

    void ScalePoints(Point2d *pt, Point2d *a, Point2d *b, SSurface *srf);
    double ScaledSignedDistanceToLine(Point2d pt, Point2d a, Point2d b,
        SSurface *srf);
    double ScaledDistanceToLine(Point2d pt, Point2d a, Point2d b, bool seg,
        SSurface *srf);

    SBspUv *InsertEdge(Point2d a, Point2d b, SSurface *srf);
    int ClassifyPoint(Point2d p, Point2d eb, SSurface *srf);
    int ClassifyEdge(Point2d ea, Point2d eb, SSurface *srf);
    double MinimumDistanceToEdge(Point2d p, SSurface *srf);
};

// Now the data structures to represent a shell of trimmed rational polynomial
// surfaces.

class SShell;

class hSSurface {
public:
    DWORD v;
};

class hSCurve {
public:
    DWORD v;
};

// Stuff for rational polynomial curves, of degree one to three. These are
// our inputs, and are also calculated for certain exact surface-surface
// intersections.
class SBezier {
public:
    int             tag;
    int             auxA, auxB;

    int             deg;
    Vector          ctrl[4];
    double          weight[4];

    Vector PointAt(double t);
    Vector TangentAt(double t);
    void ClosestPointTo(Vector p, double *t, bool converge=true);
    void SplitAt(double t, SBezier *bef, SBezier *aft);
    bool PointOnThisAndCurve(SBezier *sbb, Vector *p);

    Vector Start(void);
    Vector Finish(void);
    bool Equals(SBezier *b);
    void MakePwlInto(SEdgeList *sel, double chordTol=0);
    void MakePwlInto(List<SCurvePt> *l, double chordTol=0);
    void MakePwlInto(SContour *sc, double chordTol=0);
    void MakePwlInto(List<Vector> *l, double chordTol=0);
    void MakePwlWorker(List<Vector> *l, double ta, double tb, double chordTol);

    void AllIntersectionsWith(SBezier *sbb, SPointList *spl);
    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax);
    void Reverse(void);

    bool IsInPlane(Vector n, double d);
    bool IsCircle(Vector axis, Vector *center, double *r);
    bool IsRational(void);

    SBezier TransformedBy(Vector t, Quaternion q, double scale);
    SBezier InPerspective(Vector u, Vector v, Vector n,
                          Vector origin, double cameraTan);
    void ScaleSelfBy(double s);

    static SBezier From(Vector p0, Vector p1, Vector p2, Vector p3);
    static SBezier From(Vector p0, Vector p1, Vector p2);
    static SBezier From(Vector p0, Vector p1);
    static SBezier From(Vector4 p0, Vector4 p1, Vector4 p2, Vector4 p3);
    static SBezier From(Vector4 p0, Vector4 p1, Vector4 p2);
    static SBezier From(Vector4 p0, Vector4 p1);
};

class SBezierList {
public:
    List<SBezier>   l;

    void Clear(void);
    void ScaleSelfBy(double s);
    void CullIdenticalBeziers(void);
    void AllIntersectionsWith(SBezierList *sblb, SPointList *spl);
    bool GetPlaneContainingBeziers(Vector *p, Vector *u, Vector *v,
                                        Vector *notCoplanarAt);
};

class SBezierLoop {
public:
    int             tag;
    List<SBezier>   l;

    inline void Clear(void) { l.Clear(); }
    bool IsClosed(void);
    void Reverse(void);
    void MakePwlInto(SContour *sc, double chordTol=0);
    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax);

    static SBezierLoop FromCurves(SBezierList *spcl,
                                  bool *allClosed, SEdge *errorAt);
};

class SBezierLoopSet {
public:
    List<SBezierLoop> l;
    Vector normal;
    Vector point;

    static SBezierLoopSet From(SBezierList *spcl, SPolygon *poly,
                               double chordTol,
                               bool *allClosed, SEdge *errorAt,
                               SBezierList *openContours);

    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax);
    void MakePwlInto(SPolygon *sp);
    void Clear(void);
};

class SBezierLoopSetSet {
public:
    List<SBezierLoopSet>    l;

    void FindOuterFacesFrom(SBezierList *sbl, SPolygon *spxyz, SSurface *srfuv,
                            double chordTol,
                            bool *allClosed, SEdge *notClosedAt,
                            bool *allCoplanar, Vector *notCoplanarAt,
                            SBezierList *openContours);
    void AddOpenPath(SBezier *sb);
    void Clear(void);
};

// Stuff for the surface trim curves: piecewise linear
class SCurvePt {
public:
    int         tag;
    Vector      p;
    bool        vertex;
};

class SCurve {
public:
    hSCurve         h;

    // In a Boolean, C = A op B. The curves in A and B get copied into C, and
    // therefore must get new hSCurves assigned. For the curves in A and B,
    // we use newH to record their new handle in C.
    hSCurve         newH;
    static const int FROM_A             = 100;
    static const int FROM_B             = 200;
    static const int FROM_INTERSECTION  = 300;
    int             source;

    bool            isExact;
    SBezier         exact;

    List<SCurvePt>  pts;

    hSSurface       surfA;
    hSSurface       surfB;

    static SCurve FromTransformationOf(SCurve *a, Vector t, Quaternion q, 
                                        double scale);
    SCurve MakeCopySplitAgainst(SShell *agnstA, SShell *agnstB,
                                SSurface *srfA, SSurface *srfB);
    void RemoveShortSegments(SSurface *srfA, SSurface *srfB);
    SSurface *GetSurfaceA(SShell *a, SShell *b);
    SSurface *GetSurfaceB(SShell *a, SShell *b);

    void Clear(void);
};

// A segment of a curve by which a surface is trimmed: indicates which curve,
// by its handle, and the starting and ending points of our segment of it.
// The vector out points out of the surface; it, the surface outer normal,
// and a tangent to the beginning of the curve are all orthogonal.
class STrimBy {
public:
    hSCurve     curve;
    bool        backwards;
    // If a trim runs backwards, then start and finish still correspond to
    // the actual start and finish, but they appear in reverse order in
    // the referenced curve.
    Vector      start;
    Vector      finish;

    static STrimBy STrimBy::EntireCurve(SShell *shell, hSCurve hsc, bool bkwds);
};

// An intersection point between a line and a surface
class SInter {
public:
    int         tag;
    Vector      p;
    SSurface    *srf;
    Point2d     pinter;
    Vector      surfNormal;     // of the intersecting surface, at pinter
    bool        onEdge;         // pinter is on edge of trim poly
};

// A rational polynomial surface in Bezier form.
class SSurface {
public:
    int             tag;
    hSSurface       h;

    // Same as newH for the curves; record what a surface gets renamed to
    // when I copy things over.
    hSSurface       newH;

    int             color;
    DWORD           face;

    int             degm, degn;
    Vector          ctrl[4][4];
    double          weight[4][4];

    List<STrimBy>   trim;

    // For testing whether a point (u, v) on the surface lies inside the trim
    SBspUv          *bsp;
    SEdgeList       edges;

    // For caching our initial (u, v) when doing Newton iterations to project
    // a point into our surface.
    Point2d         cached;

    static SSurface FromExtrusionOf(SBezier *spc, Vector t0, Vector t1);
    static SSurface FromRevolutionOf(SBezier *sb, Vector pt, Vector axis,
                                        double thetas, double thetaf);
    static SSurface FromPlane(Vector pt, Vector u, Vector v);
    static SSurface FromTransformationOf(SSurface *a, Vector t, Quaternion q,
                                         double scale,
                                         bool includingTrims);
    void ScaleSelfBy(double s);

    void EdgeNormalsWithinSurface(Point2d auv, Point2d buv,
                                  Vector *pt, Vector *enin, Vector *enout,
                                  Vector *surfn,
                                  DWORD auxA, 
                                  SShell *shell, SShell *sha, SShell *shb);
    void FindChainAvoiding(SEdgeList *src, SEdgeList *dest, SPointList *avoid);
    SSurface MakeCopyTrimAgainst(SShell *parent, SShell *a, SShell *b,
                                    SShell *into, int type);
    void TrimFromEdgeList(SEdgeList *el, bool asUv);
    void IntersectAgainst(SSurface *b, SShell *agnstA, SShell *agnstB, 
                          SShell *into);
    void AddExactIntersectionCurve(SBezier *sb, SSurface *srfB,
                          SShell *agnstA, SShell *agnstB, SShell *into);

    typedef struct {
        int     tag;
        Point2d p;
    } Inter;
    void WeightControlPoints(void);
    void UnWeightControlPoints(void);
    void CopyRowOrCol(bool row, int this_ij, SSurface *src, int src_ij);
    void BlendRowOrCol(bool row, int this_ij, SSurface *a, int a_ij,
                                              SSurface *b, int b_ij);
    double DepartureFromCoplanar(void);
    void SplitInHalf(bool byU, SSurface *sa, SSurface *sb);
    void AllPointsIntersecting(Vector a, Vector b,
                                    List<SInter> *l,
                                    bool seg, bool trimmed, bool inclTangent);
    void AllPointsIntersectingUntrimmed(Vector a, Vector b,
                                            int *cnt, int *level,
                                            List<Inter> *l, bool segment,
                                            SSurface *sorig);

    void ClosestPointTo(Vector p, Point2d *puv, bool converge=true);
    void ClosestPointTo(Vector p, double *u, double *v, bool converge=true);
    bool ClosestPointNewton(Vector p, double *u, double *v, bool converge=true);

    bool PointIntersectingLine(Vector p0, Vector p1, double *u, double *v);
    Vector ClosestPointOnThisAndSurface(SSurface *srf2, Vector p);
    void PointOnSurfaces(SSurface *s1, SSurface *s2, double *u, double *v);
    Vector PointAt(double u, double v);
    Vector PointAt(Point2d puv);
    void TangentsAt(double u, double v, Vector *tu, Vector *tv);
    Vector NormalAt(Point2d puv);
    Vector NormalAt(double u, double v);
    bool LineEntirelyOutsideBbox(Vector a, Vector b, bool segment);
    void GetAxisAlignedBounding(Vector *ptMax, Vector *ptMin);
    bool CoincidentWithPlane(Vector n, double d);
    bool CoincidentWith(SSurface *ss, bool sameNormal);
    bool IsExtrusion(SBezier *of, Vector *along);
    bool IsCylinder(Vector *axis, Vector *center, double *r,
                        Vector *start, Vector *finish);

    void TriangulateInto(SShell *shell, SMesh *sm);

    // these are intended as bitmasks, even though there's just one now
    static const int AS_UV  = 0x01;
    static const int AS_XYZ = 0x00;
    void MakeTrimEdgesInto(SEdgeList *sel, int flags, SCurve *sc, STrimBy *stb);
    void MakeEdgesInto(SShell *shell, SEdgeList *sel, int flags,
            SShell *useCurvesFrom=NULL);

    Vector ExactSurfaceTangentAt(Vector p, SSurface *srfA, SSurface *srfB,
                                Vector dir);
    void MakeSectionEdgesInto(SShell *shell, SEdgeList *sel, SBezierList *sbl);
    void MakeClassifyingBsp(SShell *shell, SShell *useCurvesFrom);
    double ChordToleranceForEdge(Vector a, Vector b);
    void MakeTriangulationGridInto(List<double> *l, double vs, double vf,
                                    bool swapped);
    Vector PointAtMaybeSwapped(double u, double v, bool swapped);

    void Reverse(void);
    void Clear(void);
};

class SShell {
public:
    IdList<SCurve,hSCurve>      curve;
    IdList<SSurface,hSSurface>  surface;

    bool                        booleanFailed;

    void MakeFromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1,
                             int color);
    void MakeFromRevolutionOf(SBezierLoopSet *sbls, Vector pt, Vector axis,
                             int color);

    void MakeFromUnionOf(SShell *a, SShell *b);
    void MakeFromDifferenceOf(SShell *a, SShell *b);
    static const int AS_UNION      = 10;
    static const int AS_DIFFERENCE = 11;
    static const int AS_INTERSECT  = 12;
    void MakeFromBoolean(SShell *a, SShell *b, int type);
    void CopyCurvesSplitAgainst(bool opA, SShell *agnst, SShell *into);
    void CopySurfacesTrimAgainst(SShell *sha, SShell *shb, SShell *into,
                                    int type);
    void MakeIntersectionCurvesAgainst(SShell *against, SShell *into);
    void MakeClassifyingBsps(SShell *useCurvesFrom);
    void AllPointsIntersecting(Vector a, Vector b, List<SInter> *il,
                                bool seg, bool trimmed, bool inclTangent);
    void MakeCoincidentEdgesInto(SSurface *proto, bool sameNormal,
                                 SEdgeList *el, SShell *useCurvesFrom);
    void RewriteSurfaceHandlesForCurves(SShell *a, SShell *b);
    void CleanupAfterBoolean(void);

    // Definitions when classifying regions of a surface; it is either inside,
    // outside, or coincident (with parallel or antiparallel normal) with a
    // shell.
    static const int INSIDE     = 100;
    static const int OUTSIDE    = 200;
    static const int COINC_SAME = 300;
    static const int COINC_OPP  = 400;
    static const double DOTP_TOL;
    int ClassifyRegion(Vector edge_n, Vector inter_surf_n, Vector edge_surf_n);
    bool ClassifyEdge(int *indir, int *outdir,
                      Vector ea, Vector eb,
                      Vector p, 
                      Vector edge_n_in, Vector edge_n_out, Vector surf_n);

    void MakeFromCopyOf(SShell *a);
    void MakeFromTransformationOf(SShell *a,
                                    Vector trans, Quaternion q, double scale);
    void MakeFromAssemblyOf(SShell *a, SShell *b);
    void MergeCoincidentSurfaces(void);

    void TriangulateInto(SMesh *sm);
    void MakeEdgesInto(SEdgeList *sel);
    void MakeSectionEdgesInto(Vector n, double d,
                                SEdgeList *sel, SBezierList *sbl);
    bool IsEmpty(void);
    void RemapFaces(Group *g, int remap);
    void Clear(void);
};

#endif

