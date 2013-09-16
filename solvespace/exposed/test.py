#-----------------------------------------------------------------------------
# Some sample code for slvs.dll. We draw some geometric entities, provide
# initial guesses for their positions, and then constrain them. The solver
# calculates their new positions, in order to satisfy the constraints.
#
# Copyright 2008-2013 Jonathan Westhues.
# Ported to Python by Benjamin Koch
#-----------------------------------------------------------------------------

#from slvs import System, Param
from slvs import *
import unittest

def printf(fmt, *args):
    print fmt % args

class H(object):
    __slots__ = "handle"

    def __init__(self, h):
        self.handle = h

class TestSlvs(unittest.TestCase):
    def floatEqual(self, a, b):
        return abs(a-b) < 0.001

    def assertFloatEqual(self, a, b):
        if self.floatEqual(a, b):
            self.assertTrue(True)
        else:
            self.assertEqual(a, b)

    def assertFloatListEqual(self, xs, ys):
        if len(xs) != len(ys):
            self.assertListEqual(xs, ys)
        else:
            for i,a,b in zip(range(len(xs)), xs, ys):
                if not self.floatEqual(a, b):
                    self.assertEqual(a, b, "in list at index %d" % i)
            self.assertTrue(True)

    def test_param(self):
        sys = System()

        p1 = Param(17.3)
        self.assertFloatEqual(p1.value, 17.3)

        p2 = Param(1.0)
        p3 = Param(0.0)
        e = sys.add_point3d(p1, p2, p3)

        self.assertFloatEqual(p1.value, 17.3)
        self.assertFloatEqual(p2.value,  1.0)
        self.assertFloatEqual(p3.value,  0.0)

        p1 = e.x()
        p2 = e.y()
        p3 = e.z()

        self.assertFloatEqual(p1.value, 17.3)
        self.assertFloatEqual(p2.value,  1.0)
        self.assertFloatEqual(p3.value,  0.0)

        self.assertEqual(p1.handle, 1)
        self.assertEqual(p2.handle, 2)
        self.assertEqual(p3.handle, 3)


        p4 = sys.add_param(42.7)
        self.assertFloatEqual(p4.value, 42.7)

    #-----------------------------------------------------------------------------
    # An example of a constraint in 3d. We create a single group, with some
    # entities and constraints.
    #-----------------------------------------------------------------------------
    def test_example3d(self):
        sys = System()

        # This will contain a single group, which will arbitrarily number 1.
        g = 1;

        # A point, initially at (x y z) = (10 10 10)
        a = sys.add_param(10.0)
        b = sys.add_param(10.0)
        c = sys.add_param(10.0)
        #p1 = sys.add_point3d(Param(10.0), Param(10.0), Param(10.0))
        p1 = Point3d(a, b, c)
        # and a second point at (20 20 20)
        p2 = Point3d(Param(20.0), Param(20.0), Param(20.0), sys)
        # and a line segment connecting them.
        line = LineSegment3d(p1, p2)

        # The distance between the points should be 30.0 units.
        sys.add_constraint(Slvs_MakeConstraint(
                                                1, g,
                                                SLVS_C_PT_PT_DISTANCE,
                                                SLVS_FREE_IN_3D,
                                                30.0,
                                                p1.handle, p2.handle, 0, 0));

        # Let's tell the solver to keep the second point as close to constant
        # as possible, instead moving the first point.
        sys.set_dragged(0, 4);
        sys.set_dragged(1, 5);
        sys.set_dragged(2, 6);

        # Now that we have written our system, we solve.
        Slvs_Solve(sys, g);

        if (sys.result == SLVS_RESULT_OKAY):
            print ("okay; now at (%.3f %.3f %.3f)\n" +
                   "             (%.3f %.3f %.3f)") % (
                    sys.get_param(0).val, sys.get_param(1).val, sys.get_param(2).val,
                    sys.get_param(3).val, sys.get_param(4).val, sys.get_param(5).val)
            print "%d DOF" % sys.dof

            self.assertFloatEqual(sys.get_param(0).val,  2.698)
            self.assertFloatEqual(sys.get_param(1).val,  2.698)
            self.assertFloatEqual(sys.get_param(2).val,  2.698)
            self.assertFloatEqual(sys.get_param(3).val, 20.018)
            self.assertFloatEqual(sys.get_param(4).val, 20.018)
            self.assertFloatEqual(sys.get_param(5).val, 20.018)
            self.assertEqual(sys.dof, 5)
        else:
            self.assertTrue(False, "solve failed")

    #-----------------------------------------------------------------------------
    # An example of a constraint in 2d. In our first group, we create a workplane
    # along the reference frame's xy plane. In a second group, we create some
    # entities in that group and dimension them.
    #-----------------------------------------------------------------------------
    def test_example2d(self):
        sys = System()

        g = 1;
        # First, we create our workplane. Its origin corresponds to the origin
        # of our base frame (x y z) = (0 0 0)
        p1 = Point3d(Param(0.0), Param(0.0), Param(0.0), sys)
        # and it is parallel to the xy plane, so it has basis vectors (1 0 0)
        # and (0 1 0).
        #Slvs_MakeQuaternion(1, 0, 0,
        #                    0, 1, 0, &qw, &qx, &qy, &qz);
        qw, qx, qy, qz = Slvs_MakeQuaternion(1, 0, 0,
                                             0, 1, 0)
        wnormal = Normal3d(Param(qw), Param(qx), Param(qy), Param(qz), sys)

        wplane = Workplane(p1, wnormal)

        # Now create a second group. We'll solve group 2, while leaving group 1
        # constant; so the workplane that we've created will be locked down,
        # and the solver can't move it.
        sys.default_group = 2
        # These points are represented by their coordinates (u v) within the
        # workplane, so they need only two parameters each.
        sys.add_param(Slvs_MakeParam(11, g, 10.0));
        sys.add_param(Slvs_MakeParam(12, g, 20.0));
        #p301 = Point2d(wplane, Param(10), Param(20))
        sys.add_entity(Slvs_MakePoint2d(301, g, wplane.handle, 11, 12));
        p301 = H(301)

        sys.add_param(Slvs_MakeParam(13, g, 20.0));
        sys.add_param(Slvs_MakeParam(14, g, 10.0));
        #p302 = Point2d(wplane, Param(20), Param(10))
        sys.add_entity(Slvs_MakePoint2d(302, g, wplane.handle, 13, 14));
        p302 = H(302)

        # And we create a line segment with those endpoints.
        #line = LineSegment2d(wplane, p301, p302)
        sys.add_entity(Slvs_MakeLineSegment(400, g,
                                            200, 301, 302));
        line = H(400)


        # Now three more points.
        sys.add_param(Slvs_MakeParam(15, g, 100.0));
        sys.add_param(Slvs_MakeParam(16, g, 120.0));
        #p303 = Point2d(wplane, Param(100), Param(120))
        sys.add_entity(Slvs_MakePoint2d(303, g, wplane.handle, 15, 16));
        p303 = H(303)

        sys.add_param(Slvs_MakeParam(17, g, 120.0));
        sys.add_param(Slvs_MakeParam(18, g, 110.0));
        #p304 = Point2d(wplane, Param(120), Param(110))
        sys.add_entity(Slvs_MakePoint2d(304, g, wplane.handle, 17, 18));
        p304 = H(304)

        sys.add_param(Slvs_MakeParam(19, g, 115.0));
        sys.add_param(Slvs_MakeParam(20, g, 115.0));
        #p305 = Point2d(wplane, Param(115), Param(115))
        sys.add_entity(Slvs_MakePoint2d(305, g, wplane.handle, 19, 20));
        p305 = H(305)

        # And arc, centered at point 303, starting at point 304, ending at
        # point 305.
        sys.add_entity(Slvs_MakeArcOfCircle(401, g, wplane.handle, wnormal.handle,
                                        p303.handle, p304.handle, p305.handle));

        # Now one more point, and a distance
        sys.add_param(Slvs_MakeParam(21, g, 200.0));
        sys.add_param(Slvs_MakeParam(22, g, 200.0));
        #p306 = Point2d(wplane, Param(200), Param(200))
        sys.add_entity(Slvs_MakePoint2d(306, g, wplane.handle, 21, 22));
        p306 = H(306)

        #p23 = sys.add_param(30.0);
        sys.add_param(Slvs_MakeParam(23, g, 30.0));
        p23 = H(23)
        sys.add_entity(Slvs_MakeDistance(307, g, wplane.handle, p23.handle));

        # And a complete circle, centered at point 306 with radius equal to
        # distance 307. The normal is 102, the same as our workplane.
        sys.add_entity(Slvs_MakeCircle(402, g, wplane.handle,
                                        p306.handle, wnormal.handle, 307));


        # The length of our line segment is 30.0 units.
        sys.add_constraint(Slvs_MakeConstraint(
                                                1, g,
                                                SLVS_C_PT_PT_DISTANCE,
                                                wplane.handle,
                                                30.0,
                                                p301.handle, p302.handle, 0, 0));

        # And the distance from our line segment to the origin is 10.0 units.
        sys.add_constraint(Slvs_MakeConstraint(
                                                2, g,
                                                SLVS_C_PT_LINE_DISTANCE,
                                                wplane.handle,
                                                10.0,
                                                p1.handle, 0, line.handle, 0));

        # And the line segment is vertical.
        sys.add_constraint(Slvs_MakeConstraint(
                                                3, g,
                                                SLVS_C_VERTICAL,
                                                wplane.handle,
                                                0.0,
                                                0, 0, line.handle, 0));
        # And the distance from one endpoint to the origin is 15.0 units.
        sys.add_constraint(Slvs_MakeConstraint(
                                                4, g,
                                                SLVS_C_PT_PT_DISTANCE,
                                                wplane.handle,
                                                15.0,
                                                p301.handle, p1.handle, 0, 0));

        if False:
            # And same for the other endpoint; so if you add this constraint then
            # the sketch is overconstrained and will signal an error.
            sys.add_constraint(Slvs_MakeConstraint(
                                                    5, g,
                                                    SLVS_C_PT_PT_DISTANCE,
                                                    wplane.handle,
                                                    18.0,
                                                    p302.handle, p1.handle, 0, 0));

        # The arc and the circle have equal radius.
        sys.add_constraint(Slvs_MakeConstraint(
                                                6, g,
                                                SLVS_C_EQUAL_RADIUS,
                                                wplane.handle,
                                                0.0,
                                                0, 0, 401, 402));
        # The arc has radius 17.0 units.
        sys.add_constraint(Slvs_MakeConstraint(
                                                7, g,
                                                SLVS_C_DIAMETER,
                                                wplane.handle,
                                                17.0*2,
                                                0, 0, 401, 0));

        # If the solver fails, then ask it to report which constraints caused
        # the problem.
        sys.calculateFaileds = 1;

        # And solve.
        Slvs_Solve(sys, g);

        if(sys.result == SLVS_RESULT_OKAY):
            printf("solved okay");
            printf("line from (%.3f %.3f) to (%.3f %.3f)",
                    sys.get_param(7).val, sys.get_param(8).val,
                    sys.get_param(9).val, sys.get_param(10).val);
            self.assertFloatEqual(sys.get_param( 7).val,  10.000)
            self.assertFloatEqual(sys.get_param( 8).val,  11.180)
            self.assertFloatEqual(sys.get_param( 9).val,  10.000)
            self.assertFloatEqual(sys.get_param(10).val, -18.820)

            printf("arc center (%.3f %.3f) start (%.3f %.3f) finish (%.3f %.3f)",
                    sys.get_param(11).val, sys.get_param(12).val,
                    sys.get_param(13).val, sys.get_param(14).val,
                    sys.get_param(15).val, sys.get_param(16).val);
            self.assertFloatListEqual(
                map(lambda i: sys.get_param(i).val, range(11, 17)),
                [101.114, 119.042, 116.477, 111.762, 117.409, 114.197])

            printf("circle center (%.3f %.3f) radius %.3f",
                    sys.get_param(17).val, sys.get_param(18).val,
                    sys.get_param(19).val);
            printf("%d DOF", sys.dof);
            self.assertFloatEqual(sys.get_param(17).val, 200.000)
            self.assertFloatEqual(sys.get_param(18).val, 200.000)
            self.assertFloatEqual(sys.get_param(19).val,  17.000)

            self.assertEqual(sys.dof, 6)
        else:
            printf("solve failed: problematic constraints are:");
            for i in range(sys.faileds):
                printf(" %lu", sys.failed[i]);
            printf("");
            if (sys.result == SLVS_RESULT_INCONSISTENT):
                printf("system inconsistent");
            else:
                printf("system nonconvergent");
            self.assertTrue(False, "solve failed")

if __name__ == '__main__':
    unittest.main()
