#include <PhysBAM_Tools/Parallel_Computation/THREAD_QUEUE.h>
#include <PhysBAM_Tools/Parsing/PARSE_ARGS.h>
#include <PhysBAM_Geometry/Basic_Geometry/CYLINDER.h>
#include <PhysBAM_Geometry/Implicit_Objects/ANALYTIC_IMPLICIT_OBJECT.h>
#include <PhysBAM_Geometry/Solids_Geometry/RIGID_GEOMETRY.h>
#include "WATER_DRIVER.h"
#include "WATER_EXAMPLE.h"

using namespace PhysBAM;

template<class T>
void Add_Source(WATER_EXAMPLE<VECTOR<T,1> >* example)
{
    PHYSBAM_FATAL_ERROR();
}

template<class T>
void Add_Source(WATER_EXAMPLE<VECTOR<T,2> >* example)
{
    typedef VECTOR<T,2> TV;
    TV point1,point2;BOX<TV> source;
    //point1=TV::All_Ones_Vector()*(T).5;point1(1)=.4;point1(2)=.95;point2=TV::All_Ones_Vector()*(T).65;point2(1)=.55;point2(2)=1;
    point1=TV::All_Ones_Vector()*(T).5;point1(1)=.95;point1(2)=.6;point2=TV::All_Ones_Vector()*(T).65;point2(1)=1;point2(2)=.75;
    source.min_corner=point1;source.max_corner=point2;
    example->sources.Append(new ANALYTIC_IMPLICIT_OBJECT<BOX<TV> >(source));
}

template<class T>
void Add_Source(WATER_EXAMPLE<VECTOR<T,3> >* example)
{
    typedef VECTOR<T,3> TV;
    TV point1,point2;CYLINDER<T> source;
    //point1=TV::All_Ones_Vector()*(T).6;point1(1)=.4;point1(2)=.95;point2=TV::All_Ones_Vector()*(T).6;point2(1)=.4;point2(2)=1;
    point1=TV::All_Ones_Vector()*(T).8;point1(1)=.4;point1(3)=.95;point2=TV::All_Ones_Vector()*(T).8;point2(1)=.4;point2(3)=1;
    source.Set_Endpoints(point1,point2);source.radius=.1;
    IMPLICIT_OBJECT<TV>* analytic=new ANALYTIC_IMPLICIT_OBJECT<CYLINDER<T> >(source);
    example->sources.Append(analytic);
}

int main(int argc,char *argv[])
{
    typedef float T;
    typedef float RW;
    STREAM_TYPE stream_type((RW()));
    typedef VECTOR<T,2> TV;
    //typedef VECTOR<T,3> TV;
    typedef VECTOR<int,TV::dimension> TV_INT;


    PARSE_ARGS parse_args;
    parse_args.Add_Integer_Argument("-restart",0,"restart frame");
    parse_args.Add_Integer_Argument("-scale",128,"fine scale grid resolution");
    parse_args.Add_Integer_Argument("-substep",-1,"output-substep level");
    parse_args.Add_Integer_Argument("-e",100,"last frame");
    parse_args.Add_Integer_Argument("-refine",1,"refine levels");
    parse_args.Add_Integer_Argument("-threads",1,"number of threads");
    parse_args.Add_Double_Argument("-cfl",1,"cfl number");

    LOG::Initialize_Logging(false,false,1<<30,true,parse_args.Get_Integer_Value("-threads"));

    parse_args.Parse(argc,argv);
    parse_args.Print_Arguments(argc,argv);
    
    WATER_EXAMPLE<TV>* example=new WATER_EXAMPLE<TV>(stream_type,parse_args.Get_Integer_Value("-refine"));

    int scale=parse_args.Get_Integer_Value("-scale");
    example->Initialize_Grid(TV_INT::All_Ones_Vector()*scale,RANGE<TV>(TV(),TV::All_Ones_Vector()));
    example->last_frame=parse_args.Get_Integer_Value("-e");
    example->write_substeps_level=parse_args.Get_Integer_Value("-substep");
    example->cfl=parse_args.Get_Double_Value("-cfl");
    Add_Source(example);
    //Add_Sphere(example);

    // Custom Partition
    TV_INT ppd=TV_INT::All_Ones_Vector();
    ppd(1)=4;

    FILE_UTILITIES::Create_Directory(example->output_directory+"/common");
    LOG::Instance()->Copy_Log_To_File(example->output_directory+"/common/log.txt",false);
    
    WATER_DRIVER<TV> driver(*example);

    driver.Execute_Main_Program();

    return 0;
}
