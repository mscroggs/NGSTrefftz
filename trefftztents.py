from netgen.geom2d import unit_square
from netgen.csg import unit_cube
from ngsolve.TensorProductTools import *
from ngsolve import *
import time
import math
from trefftzngs import *
# SetHeapSize(1000*1000*1000)


def SolveTrefftzTents(mesh, order, finaltime):
    D = initmesh.dim
    t = CoordCF(D)

    if D==3:
        sq = sqrt(3.0);
        bdd = CoefficientFunction((
            sin(math.pi*x)*sin(math.pi*y)*sin(math.pi*z)*sin(math.pi*t*c*sq)/(sq*math.pi),
            cos(math.pi*x)*sin(math.pi*y)*sin(math.pi*z)*sin(math.pi*t*c*sq)/sq,
            sin(math.pi*x)*cos(math.pi*y)*sin(math.pi*z)*sin(math.pi*t*c*sq)/sq,
            sin(math.pi*x)*sin(math.pi*y)*cos(math.pi*z)*sin(math.pi*t*c*sq)/sq,
            sin(math.pi*x)*sin(math.pi*y)*sin(math.pi*z)*cos(math.pi*t*c*sq)*c
            ))
    elif D==2:
        sq = sqrt(2.0);
        bdd = CoefficientFunction((
            sin(math.pi*x)*sin(math.pi*y)*sin(math.pi*t*c*sq)/(sq*math.pi),
            cos(math.pi*x)*sin(math.pi*y)*sin(math.pi*t*c*sq)/sq,
            sin(math.pi*x)*cos(math.pi*y)*sin(math.pi*t*c*sq)/sq,
            sin(math.pi*x)*sin(math.pi*y)*cos(math.pi*t*c*sq)*c
            ))
    elif D==1:
        sq = sqrt(1.0);
        bdd = CoefficientFunction((
            sin(math.pi*x)*sin(math.pi*t*c*sq)/(sq*math.pi),
            cos(math.pi*x)*sin(math.pi*t*c*sq)/sq,
            sin(math.pi*x)*cos(math.pi*t*c*sq)*c
            ))



    wavefront = EvolveTentsMakeWavefront(order,initmesh,t_start,bdd)

    start = time.time()
    with TaskManager():
        wavefront = EvolveTents(order,initmesh,c,t_step,wavefront,t_start,bdd)
    timing = (time.time()-start)
    print("time ",time.time()-start)
    error = EvolveTentsError(order,initmesh,wavefront,EvolveTentsMakeWavefront(order,initmesh,t_step,bdd))
    print("error ", error)
    adiam = EvolveTentsAdiam(initmesh,1,t_step)

    return [error, timing, adiam]


if __name__ == '__main__':
    order = 3
    SetNumThreads(2)

    c = 1
    t_start = 0
    t_step = 2/sqrt(3)

    h1error = []
    adiam = []
    timer = []
# ms = [0.4,0.3,0.2,0.1]
    ms = [0,1,2]

    meshes=[ Mesh(SegMesh(4,0,1)), Mesh(unit_square.GenerateMesh(maxh = 0.4)), Mesh(unit_cube.GenerateMesh(maxh = 1))]
    for initmesh in meshes:
        h1error = []
        adiam = []
        timer = []
        for maxh in ms:
            print("RUN: ", maxh)
            # initmesh = Mesh(unit_cube.GenerateMesh(maxh = maxh))
            [error,timing,tentdiam] =  SolveTrefftzTents(initmesh, order, t_step)
            h1error.append(error)
            adiam.append(tentdiam)
            timer.append(timing)

            if maxh != ms[-1] and initmesh.dim!=1:
                initmesh.Refine()
            elif initmesh.dim==1:
                initmesh=Mesh(SegMesh(initmesh.ne*2,0,1))

        print("====================",initmesh.dim,"====================")
        for i in range(len(ms)):
            print("adiam ",adiam[i]," maxh ", ms[i]," error ", h1error[i], " time ", timer[i])
        print("rate")
        for i in range(len(ms)-1):
            print(math.log(h1error[i]/h1error[i+1])/math.log(2))
            # print(log(h1error[i]/h1error[i+1])/log(ms[i]/ms[i+1]))
        print("adiam rate")
        for i in range(len(ms)-1):
            print(math.log(h1error[i]/h1error[i+1])/math.log(adiam[i]/adiam[i+1]))
        print("===========================================")
