#include "evolvetent.hpp"
#include "trefftzelement.hpp"
#include "tents/tents.hpp"
#include <comp.hpp>    // provides FESpace, ...
#include <h1lofe.hpp>
#include <regex>
#include <fem.hpp>
#include <multigrid.hpp>


namespace ngcomp
{

    template<int D>
    Vec<D+2> TestSolution(Vec<D+1> p)
    {
        double x = p[0]; double t = p[D];
        int wavespeed = 1;
        Vec<D+2> sol;
        int k = 3;
        if(D==1){
            sol[0] =  sin( k*(wavespeed*t + x) );
            sol[1] = -k*cos(k*(wavespeed*t+x));
            sol[2] = wavespeed*k*cos(k*(wavespeed*t+x));
        } else if(D==2) {
            double y = p[1];
            double sq = sqrt(0.5);
            sol[0] = sin( wavespeed*t+sq*(x+y) );
            sol[1] = -sq*cos(wavespeed*t+sq*(x+y));
            sol[2] = -sq*cos(wavespeed*t+sq*(x+y));
            sol[3] = wavespeed*cos(wavespeed*t+sq*(x+y));
        }
        return sol;
    }

    template<int D, typename TFUNC>
    Vector<> MakeWavefront(IntegrationRule ir, shared_ptr<MeshAccess> ma, LocalHeap& lh, TFUNC func, double time){
        Vector<> ic(ir.Size() * ma->GetNE() * (D+2));
        for(int elnr=0;elnr<ma->GetNE();elnr++){
            MappedIntegrationRule<1,D> mir(ir, ma->GetTrafo(elnr,lh), lh); // <dim  el, dim space>
            for(int imip=0;imip<mir.Size();imip++)
            {
                Vec<D+1> p;
                p.Range(0,D+1) = mir[imip].GetPoint();
                p[D] = time;
                int offset = elnr*ir.Size()*(D+2) + imip*(D+2);
                ic.Range(offset,offset+D+2) = func(p);
            }
        }
        return ic;
    }

