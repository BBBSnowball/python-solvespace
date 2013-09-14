//-----------------------------------------------------------------------------
// Anything involving surfaces and sets of surfaces (i.e., shells); except
// for the real math, which is in ratpoly.cpp.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

SSurface SSurface::FromExtrusionOf(SBezier *sb, Vector t0, Vector t1) {
    SSurface ret;
    ZERO(&ret);

    ret.degm = sb->deg;
    ret.degn = 1;

    int i;
    for(i = 0; i <= ret.degm; i++) {
        ret.ctrl[i][0] = (sb->ctrl[i]).Plus(t0);
        ret.weight[i][0] = sb->weight[i];

        ret.ctrl[i][1] = (sb->ctrl[i]).Plus(t1);
        ret.weight[i][1] = sb->weight[i];
    }

    return ret;
}

bool SSurface::IsExtrusion(SBezier *of, Vector *alongp) {
    int i;

    if(degn != 1) return false;

    Vector along = (ctrl[0][1]).Minus(ctrl[0][0]);
    for(i = 0; i <= degm; i++) {
        if((fabs(weight[i][1] - weight[i][0]) < LENGTH_EPS) &&
           ((ctrl[i][1]).Minus(ctrl[i][0])).Equals(along))
        {
            continue;
        }
        return false;
    }

    // yes, we are a surface of extrusion; copy the original curve and return
    if(of) {
        for(i = 0; i <= degm; i++) {
            of->weight[i] = weight[i][0];
            of->ctrl[i] = ctrl[i][0];
        }
        of->deg = degm;
        *alongp = along;
    }
    return true;
}

bool SSurface::IsCylinder(Vector *axis, Vector *center, double *r,
                            Vector *start, Vector *finish)
{
    SBezier sb;
    if(!IsExtrusion(&sb, axis)) return false;
    if(!sb.IsCircle(*axis, center, r)) return false;

    *start = sb.ctrl[0];
    *finish = sb.ctrl[2];
    return true;
}

SSurface SSurface::FromRevolutionOf(SBezier *sb, Vector pt, Vector axis,
                                    double thetas, double thetaf)
{
    SSurface ret;
    ZERO(&ret);


    ret.degm = sb->deg;
    ret.degn = 2;

    double dtheta = fabs(WRAP_SYMMETRIC(thetaf - thetas, 2*PI));

    // We now wish to revolve the curve about the z axis
    int i;
    for(i = 0; i <= ret.degm; i++) {
        Vector p = sb->ctrl[i];

        Vector ps = p.RotatedAbout(pt, axis, thetas),
               pf = p.RotatedAbout(pt, axis, thetaf);

        Vector ct;
        if(ps.Equals(pf)) {
            // Degenerate case: a control point lies on the axis of revolution,
            // so we get three coincident control points.
            ct = ps;
        } else {
            // Normal case, the control point sweeps out a circle.
            Vector c = ps.ClosestPointOnLine(pt, axis);

            Vector rs = ps.Minus(c),
                   rf = pf.Minus(c);

            Vector ts = axis.Cross(rs),
                   tf = axis.Cross(rf);

            ct = Vector::AtIntersectionOfLines(ps, ps.Plus(ts),
                                               pf, pf.Plus(tf),
                                               NULL, NULL, NULL);
        }

        ret.ctrl[i][0] = ps;
        ret.ctrl[i][1] = ct;
        ret.ctrl[i][2] = pf;

        ret.weight[i][0] = sb->weight[i];
        ret.weight[i][1] = sb->weight[i]*cos(dtheta/2);
        ret.weight[i][2] = sb->weight[i];
    }

    return ret;
}

SSurface SSurface::FromPlane(Vector pt, Vector u, Vector v) {
    SSurface ret;
    ZERO(&ret);

    ret.degm = 1;
    ret.degn = 1;

    ret.weight[0][0] = ret.weight[0][1] = 1; 
    ret.weight[1][0] = ret.weight[1][1] = 1;

    ret.ctrl[0][0] = pt;
    ret.ctrl[0][1] = pt.Plus(u);
    ret.ctrl[1][0] = pt.Plus(v);
    ret.ctrl[1][1] = pt.Plus(v).Plus(u);
    
    return ret;
}

