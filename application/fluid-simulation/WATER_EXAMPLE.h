//#####################################################################
// Copyright 2009, Michael Lentine.
// This file is part of PhysBAM whose distribution is governed by the license contained in the accompanying file PHYSBAM_COPYRIGHT.txt.
//#####################################################################
#ifndef __WATER_EXAMPLE__
#define __WATER_EXAMPLE__
#include <PhysBAM_Tools/Grids_Uniform_Advection/ADVECTION_SEMI_LAGRANGIAN_UNIFORM.h>
#include <PhysBAM_Tools/Grids_Uniform_Arrays/ARRAYS_ND.h>
#include <PhysBAM_Tools/Grids_Uniform_Boundaries/BOUNDARY_UNIFORM.h>
#include <PhysBAM_Tools/Grids_Uniform_PDE_Linear/PROJECTION_UNIFORM.h>
#include <PhysBAM_Tools/Read_Write/Point_Clouds/READ_WRITE_POINT_CLOUD.h>
#include <PhysBAM_Tools/Read_Write/Utilities/FILE_UTILITIES.h>
#include <PhysBAM_Tools/Vectors/VECTOR.h>
#include <PhysBAM_Geometry/Grids_Uniform_Collisions/GRID_BASED_COLLISION_GEOMETRY_COLLECTION_POLICY_UNIFORM.h>
#include <PhysBAM_Geometry/Grids_Uniform_Collisions/GRID_BASED_COLLISION_GEOMETRY_UNIFORM.h>
#include <PhysBAM_Geometry/Grids_Uniform_Level_Sets/LEVELSET_POLICY_UNIFORM.h>
#include <PhysBAM_Fluids/PhysBAM_Incompressible/Boundaries/GEOMETRY_BOUNDARY_POLICY.h>
#include <PhysBAM_Fluids/PhysBAM_Incompressible/Incompressible_Flows/INCOMPRESSIBLE_UNIFORM.h>
#include <PhysBAM_Dynamics/Boundaries/BOUNDARY_PHI_WATER.h>
#include <PhysBAM_Dynamics/Level_Sets/LEVELSET_CALLBACKS.h>
#include <PhysBAM_Dynamics/Level_Sets/PARTICLE_LEVELSET_EVOLUTION_UNIFORM.h>

namespace PhysBAM
{

template<class T_GRID> class LEVELSET_MULTIPLE_UNIFORM;

//TODO: Should adventually derive off of a incompressible project
template<class TV>
class WATER_EXAMPLE:public LEVELSET_CALLBACKS<GRID<TV> >,RIGID_GEOMETRY_EXAMPLE_VELOCITIES<TV>
{
    typedef typename TV::SCALAR T;
    typedef typename TV::template REBIND<int>::TYPE TV_INT;
    typedef typename LEVELSET_POLICY<GRID<TV> >::LEVELSET T_LEVELSET;
    typedef typename LEVELSET_POLICY<GRID<TV> >::PARTICLE_LEVELSET T_PARTICLE_LEVELSET;
    typedef typename GEOMETRY_BOUNDARY_POLICY<GRID<TV> >::BOUNDARY_PHI_WATER T_BOUNDARY_PHI_WATER;
    typedef typename COLLISION_GEOMETRY_COLLECTION_POLICY<GRID<TV> >::GRID_BASED_COLLISION_GEOMETRY T_GRID_BASED_COLLISION_GEOMETRY;
    enum workaround1{d=TV::m};

public:

    // TODO: convert to scheduler variables/ worker variables

    STREAM_TYPE stream_type;
    T initial_time;
    int first_frame,last_frame;
    T frame_rate;
    std::string frame_title;
    int write_substeps_level;
    bool write_output_files;
    std::string output_directory;
    int restart;
    int number_of_ghost_cells;
    int test_number;

    T cfl;

    GRID<TV> mac_grid;
    MPI_UNIFORM_GRID<GRID<TV> > *mpi_grid;
    THREAD_QUEUE* thread_queue;
    PROJECTION_DYNAMICS_UNIFORM<GRID<TV> >& projection;
    PARTICLE_LEVELSET_EVOLUTION_UNIFORM<GRID<TV> > particle_levelset_evolution;
    INCOMPRESSIBLE_UNIFORM<GRID<TV> > incompressible;
    ARRAY<T,FACE_INDEX<TV::dimension> > face_velocities;
    ADVECTION_SEMI_LAGRANGIAN_UNIFORM<GRID<TV>,T> advection_scalar;
    BOUNDARY_UNIFORM<GRID<TV>,T> boundary_scalar;
    BOUNDARY_UNIFORM<GRID<TV>,T> *boundary,*phi_boundary;
    T_BOUNDARY_PHI_WATER phi_boundary_water;
    VECTOR<VECTOR<bool,2>,TV::dimension> domain_boundary;
    RIGID_GEOMETRY_COLLECTION<TV> rigid_geometry_collection;
    T_GRID_BASED_COLLISION_GEOMETRY collision_bodies_affecting_fluid;
    ARRAY<IMPLICIT_OBJECT<TV>*> sources;

    WATER_EXAMPLE(const STREAM_TYPE stream_type_input,int number_of_threads=1,int refine=0);
    virtual ~WATER_EXAMPLE();

    // TODO: What to do about the following?

    void Initialize_Grid(TV_INT counts,RANGE<TV> range);
    void Initialize_Phi();

    // following functions are required by the driver

    T Time_At_Frame(const int frame) const
    {
        return initial_time+(frame-first_frame)/frame_rate;
    }

    void Set_Boundary_Conditions(const T time);
    void Adjust_Phi_With_Sources(const T time);
    void Adjust_Phi_With_Objects(const T time);
    void Extrapolate_Phi_Into_Objects(const T time);

    void Write_Output_Files(const int frame);
    void Read_Output_Files(const int frame);

//#####################################################################
};

}
#endif