    template<int D>
    Vector<> EvolveTents(int order, shared_ptr<MeshAccess> ma, double wavespeed, double dt, Vector<> wavefront)
    {
        LocalHeap lh(500000);
        T_TrefftzElement<D+1> tel(order,wavespeed);
        int nbasis = tel.GetNBasis();

        const ELEMENT_TYPE eltyp = D==1 ? ET_SEGM : ET_TRIG;
        IntegrationRule ir(eltyp, order+2);
        ScalarFE<eltyp,1> faceint;

        TentPitchedSlab<D> tps = TentPitchedSlab<D>(ma);      // collection of tents in timeslab
        tps.PitchTents(dt, wavespeed); // adt = time slab height, wavespeed

        if(wavefront.Size() == 0)
        {
            wavefront(ir.Size() * ma->GetNE() * (D+2));
            wavefront = MakeWavefront<D>(ir,ma,lh,TestSolution<D>,0);
        }

        RunParallelDependency (tps.tent_dependency, [&] (int i) {
            // LocalHeap slh = lh.Split();  // split to threads
            HeapReset hr(lh);
            Tent* tent = tps.tents[i];
            //cout << endl << "%%%% tent: " << i << " vert: " << tent->vertex << " els: " << tent->els << endl;
            //cout << *tent << endl;

            //Vec<D+1> center;
            //center.Range(0,D)=ma->GetPoint<D>(tent->vertex);
            //center[D]=(tent->ttop-tent->tbot)/2+tent->tbot;
            //double size = (tent->ttop-tent->tbot)*(tent->ttop-tent->tbot);
            //tel.SetCenter(center);
            //tel.SetElSize(size);

            FlatMatrix<> elmat(nbasis,lh);
            FlatVector<> elvec(nbasis,lh);
            elmat = 0; elvec = 0;
            for(auto elnr: tent->els)
            {
                INT<D+1> vnr = ma->GetEdgePNums(elnr);
                MappedIntegrationRule<1,D> mir(ir, ma->GetTrafo(elnr,lh), lh); // <dim  el, dim space>

                // Integration over top of tent
                Mat<D+1,D+1> v = TentFaceVerts<D>(tent, elnr, ma, 1);
                Vec<D+1> n = TentFaceNormal<D>(v,1);
                Vec<D+1> bs = v.Row(D);
                double A = TentFaceArea<D>(v);
                for(int imip=0;imip<mir.Size();imip++)
                {
                    Vec<D+1> p;
                    p.Range(0,D) = mir[imip].GetPoint();
                    p(D) = faceint.Evaluate(ir[imip], bs);

                    FlatMatrix<> dshape(nbasis,D+1,lh);
                    tel.CalcDShape(p,dshape);
                    FlatVector<> shape(nbasis,lh);
                    tel.CalcShape(p,shape);

                    double weight = A*ir[imip].Weight();
                    for(int i=0;i<nbasis;i++)
                    {
                        for(int j=0;j<nbasis;j++)
                        {
                            Vec<D> sig = -dshape.Row(i).Range(0,D);
                            Vec<D> tau = -dshape.Row(j).Range(0,D);
                            elmat(j,i) += weight * dshape(i,D)*dshape(j,D)*n(D) * (1/(wavespeed*wavespeed));
                            elmat(j,i) += weight * InnerProduct(sig,tau)*n(D);
                            elmat(j,i) += weight * dshape(i,D)*InnerProduct(tau,n.Range(0,D));
                            elmat(j,i) += weight * dshape(j,D)*InnerProduct(sig,n.Range(0,D));
                        }
                    }
                }

                // Integration over bot of tent
                v = TentFaceVerts<D>(tent, elnr, ma, 0);
                n = TentFaceNormal<D>(v,0);
                bs = v.Row(D);
                A = TentFaceArea<D>(v);
                for(int imip=0;imip<mir.Size();imip++)
                {
                    Vec<D+1> p;
                    p.Range(0,D) = mir[imip].GetPoint();
                    p(D) = faceint.Evaluate(ir[imip], bs);

                    FlatMatrix<> dshape(nbasis,D+1,lh);
                    tel.CalcDShape(p,dshape);
                    FlatVector<> shape(nbasis,lh);
                    tel.CalcShape(p,shape);

                    int offset = elnr*ir.Size()*(D+2) + imip*(D+2);
                    double weight = A*ir[imip].Weight();
                    for(int j=0;j<nbasis;j++)
                    {
                        Vec<D> tau = -dshape.Row(j).Range(0,D);
                        elvec(j) -= weight * wavefront(offset+D+1)*dshape(j,D)*n(D) * (1/(wavespeed*wavespeed));
                        elvec(j) -= weight * InnerProduct(wavefront.Range(offset+1,offset+D+1),tau)*n(D);
                        elvec(j) -= weight * wavefront(offset+D+1)*InnerProduct(tau,n.Range(0,D));
                        elvec(j) -= weight * dshape(j,D)*InnerProduct(wavefront.Range(offset+1,offset+D+1),n.Range(0,D));
                        elvec(j) += weight * wavefront(offset)*shape(j);

                        for(int i=0;i<nbasis;i++)
                        {
                            elmat(j,i) += weight * ( shape(i)*shape(j) );
                        }
                    }
                }
            } // close loop over tent elements


            //Integrate over side of tent
            for(auto surfel : ma->GetVertexSurfaceElements(tent->vertex))
            {
                for(int imip=0;imip<ir.Size();imip++)
                {
                    double A;
                    Vec<D+1> n;
                    Vec<D+1> p;
                    if(D==1)
                    {
                        A = tent->ttop - tent->tbot;
                        n = sgn_nozero<int>(tent->vertex - tent->nbv[0]); n(D) = 0; n /= L2Norm(n);
                        p.Range(0,D) = ma->GetPoint<D>(tent->vertex);
                        p(D) = A*ir[imip].Point()[0] + tent->tbot;
                    }
                    else if(D==2) 
                    {
                        auto sel_verts = ma->GetElVertices(ElementId(BND,surfel));
                        int nbv = tent->vertex==sel_verts[0] ? sel_verts[1] : sel_verts[0];
                        Mat<D+1,D+1> v = 0;
                        for(int i=0;i<D;i++)
                            v.Col(i).Range(0,D) = ma->GetPoint<D>(tent->vertex);
                        v.Col(D).Range(0,D) = ma->GetPoint<D>(nbv);
                        v(D,0) = tent->ttop;
                        v(D,1) = tent->tbot;
                        v(D,2) = tent->nbtime[tent->nbv.Pos(nbv)];
                        A = TentFaceArea<D>(v);

                        n.Range(0,D) =  ma->GetPoint<D>(sel_verts[0]) - ma->GetPoint<D>(sel_verts[1]);
                        n[2] = n[0]; n[0] = n[1]; n[1] = n[2]; n[2] = 0;
                        n[0] = -n[0];
                        n /= L2Norm(n);

                        Mat<D+1,D> map;
                        map.Col(0) = v.Col(1)-v.Col(0);
                        map.Col(1) = v.Col(2)-v.Col(0);
                        p = map * ir[imip].Point() + v.Col(0);
                    }
                    FlatMatrix<> dshape(nbasis,D+1,lh);
                    tel.CalcDShape(p,dshape);
                    double weight = A*ir[imip].Weight();
                    for(int j=0;j<nbasis;j++)
                    {
                        Vec<D> tau = -dshape.Row(j).Range(0,D);
                        elvec(j) -= weight * InnerProduct(tau,n.Range(0,D)) * TestSolution<D>(p)[2];
                        for(int i=0;i<nbasis;i++)
                        {
                            Vec<D> sig = -dshape.Row(i).Range(0,D);
                            elmat(j,i) += weight * InnerProduct(sig,n.Range(0,D)) * dshape(j,D);
                        }
                    }
                }
            }

            CalcInverse(elmat);
            Vector<> sol = elmat*elvec;

            // eval solution on top of tent
            for(auto elnr: tent->els)
            {
                INT<D+1> vnr = ma->GetEdgePNums(elnr);
                MappedIntegrationRule<1,D> mir(ir, ma->GetTrafo(elnr,lh), lh); // <dim  el, dim space>

                Mat<D+1,D+1> v = TentFaceVerts<D>(tent, elnr, ma, 1);
                Vec<D+1> n = TentFaceNormal<D>(v,1);
                Vec<D+1> bs = v.Row(D);
                double A = TentFaceArea<D>(v);
                for(int imip=0;imip<mir.Size();imip++)
                {
                    Vec<D+1> p;
                    p.Range(0,D) = mir[imip].GetPoint();
                    p(D) = faceint.Evaluate(ir[imip], bs);

                    Matrix<> dshape(nbasis,D+1);
                    tel.CalcDShape(p,dshape);
                    Vector<> shape(nbasis);
                    tel.CalcShape(p,shape);

                    int offset = elnr*ir.Size()*(D+2) + imip*(D+2);
                    wavefront(offset) = InnerProduct(shape,sol);
                    wavefront.Range(offset+1,offset+D+2) = Trans(dshape)*sol;
                    wavefront.Range(offset+1,offset+D+1) *= (-1);

                    //cout << "at " << p << " value: " <<endl<< wavefront.Range(offset,offset+D+2) << endl; //InnerProduct(sol,shape) << endl << Trans(dshape)*sol << endl;
                    //cout << "corr sol: " << TestSolution<D>(p) << endl;
                }
            }
        }); // end loop over tents

        Vector<> wavefront_corr = MakeWavefront<D>(ir,ma,lh,TestSolution<D>, dt);
        double l2error=0;
        for(int elnr=0;elnr<ma->GetNE();elnr++)
        {
            MappedIntegrationRule<1,D> mir(ir, ma->GetTrafo(elnr,lh), lh); // <dim  el, dim space>
            for(int imip=0;imip<ir.Size();imip++)
            {
                int offset = elnr*ir.Size()*(D+2) + imip*(D+2);
                l2error += (wavefront(offset)-wavefront_corr(offset))*(wavefront(offset)-wavefront_corr(offset))*mir[imip].GetWeight();
            }
        }
        l2error = sqrt(l2error);
        cout << "L2 Error: " << l2error<< endl;

        return wavefront;
    }