SSurface SSurface::FromTransformationOf(SSurface *a,
                                        Vector t, Quaternion q, double scale,
                                        bool includingTrims)
{
    SSurface ret;
    ZERO(&ret);

    ret.h = a->h;
    ret.color = a->color;
    ret.face = a->face;

    ret.degm = a->degm;
    ret.degn = a->degn;
    int i, j;
    for(i = 0; i <= 3; i++) {
        for(j = 0; j <= 3; j++) {
            ret.ctrl[i][j] = a->ctrl[i][j];
            ret.ctrl[i][j] = (ret.ctrl[i][j]).ScaledBy(scale);
            ret.ctrl[i][j] = (q.Rotate(ret.ctrl[i][j])).Plus(t);

            ret.weight[i][j] = a->weight[i][j];
        }
    }

    if(includingTrims) {
        STrimBy *stb;
        for(stb = a->trim.First(); stb; stb = a->trim.NextAfter(stb)) {
            STrimBy n = *stb;
            n.start  = n.start.ScaledBy(scale);
            n.finish = n.finish.ScaledBy(scale);
            n.start  = (q.Rotate(n.start)) .Plus(t);
            n.finish = (q.Rotate(n.finish)).Plus(t);
            ret.trim.Add(&n);
        }
    }

    if(scale < 0) {
        // If we mirror every surface of a shell, then it will end up inside
        // out. So fix that here.
        ret.Reverse();
    }

    return ret;
}

void SSurface::GetAxisAlignedBounding(Vector *ptMax, Vector *ptMin) {
    *ptMax = Vector::From(VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE);
    *ptMin = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            (ctrl[i][j]).MakeMaxMin(ptMax, ptMin);
        }
    }
}

