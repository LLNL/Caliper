program fortran_example
    use caliper_mod

    implicit none

    type(ScopeAnnotation) :: m_ann, i_ann
    type(ConfigManager)   :: mgr

    integer               :: i, count, argc

    logical               :: ret
    character(len=:), allocatable :: errmsg
    character(len=256)    :: arg

    ! Initialize Caliper. Use cali_mpi_init() in an MPI program.
    call cali_init()

    ! (Optional) create a ConfigManager object to control profiling.
    ! Users can provide a configuration string (e.g., 'runtime-report')
    ! on the command line.
    mgr = ConfigManager_new()
    argc = command_argument_count()
    if (argc .ge. 1) then
        call get_command_argument(1, arg)
        call mgr%add(arg)
        ret = mgr%error()
        if (ret) then
            errmsg = mgr%error_msg()
            write(*,*) 'ConfigManager: ', errmsg
        endif
    endif

    ! Start configured profiling channels
    call mgr%start

    ! A scope annotation. Start region 'main'
    m_ann = ScopeAnnotation_begin('main')

    ! Add another region 'inner' nested under 'main'
    i_ann = ScopeAnnotation_begin('inner')
    count = 4
    ! End the inner region
    call ScopeAnnotation_end(i_ann)

    ! End 'main'
    call ScopeAnnotation_end(m_ann)

    ! Compute and flush output for the ConfigManager profiles.
    call mgr%flush
    call ConfigManager_delete(mgr)
end program fortran_example