    // returns matrix where cols correspond to vertex coordinates of the space-time element
    template<int D>
    Mat<D+1,D+1> TentFaceVerts(Tent* tent, int elnr, shared_ptr<MeshAccess> ma, bool top)
    {
        INT<D+1> vnr = ma->GetElVertices(elnr);
        Mat<D+1, D+1> v;
        // determine linear basis function coeffs to use for tent face
        for(int ivert = 0;ivert<vnr.Size();ivert++)
        {
            if(vnr[ivert] == tent->vertex) v(D,ivert) =  top ? tent->ttop : tent->tbot;
            for (int k = 0; k < tent->nbv.Size(); k++)
                if(vnr[ivert] == tent->nbv[k]) v(D,ivert) = tent->nbtime[k];
            v.Col(ivert).Range(0,D) = ma->GetPoint<D>(vnr[ivert]);
        }

        return v;
    }

    template<int D>
    double TentFaceArea( Mat<D+1,D+1> v )
    {
        switch(D) {
            case 1 : return L2Norm(v.Col(0)-v.Col(1));
                     break;
            case 2 :{
                        double a = L2Norm(v.Col(0)-v.Col(1));
                        double b = L2Norm(v.Col(1)-v.Col(2));
                        double c = L2Norm(v.Col(0)-v.Col(2));
                        double s = 0.5*(a+b+c);
                        return sqrt(s*(s-a)*(s-b)*(s-c));
                        break;
                    }
        }
    }