bool SSurface::LineEntirelyOutsideBbox(Vector a, Vector b, bool segment) {
    Vector amax, amin;
    GetAxisAlignedBounding(&amax, &amin);
    if(!Vector::BoundingBoxIntersectsLine(amax, amin, a, b, segment)) {
        // The line segment could fail to intersect the bbox, but lie entirely
        // within it and intersect the surface.
        if(a.OutsideAndNotOn(amax, amin) && b.OutsideAndNotOn(amax, amin)) {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// Generate the piecewise linear approximation of the trim stb, which applies
// to the curve sc.
//-----------------------------------------------------------------------------
void SSurface::MakeTrimEdgesInto(SEdgeList *sel, int flags,
                                 SCurve *sc, STrimBy *stb)
{
    Vector prev;
    bool inCurve = false, empty = true;
    double u = 0, v = 0;

    int i, first, last, increment;
    if(stb->backwards) {
        first = sc->pts.n - 1;
        last = 0;
        increment = -1;
    } else {
        first = 0;
        last = sc->pts.n - 1;
        increment = 1;
    }
    for(i = first; i != (last + increment); i += increment) {
        Vector tpt, *pt = &(sc->pts.elem[i].p);

        if(flags & AS_UV) {
            ClosestPointTo(*pt, &u, &v);
            tpt = Vector::From(u, v, 0);
        } else {
            tpt = *pt;
        }

        if(inCurve) {
            sel->AddEdge(prev, tpt, sc->h.v, stb->backwards);
            empty = false;
        }

        prev = tpt;     // either uv or xyz, depending on flags

        if(pt->Equals(stb->start)) inCurve = true;
        if(pt->Equals(stb->finish)) inCurve = false;
    }
    if(inCurve) dbp("trim was unterminated");
    if(empty)   dbp("trim was empty");
}

//-----------------------------------------------------------------------------
// Generate all of our trim curves, in piecewise linear form. We can do
// so in either uv or xyz coordinates. And if requested, then we can use
// the split curves from useCurvesFrom instead of the curves in our own
// shell.
//-----------------------------------------------------------------------------
void SSurface::MakeEdgesInto(SShell *shell, SEdgeList *sel, int flags,
                             SShell *useCurvesFrom)
{
    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        SCurve *sc = shell->curve.FindById(stb->curve);

        // We have the option to use the curves from another shell; this
        // is relevant when generating the coincident edges while doing the
        // Booleans, since the curves from the output shell will be split
        // against any intersecting surfaces (and the originals aren't).
        if(useCurvesFrom) {
            sc = useCurvesFrom->curve.FindById(sc->newH);
        }

        MakeTrimEdgesInto(sel, flags, sc, stb);
    }
}

//-----------------------------------------------------------------------------
// Compute the exact tangent to the intersection curve between two surfaces,
// by taking the cross product of the surface normals. We choose the direction
// of this tangent so that its dot product with dir is positive.
//-----------------------------------------------------------------------------
Vector SSurface::ExactSurfaceTangentAt(Vector p, SSurface *srfA, SSurface *srfB,
                                       Vector dir)
{
    Point2d puva, puvb;
    srfA->ClosestPointTo(p, &puva);
    srfB->ClosestPointTo(p, &puvb);
    Vector ts = (srfA->NormalAt(puva)).Cross(
                (srfB->NormalAt(puvb)));
    ts = ts.WithMagnitude(1);
    if(ts.Dot(dir) < 0) {
        ts = ts.ScaledBy(-1);
    }
    return ts;
}

//-----------------------------------------------------------------------------
// Report our trim curves. If a trim curve is exact and sbl is not null, then
// add its exact form to sbl. Otherwise, add its piecewise linearization to
// sel.
//-----------------------------------------------------------------------------
void SSurface::MakeSectionEdgesInto(SShell *shell,
                                    SEdgeList *sel, SBezierList *sbl)
{
    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        SCurve *sc = shell->curve.FindById(stb->curve);
        SBezier *sb = &(sc->exact);

        if(sbl && sc->isExact && (sb->deg != 1 || !sel)) {
            double ts, tf;
            if(stb->backwards) {
                sb->ClosestPointTo(stb->start,  &tf);
                sb->ClosestPointTo(stb->finish, &ts);
            } else {
                sb->ClosestPointTo(stb->start,  &ts);
                sb->ClosestPointTo(stb->finish, &tf);
            }
            SBezier junk_bef, keep_aft;
            sb->SplitAt(ts, &junk_bef, &keep_aft);
            // In the kept piece, the range that used to go from ts to 1
            // now goes from 0 to 1; so rescale tf appropriately.
            tf = (tf - ts)/(1 - ts);

            SBezier keep_bef, junk_aft;
            keep_aft.SplitAt(tf, &keep_bef, &junk_aft);

            sbl->l.Add(&keep_bef);
        } else if(sbl && !sel && !sc->isExact) {
            // We must approximate this trim curve, as piecewise cubic sections.
            SSurface *srfA = shell->surface.FindById(sc->surfA),
                     *srfB = shell->surface.FindById(sc->surfB);

            Vector s = stb->backwards ? stb->finish : stb->start,
                   f = stb->backwards ? stb->start : stb->finish;

            int sp, fp;
            for(sp = 0; sp < sc->pts.n; sp++) {
                if(s.Equals(sc->pts.elem[sp].p)) break;
            }
            if(sp >= sc->pts.n) return;
            for(fp = sp; fp < sc->pts.n; fp++) {
                if(f.Equals(sc->pts.elem[fp].p)) break;
            }
            if(fp >= sc->pts.n) return;
            // So now the curve we want goes from elem[sp] to elem[fp]

            while(sp < fp) {
                // Initially, we'll try approximating the entire trim curve
                // as a single Bezier segment
                int fpt = fp;

                for(;;) {
                    // So construct a cubic Bezier with the correct endpoints
                    // and tangents for the current span.
                    Vector st = sc->pts.elem[sp].p,
                           ft = sc->pts.elem[fpt].p,
                           sf = ft.Minus(st);
                    double m = sf.Magnitude() / 3;

                    Vector stan = ExactSurfaceTangentAt(st, srfA, srfB, sf),
                           ftan = ExactSurfaceTangentAt(ft, srfA, srfB, sf);

                    SBezier sb = SBezier::From(st,
                                               st.Plus (stan.WithMagnitude(m)),
                                               ft.Minus(ftan.WithMagnitude(m)),
                                               ft);

                    // And test how much this curve deviates from the
                    // intermediate points (if any).
                    int i;
                    bool tooFar = false;
                    for(i = sp + 1; i <= (fpt - 1); i++) {
                        Vector p = sc->pts.elem[i].p;
                        double t;
                        sb.ClosestPointTo(p, &t, false);
                        Vector pp = sb.PointAt(t);
                        if((pp.Minus(p)).Magnitude() > SS.ChordTolMm()/2) {
                            tooFar = true;
                            break;
                        }
                    }

                    if(tooFar) {
                        // Deviates by too much, so try a shorter span
                        fpt--;
                        continue;
                    } else {
                        // Okay, so use this piece and break.
                        sbl->l.Add(&sb);
                        break;
                    }
                }

                // And continue interpolating, starting wherever the curve
                // we just generated finishes.
                sp = fpt;
            }
        } else {
            if(sel) MakeTrimEdgesInto(sel, AS_XYZ, sc, stb);
        }
    }
}

void SSurface::TriangulateInto(SShell *shell, SMesh *sm) {
    SEdgeList el;
    ZERO(&el);

    MakeEdgesInto(shell, &el, AS_UV);

    SPolygon poly;
    ZERO(&poly);
    if(el.AssemblePolygon(&poly, NULL, true)) {
        int i, start = sm->l.n;
        if(degm == 1 && degn == 1) {
            // A surface with curvature along one direction only; so 
            // choose the triangulation with chords that lie as much
            // as possible within the surface. And since the trim curves
            // have been pwl'd to within the desired chord tol, that will
            // produce a surface good to within roughly that tol.
            //
            // If this is just a plane (degree (1, 1)) then the triangulation
            // code will notice that, and not bother checking chord tols.
            poly.UvTriangulateInto(sm, this);
        } else {
            // A surface with compound curvature. So we must overlay a
            // two-dimensional grid, and triangulate around that.
            poly.UvGridTriangulateInto(sm, this);
        }

        STriMeta meta = { face, color };
        for(i = start; i < sm->l.n; i++) {
            STriangle *st = &(sm->l.elem[i]);
            st->meta = meta;
            st->an = NormalAt(st->a.x, st->a.y);
            st->bn = NormalAt(st->b.x, st->b.y);
            st->cn = NormalAt(st->c.x, st->c.y);
            st->a = PointAt(st->a.x, st->a.y);
            st->b = PointAt(st->b.x, st->b.y);
            st->c = PointAt(st->c.x, st->c.y);
            // Works out that my chosen contour direction is inconsistent with
            // the triangle direction, sigh.
            st->FlipNormal();
        }
    } else {
        dbp("failed to assemble polygon to trim nurbs surface in uv space");
    }

    el.Clear();
    poly.Clear();
}

//-----------------------------------------------------------------------------
// Reverse the parametrisation of one of our dimensions, which flips the
// normal. We therefore must reverse all our trim curves too. The uv
// coordinates change, but trim curves are stored as xyz so nothing happens
//-----------------------------------------------------------------------------
void SSurface::Reverse(void) {
    int i, j;
    for(i = 0; i < (degm+1)/2; i++) {
        for(j = 0; j <= degn; j++) {
            SWAP(Vector, ctrl[i][j], ctrl[degm-i][j]);
            SWAP(double, weight[i][j], weight[degm-i][j]);
        }
    }

    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        stb->backwards = !stb->backwards;
        SWAP(Vector, stb->start, stb->finish);
    }
}

