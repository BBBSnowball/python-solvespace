import math
from slvs  import *
from solid import *

sys = System()

# We want to find the plane for three points. The points
# shouldn't change, so we put them into another group.

p1 = Point3d(Param(1), Param(1), Param(9), sys)
p2 = Point3d(Param(5), Param(2), Param(2), sys)
p3 = Point3d(Param(0), Param(7), Param(5), sys)

#p1 = Point3d(Param(0), Param(0), Param(1), sys)
#p2 = Point3d(Param(5), Param(0), Param(1), sys)
#p3 = Point3d(Param(0), Param(7), Param(2), sys)

# Other entities go into another group
sys.default_group = 2

wrkpl = Workplane(p1, Normal3d(Param(1), Param(0), Param(0), Param(0), sys))

# Some constraints: all points are in the plane
# (p1 is its origin, so we don't need a constraint)
Constraint.on(wrkpl, p2)
Constraint.on(wrkpl, p3)

# Solve it (group 2 is still active)
sys.solve()

if(sys.result == SLVS_RESULT_OKAY):
    print "solved okay"
    for p in [p1, p2, p3]:
    	print "point at (%.3f, %.3f, %.3f)" % tuple(p.to_openscad())
    print "%d DOF" % sys.dof
else:
    print("solve failed")


# draw some geometry, so we see the result

geom = union()

# a small triangle for each point
for p in [p1, p2, p3]:
	x = p.x().value
	y = p.y().value
	z = p.z().value

	w = 0.3

	geom.add(
		linear_extrude(height = z)(
			polygon([[x,y], [x-w,y], [x,y-w]])))

# draw plane: create flat surface in xy-plane and rotate+translate

plane = linear_extrude(height = 0.3)(
	square(size = [8, 8]))

# rotate and translate
q = [wrkpl.normal().qw().value, wrkpl.normal().qx().value,
     wrkpl.normal().qy().value, wrkpl.normal().qz().value]
m = [ Slvs_QuaternionU(*q) + [0],
      Slvs_QuaternionV(*q) + [0],
      Slvs_QuaternionN(*q) + [0],
      [0, 0, 0, 1] ]
def transpose(m):
	for i in range(4):
		for j in range(4):
			if i < j:
				a = m[i][j]
				b = m[j][i]
				m[i][j] = b
				m[j][i] = a
				print (i,j)
transpose(m)
m[0][3] = wrkpl.origin().x().value
m[1][3] = wrkpl.origin().y().value
m[2][3] = wrkpl.origin().z().value
plane1 = multmatrix(m)(plane)

geom += plane1



# This time we do it without the solver.

# Move points, so the second plane won't overlap the first
p1.z().value += 2
p2.z().value += 2
p3.z().value += 2

origin = [ p1.x().value, p1.y().value, p1.z().value ]
v1 = [ p2.x().value - origin[0], p2.y().value - origin[1], p2.z().value - origin[2] ]
v2 = [ p3.x().value - origin[0], p3.y().value - origin[1], p3.z().value - origin[2] ]

def cross(a, b):
	return [ a[1]*b[2] - a[2]*b[1],
	         a[2]*b[0] - a[0]*b[2],
	         a[0]*b[1] - a[1]*b[0] ]
def normalize(v):
	l = math.sqrt(sum(map(lambda x: x*x, v)))
	return map(lambda x: x/l, v)

# make sure they have length 1.0
v1 = normalize(v1)
v2 = normalize(v2)

# third vector is perpendicular
v3 = cross(v1, v2)

# we calculate the second vector again to make sure
# that v1 and v2 are perpendicular
v2 = cross(v3, v1)

m = [ v1 + [0],
      v2 + [0],
      v3 + [0],
      [0, 0, 0, 1] ]
transpose(m)
m[0][3] = origin[0]
m[1][3] = origin[1]
m[2][3] = origin[2]
plane2 = multmatrix(m)(plane)
geom += plane2

scad_render_to_file(geom, "/tmp/test2.scad")