    template<int D>
    Vec<D+1> TentFaceNormal( Mat<D+1,D+1> v, bool top )
    {
        Vec<D+1> normv;
        switch(D){
            case 1: {
                        normv(0) = v(1,0)-v(1,1);
                        normv(1) = v(0,1)-v(0,0);
                        normv /= L2Norm(normv);
                        break;
                    }
            case 2: {
                        Vec<D+1> a = v.Col(0)-v.Col(1);
                        Vec<D+1> b = v.Col(0)-v.Col(2);
                        normv(0) = a(1) * b(2) - a(2) * b(1);
                        normv(1) = a(2) * b(0) - a(0) * b(2);
                        normv(2) = a(0) * b(1) - a(1) * b(0);
                        normv /= sqrt(L2Norm2(a)*L2Norm2(b)- (a[0]*b[0]+a[1]*b[1]+a[2]*b[2])*(a[0]*b[0]+a[1]*b[1]+a[2]*b[2]));
                        break;
                    }
        }
        if(top == 1) normv *= sgn_nozero<double>(normv[D]);
        else normv *= (-sgn_nozero<double>(normv[D]));
        return normv;
    }
}

#ifdef NGS_PYTHON
#include <python_ngstd.hpp>
void ExportEvolveTent(py::module m)
{
    m.def("EvolveTents", [](int order, shared_ptr<MeshAccess> ma, double wavespeed, double dt) //-> shared_ptr<MeshAccess>
          {
              Vector<> wavefront;
              if(ma->GetDimension() == 1)
                  EvolveTents<1>(order,ma,wavespeed,dt,wavefront);
              else if(ma->GetDimension() == 2)
                  EvolveTents<2>(order,ma,wavespeed,dt,wavefront);

          }//, py::call_guard<py::gil_scoped_release>()
         );
}
#endif // NGS_PYTHON