void SSurface::ScaleSelfBy(double s) {
    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            ctrl[i][j] = ctrl[i][j].ScaledBy(s);
        }
    }
}

void SSurface::Clear(void) {
    trim.Clear();
}

void SShell::MakeFromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1,
                                 int color)
{
    // Make the extrusion direction consistent with respect to the normal
    // of the sketch we're extruding.
    if((t0.Minus(t1)).Dot(sbls->normal) < 0) {
        SWAP(Vector, t0, t1);
    }

    // Define a coordinate system to contain the original sketch, and get
    // a bounding box in that csys
    Vector n = sbls->normal.ScaledBy(-1);
    Vector u = n.Normal(0), v = n.Normal(1);
    Vector orig = sbls->point;
    double umax = 1e-10, umin = 1e10;
    sbls->GetBoundingProjd(u, orig, &umin, &umax);
    double vmax = 1e-10, vmin = 1e10;
    sbls->GetBoundingProjd(v, orig, &vmin, &vmax);
    // and now fix things up so that all u and v lie between 0 and 1
    orig = orig.Plus(u.ScaledBy(umin));
    orig = orig.Plus(v.ScaledBy(vmin));
    u = u.ScaledBy(umax - umin);
    v = v.ScaledBy(vmax - vmin);

    // So we can now generate the top and bottom surfaces of the extrusion,
    // planes within a translated (and maybe mirrored) version of that csys.
    SSurface s0, s1;
    s0 = SSurface::FromPlane(orig.Plus(t0), u, v);
    s0.color = color;
    s1 = SSurface::FromPlane(orig.Plus(t1).Plus(u), u.ScaledBy(-1), v);
    s1.color = color;
    hSSurface hs0 = surface.AddAndAssignId(&s0),
              hs1 = surface.AddAndAssignId(&s1);
    
    // Now go through the input curves. For each one, generate its surface
    // of extrusion, its two translated trim curves, and one trim line. We
    // go through by loops so that we can assign the lines correctly.
    SBezierLoop *sbl;
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        SBezier *sb;

        typedef struct {
            hSCurve     hc;
            hSSurface   hs;
        } TrimLine;
        List<TrimLine> trimLines;
        ZERO(&trimLines);

        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            // Generate the surface of extrusion of this curve, and add
            // it to the list
            SSurface ss = SSurface::FromExtrusionOf(sb, t0, t1);
            ss.color = color;
            hSSurface hsext = surface.AddAndAssignId(&ss);

            // Translate the curve by t0 and t1 to produce two trim curves
            SCurve sc;
            ZERO(&sc);
            sc.isExact = true;
            sc.exact = sb->TransformedBy(t0, Quaternion::IDENTITY, 1.0);
            (sc.exact).MakePwlInto(&(sc.pts));
            sc.surfA = hs0;
            sc.surfB = hsext;
            hSCurve hc0 = curve.AddAndAssignId(&sc);

            ZERO(&sc);
            sc.isExact = true;
            sc.exact = sb->TransformedBy(t1, Quaternion::IDENTITY, 1.0);
            (sc.exact).MakePwlInto(&(sc.pts));
            sc.surfA = hs1;
            sc.surfB = hsext;
            hSCurve hc1 = curve.AddAndAssignId(&sc);

            STrimBy stb0, stb1;
            // The translated curves trim the flat top and bottom surfaces.
            stb0 = STrimBy::EntireCurve(this, hc0, false);
            stb1 = STrimBy::EntireCurve(this, hc1, true);
            (surface.FindById(hs0))->trim.Add(&stb0);
            (surface.FindById(hs1))->trim.Add(&stb1);

            // The translated curves also trim the surface of extrusion.
            stb0 = STrimBy::EntireCurve(this, hc0, true);
            stb1 = STrimBy::EntireCurve(this, hc1, false);
            (surface.FindById(hsext))->trim.Add(&stb0);
            (surface.FindById(hsext))->trim.Add(&stb1);

            // And form the trim line
            Vector pt = sb->Finish();
            ZERO(&sc);
            sc.isExact = true;
            sc.exact = SBezier::From(pt.Plus(t0), pt.Plus(t1));
            (sc.exact).MakePwlInto(&(sc.pts));
            hSCurve hl = curve.AddAndAssignId(&sc);
            // save this for later
            TrimLine tl;
            tl.hc = hl;
            tl.hs = hsext;
            trimLines.Add(&tl);
        }

        int i;
        for(i = 0; i < trimLines.n; i++) {
            TrimLine *tl = &(trimLines.elem[i]);
            SSurface *ss = surface.FindById(tl->hs);

            TrimLine *tlp = &(trimLines.elem[WRAP(i-1, trimLines.n)]);

            STrimBy stb;
            stb = STrimBy::EntireCurve(this, tl->hc, true);
            ss->trim.Add(&stb);
            stb = STrimBy::EntireCurve(this, tlp->hc, false);
            ss->trim.Add(&stb);

            (curve.FindById(tl->hc))->surfA = ss->h;
            (curve.FindById(tlp->hc))->surfB = ss->h;
        }
        trimLines.Clear();
    }
}


