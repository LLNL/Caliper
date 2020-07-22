program fortran_example
    use caliper_mod

    implicit none

    type(ConfigManager)   :: mgr

    integer               :: i, count, argc

    logical               :: ret
    character(len=:), allocatable :: errmsg
    character(len=256)    :: arg

    ! (Optional) create a ConfigManager object to control profiling.
    ! Users can provide a configuration string (e.g., 'runtime-report')
    ! on the command line.
    mgr = ConfigManager_new()
    call mgr%set_default_parameter('aggregate_across_ranks', 'false')
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

    ! Add another region 'inner' nested under 'main'
    call cali_begin_region('inner')
    count = 4
    ! End the inner region
    call cali_end_region('inner')

    ! End 'main'
    call cali_end_region('main')

    ! Compute and flush output for the ConfigManager profiles.
    call mgr%flush
    call ConfigManager_delete(mgr)
end program fortran_example
