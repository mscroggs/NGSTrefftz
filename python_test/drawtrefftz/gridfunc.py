#from netgen.geom2d import unit_square
from ngsolve import *
from trefftzngs import *
from netgen.csg import unit_cube
from netgen.geom2d import unit_square

# mesh = Mesh(unit_square.GenerateMesh(maxh=0.1))
# mesh = Mesh("cone/cone.vol.gz")
mesh = Mesh(unit_cube.GenerateMesh(maxh = 0.5)
Draw(mesh)

c = 4
order = 4
k = 4
fes = FESpace("trefftzfespace", mesh, order = order, wavespeed = c, testshift = True)
# fes = L2(mesh, order=order, flags = { "dgjumps" : True })

gfu = GridFunction(fes)


# u = fes.TestFunction()
# v = fes.TrialFunction()

# bfi = SymbolicBFI(u*v)

# gfu.vec[:] = 0
# for id in mesh.Elements(VOL):
# 	fe = fes.GetFE(id)
# 	trafo = mesh.GetTrafo(id)
# 	mat = bfi.CalcElementMatrix(fe, trafo)
# 	for dof in fes.GetDofNrs(id):
# 		gfu.vec[dof] += 1
# 	inv = Matrix(mat.Height(), mat.Width())
# 	try:
# 		mat.Inverse(inv)
# 	except:
# 		print(id.nr, mat)


	#print(inv)
	#print(Norm(inv))
	# print(dir(bfi))
#print(gfu.vec)
#kx
#uex = sin(kx*x+ky*y - c*t)
uex = sin(k*(c*x+y))
# #gfu.Set(CoefficientFunction(0))
gfu.Set(uex)
#
# #print(gfu.vec)
gradu = grad(gfu)
# Draw(uex,mesh,'uex')
# #Draw(gfu, mesh, 'gfu')
Draw(gfu)
Draw(gradu, mesh, 'gradu')