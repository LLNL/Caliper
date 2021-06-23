program fortran_example
    use caliper_mod
    use iso_c_binding, ONLY : C_INT64_T

    implicit none

    type(ConfigManager)   :: mgr

    integer               :: i, count, argc
    integer(C_INT64_T)    :: loop_attribute, iter_attribute

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

    count = 4

    !   Annotate a loop. We'll have to find Caliper's built-in "loop"
    ! attribute first and create a loop iteration attribute, using the region
    ! name ("mainloop") of our loop.

    loop_attribute = cali_find_attribute('loop')
    iter_attribute = cali_make_loop_iteration_attribute('mainloop')

    call cali_begin_string(loop_attribute, 'mainloop')

    do i = 1, count
        call cali_begin_int(iter_attribute, i)

        ! ...

        call cali_end(iter_attribute)
    end do

    call cali_end(loop_attribute)

    ! End 'main'
    call cali_end_region('main')

    ! Compute and flush output for the ConfigManager profiles.
    call mgr%flush
    call ConfigManager_delete(mgr)
end program fortran_example
