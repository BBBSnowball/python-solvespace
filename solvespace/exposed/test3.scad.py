import math
from slvs  import *
from solid import *


# call to_openscad(), if it exists
def _to_openscad(x):
    if hasattr(x, 'to_openscad'):
        return x.to_openscad()
    elif isinstance(x, list) or isinstance(x, tuple):
        return map(_to_openscad, x)
    else:
        return x

def mat_transpose(m):
    for i in range(4):
        for j in range(4):
            if i < j:
                a = m[i][j]
                b = m[j][i]
                m[i][j] = b
                m[j][i] = a


class Vector(object):
    __slots__ = "xs"

    def __init__(self, *args):
        args = _to_openscad(args)
        if len(args) == 1 and isinstance(args[0], Vector):
            self.xs = list(args[0].xs)
        elif len(args) == 1 and (isinstance(args[0], list) or isinstance(x, tuple)):
            self.xs = list(args[0])
        else:
            self.xs = list(args)

    def __add__(self, v):
        v = Vector(v)
        if len(self.xs) != len(v.xs):
            raise ValueError("vectors must have the same length (self: %d, other: %d)" % (len(self.xs), len(v.xs)))
        return Vector(map(lambda a,b: a+b, self.xs, v.xs))

    def __sub__(self, v):
        v = Vector(v)
        if len(self.xs) != len(v.xs):
            raise ValueError("vectors must have the same length (self: %d, other: %d)" % (len(self.xs), len(v.xs)))
        return Vector(map(lambda a,b: a-b, self.xs, v.xs))

    def __mul__(self, v):
        if not isinstance(v, (int, long, float)):
            raise ValueError("Vectors can only be scaled by a number. Use cross or dot to multiply vectors.")
        return Vector(map(lambda x: x*v, self.xs))

    def cross(self, v):
        v = Vector(v)
        if len(self.xs) != 3 or len(v.xs) != 3:
            raise ValueError("vectors must have length 3")
        a = self.xs
        b = v.xs
        c = [ a[1]*b[2] - a[2]*b[1],
              a[2]*b[0] - a[0]*b[2],
              a[0]*b[1] - a[1]*b[0] ]
        return Vector(c)

    def dot(self, v):
        v = Vector(v)
        if len(self.xs) != len(v.xs):
            raise ValueError("vectors must have the same length (self: %d, other: %d)" % (len(self.xs), len(v.xs)))
        return sum(map(lambda a,b: a*b, self.xs, v.xs))

    def length(self):
        return math.sqrt(sum(map(lambda x: x*x, self.xs)))

    def normalize(self, length = 1.0):
        l = self.length()
        return Vector(map(lambda x: x/l*length, self.xs))

    def to_openscad(self):
        return self.xs

class RenderSystem(object):
    __slots__ = "itemscale"

    def __init__(self):
        self.itemscale = 1

    def point3d(self, pt, size = 0.1):
        pt = Vector(pt)
        size *= self.itemscale
        # We want equilateral triangles. I built that in
        # in SolveSpace and copied the dimensions.
        side = size
        hs = side/2
        height = size*0.8660
        center_to_baseline = size*0.2887
        center_to_top      = size*0.5773
        cb = center_to_baseline
        ct = center_to_top
        triangle = [ [-hs, -cb], [hs, -cb], [0, ct] ]
        pts = [ pt ]
        for z in [-height, +height]:
            for p in triangle:
                pts.append(pt + (p + [z]))
        ts = []
        for ps in [[0,1,2,3], [0,4,5,6]]:
            for a in range(0, 4):
                for b in range(a+1, 4):
                    for c in range(b+1, 4):
                        ts.append([ps[a], ps[b], ps[c]])
        return polyhedron(points = pts,
            #triangles = [[1,2,3],[4,6,5]])
            triangles = ts)

    def line3d(self, a, b, size = 0.01):
        a = Vector(a)
        b = Vector(b)
        length = (b-a).length()
        width = size*self.itemscale
        wh = width/2
        obj = linear_extrude(height = width)(
            polygon([ [0,-wh], [0, wh], [length, wh], [length, -wh] ]))
        return multmatrix(move_and_rotate(a, b, a+[0,0,1]))(obj)

    def system(self, system):
        obj = union()

        for i in range(system.entities):
            t = system.entity_type(i)
            if t == SLVS_E_POINT_IN_3D:
                obj.add(self.point3d(system.get_Point3d(i)))
            elif t == SLVS_E_LINE_SEGMENT:
                line = system.get_LineSegment3d(i)
                obj.add(self.line3d(line.a(), line.b()))

        return obj

r = RenderSystem()

p1 = [2,  0,  0]
p2 = [0,  0,  0]
p3 = [2,  4,  2]
p4 = [-3, 2, -2]

pts = [ p1, p2, p3, p4 ]

#obj = r.point3d([2, 0, 0]) + r.point3d([0, 0, 0]) + linear_extrude(height = 0.5)(polygon([[0,0,0], [2,0,0], [0,2,0]]))
obj = union()(map(r.point3d, pts)) + r.line3d(p3, p4)

sys = System()
p1s = Point3d(*(p1 + [sys]))
p2s = Point3d(*(p2 + [sys]))
p3s = Point3d(*(p3 + [sys]))
p4s = Point3d(*(p4 + [sys]))
line = LineSegment3d(p3s, p4s)

#obj = r.system(sys)

scad_render_to_file(obj, "/tmp/out.scad")