void SShell::MakeFromRevolutionOf(SBezierLoopSet *sbls, Vector pt, Vector axis,
                                   int color)
{
    SBezierLoop *sbl;

    int i0 = surface.n, i;

    // Normalize the axis direction so that the direction of revolution
    // ends up parallel to the normal of the sketch, on the side of the
    // axis where the sketch is.
    Vector pto;
    double md = VERY_NEGATIVE;
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        SBezier *sb;
        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            // Choose the point farthest from the axis; we'll get garbage
            // if we choose a point that lies on the axis, for example.
            // (And our surface will be self-intersecting if the sketch
            // spans the axis, so don't worry about that.)
            Vector p = sb->Start();
            double d = p.DistanceToLine(pt, axis);
            if(d > md) {
                md = d;
                pto = p;
            }
        }
    }
    Vector ptc = pto.ClosestPointOnLine(pt, axis),
           up  = (pto.Minus(ptc)).WithMagnitude(1),
           vp  = (sbls->normal).Cross(up);
    if(vp.Dot(axis) < 0) {
        axis = axis.ScaledBy(-1);
    }

    // Now we actually build and trim the surfaces.
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        int i, j;
        SBezier *sb, *prev;

        typedef struct {
            hSSurface   d[4];
        } Revolved;
        List<Revolved> hsl;
        ZERO(&hsl);

        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            Revolved revs;
            for(j = 0; j < 4; j++) {
                if(sb->deg == 1 && 
                    (sb->ctrl[0]).DistanceToLine(pt, axis) < LENGTH_EPS &&
                    (sb->ctrl[1]).DistanceToLine(pt, axis) < LENGTH_EPS)
                {
                    // This is a line on the axis of revolution; it does
                    // not contribute a surface.
                    revs.d[j].v = 0;
                } else {
                    SSurface ss = SSurface::FromRevolutionOf(sb, pt, axis,
                                                             (PI/2)*j,
                                                             (PI/2)*(j+1));
                    ss.color = color;
                    revs.d[j] = surface.AddAndAssignId(&ss);
                }
            }
            hsl.Add(&revs);
        }

        for(i = 0; i < sbl->l.n; i++) {
            Revolved revs  = hsl.elem[i],
                     revsp = hsl.elem[WRAP(i-1, sbl->l.n)];

            sb   = &(sbl->l.elem[i]);
            prev = &(sbl->l.elem[WRAP(i-1, sbl->l.n)]);

            for(j = 0; j < 4; j++) {
                SCurve sc;
                Quaternion qs = Quaternion::From(axis, (PI/2)*j);
                // we want Q*(x - p) + p = Q*x + (p - Q*p)
                Vector ts = pt.Minus(qs.Rotate(pt));

                // If this input curve generate a surface, then trim that
                // surface with the rotated version of the input curve.
                if(revs.d[j].v) {
                    ZERO(&sc);
                    sc.isExact = true;
                    sc.exact = sb->TransformedBy(ts, qs, 1.0);
                    (sc.exact).MakePwlInto(&(sc.pts));
                    sc.surfA = revs.d[j];
                    sc.surfB = revs.d[WRAP(j-1, 4)];

                    hSCurve hcb = curve.AddAndAssignId(&sc);

                    STrimBy stb;
                    stb = STrimBy::EntireCurve(this, hcb, true);
                    (surface.FindById(sc.surfA))->trim.Add(&stb);
                    stb = STrimBy::EntireCurve(this, hcb, false);
                    (surface.FindById(sc.surfB))->trim.Add(&stb);
                }

                // And if this input curve and the one after it both generated
                // surfaces, then trim both of those by the appropriate
                // circle. 
                if(revs.d[j].v && revsp.d[j].v) {
                    SSurface *ss = surface.FindById(revs.d[j]);

                    ZERO(&sc);
                    sc.isExact = true;
                    sc.exact = SBezier::From(ss->ctrl[0][0],
                                             ss->ctrl[0][1],
                                             ss->ctrl[0][2]);
                    sc.exact.weight[1] = ss->weight[0][1];
                    (sc.exact).MakePwlInto(&(sc.pts));
                    sc.surfA = revs.d[j];
                    sc.surfB = revsp.d[j];

                    hSCurve hcc = curve.AddAndAssignId(&sc);

                    STrimBy stb;
                    stb = STrimBy::EntireCurve(this, hcc, false);
                    (surface.FindById(sc.surfA))->trim.Add(&stb);
                    stb = STrimBy::EntireCurve(this, hcc, true);
                    (surface.FindById(sc.surfB))->trim.Add(&stb);
                }
            }
        }

        hsl.Clear();
    }

    for(i = i0; i < surface.n; i++) {
        SSurface *srf = &(surface.elem[i]);

        // Revolution of a line; this is potentially a plane, which we can
        // rewrite to have degree (1, 1).
        if(srf->degm == 1 && srf->degn == 2) {
            // close start, far start, far finish
            Vector cs, fs, ff;
            double d0, d1;
            d0 = (srf->ctrl[0][0]).DistanceToLine(pt, axis);
            d1 = (srf->ctrl[1][0]).DistanceToLine(pt, axis);

            if(d0 > d1) {
                cs = srf->ctrl[1][0];
                fs = srf->ctrl[0][0];
                ff = srf->ctrl[0][2];
            } else {
                cs = srf->ctrl[0][0];
                fs = srf->ctrl[1][0];
                ff = srf->ctrl[1][2];
            }

            // origin close, origin far
            Vector oc = cs.ClosestPointOnLine(pt, axis),
                   of = fs.ClosestPointOnLine(pt, axis);

            if(oc.Equals(of)) {
                // This is a plane, not a (non-degenerate) cone.
                Vector oldn = srf->NormalAt(0.5, 0.5);

                Vector u = fs.Minus(of), v;

                v = (axis.Cross(u)).WithMagnitude(1);

                double vm = (ff.Minus(of)).Dot(v);
                v = v.ScaledBy(vm);

                srf->degm = 1;
                srf->degn = 1;
                srf->ctrl[0][0] = of;
                srf->ctrl[0][1] = of.Plus(u);
                srf->ctrl[1][0] = of.Plus(v);
                srf->ctrl[1][1] = of.Plus(u).Plus(v);
                srf->weight[0][0] = 1;
                srf->weight[0][1] = 1;
                srf->weight[1][0] = 1;
                srf->weight[1][1] = 1;

                if(oldn.Dot(srf->NormalAt(0.5, 0.5)) < 0) {
                    SWAP(Vector, srf->ctrl[0][0], srf->ctrl[1][0]);
                    SWAP(Vector, srf->ctrl[0][1], srf->ctrl[1][1]);
                }
                continue;
            }

            if(fabs(d0 - d1) < LENGTH_EPS) {
                // This is a cylinder; so transpose it so that we'll recognize
                // it as a surface of extrusion.
                SSurface sn = *srf;

                // Transposing u and v flips the normal, so reverse u to
                // flip it again and put it back where we started.
                sn.degm = 2;
                sn.degn = 1;
                int dm, dn;
                for(dm = 0; dm <= 1; dm++) {
                    for(dn = 0; dn <= 2; dn++) {
                        sn.ctrl  [dn][dm] = srf->ctrl  [1-dm][dn];
                        sn.weight[dn][dm] = srf->weight[1-dm][dn];
                    }
                }

                *srf = sn;
                continue;
            }
        }

    }

}

