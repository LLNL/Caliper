program fortran_example
    use caliper_mod

    implicit none

    type(BufferedRegionProfile) :: rp
    type(ConfigManager)   :: mgr

    integer               :: i, count, argc
    real                  :: innertime, outertime, tottime

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
    call cali_begin_region('main')

    ! Create a BufferedRegionProfile instance to query Caliper region times
    rp = BufferedRegionProfile_new()

    do i = 1, 4
        ! Start the region profile
        call rp%start()

        ! Add region 'outer' using cali_begin_region()
        call cali_begin_region('outer')
        ! Add another region 'inner' nested under 'outer'
        call cali_begin_region('inner')

        call cali_end_region('inner')
        call cali_end_region('outer')

        ! Stop the region profile and fetch region times 
        call rp%stop
        call rp%fetch_inclusive_region_times
        innertime = rp%region_time('inner')
        outertime = rp%region_time('outer')
        tottime   = rp%total_region_time()

        write(*,*) 'Cycle ', i, ': ', tottime, ' sec'
        write(*,*) 'outer:   ', outertime, ' sec'
        write(*,*) '  inner: ', innertime, ' sec'

        ! Reset the profile
        call rp%clear
    end do

    ! Delete the RegionProfile instance
    call BufferedRegionProfile_delete(rp)

    call cali_end_region('main')
  
    ! Compute and flush output for the ConfigManager profiles.
    call mgr%flush
    call ConfigManager_delete(mgr)
end program fortran_example
