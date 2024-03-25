// Invoke pvar handle allocation routines for pvars bound to some MPI object

{{fn func MPI_Comm_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

#ifdef CALIPER_HAVE_MPIT
    if(mpit_enabled) {
        mpit_allocate_bound_pvar_handles({{2}}, MPI_T_BIND_MPI_COMM); 
    }
#endif
}{{endfn}}

{{fn func MPI_Errhandler_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

#ifdef CALIPER_HAVE_MPIT
    if (mpit_enabled) {
        mpit_allocate_bound_pvar_handles({{1}}, MPI_T_BIND_MPI_ERRHANDLER); 
    }
#endif
}{{endfn}}

{{fn func MPI_File_open}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

#ifdef CALIPER_HAVE_MPIT
    if (mpit_enabled) {
        mpit_allocate_bound_pvar_handles({{4}}, MPI_T_BIND_MPI_FILE); 
    }
#endif
}{{endfn}}

{{fn func MPI_Comm_group}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

#ifdef CALIPER_HAVE_MPIT
    if (mpit_enabled) {
        mpit_allocate_bound_pvar_handles({{1}}, MPI_T_BIND_MPI_GROUP); 
    }
#endif
}{{endfn}}

{{fn func MPI_Op_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

#ifdef CALIPER_HAVE_MPIT
    if (mpit_enabled) {
        mpit_allocate_bound_pvar_handles({{2}}, MPI_T_BIND_MPI_OP); 
    }
#endif
}{{endfn}}

{{fn func MPI_Win_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

#ifdef CALIPER_HAVE_MPIT
    if (mpit_enabled) {
        mpit_allocate_bound_pvar_handles({{5}}, MPI_T_BIND_MPI_WIN); 
    }
#endif
}{{endfn}}

{{fn func MPI_Info_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

#ifdef CALIPER_HAVE_MPIT
    if (mpit_enabled) {
        mpit_allocate_bound_pvar_handles({{0}}, MPI_T_BIND_MPI_INFO); 
    }
#endif
}{{endfn}}