void SShell::MakeFromCopyOf(SShell *a) {
    MakeFromTransformationOf(a,
        Vector::From(0, 0, 0), Quaternion::IDENTITY, 1.0);
}

void SShell::MakeFromTransformationOf(SShell *a,
                                      Vector t, Quaternion q, double scale)
{
    booleanFailed = false;

    SSurface *s;
    for(s = a->surface.First(); s; s = a->surface.NextAfter(s)) {
        SSurface n;
        n = SSurface::FromTransformationOf(s, t, q, scale, true);
        surface.Add(&n); // keeping the old ID
    }

    SCurve *c;
    for(c = a->curve.First(); c; c = a->curve.NextAfter(c)) {
        SCurve n;
        n = SCurve::FromTransformationOf(c, t, q, scale);
        curve.Add(&n); // keeping the old ID
    }
}

void SShell::MakeEdgesInto(SEdgeList *sel) {
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        s->MakeEdgesInto(this, sel, SSurface::AS_XYZ);
    }
}

void SShell::MakeSectionEdgesInto(Vector n, double d,
                                 SEdgeList *sel, SBezierList *sbl)
{
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        if(s->CoincidentWithPlane(n, d)) {
            s->MakeSectionEdgesInto(this, sel, sbl);
        }
    }
}

void SShell::TriangulateInto(SMesh *sm) {
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        s->TriangulateInto(this, sm);
    }
}

bool SShell::IsEmpty(void) {
    return (surface.n == 0);
}

void SShell::Clear(void) {
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        s->Clear();
    }
    surface.Clear();

    SCurve *c;
    for(c = curve.First(); c; c = curve.NextAfter(c)) {
        c->Clear();
    }
    curve.Clear();
}

