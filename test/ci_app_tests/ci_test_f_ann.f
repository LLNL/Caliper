program fortran_example
    use caliper_mod

    implicit none

    type(Annotation)      :: g_ann
    type(ScopeAnnotation) :: f_ann, w_ann
    type(ConfigManager)   :: mgr

    integer               :: i, count, argc, prop

    logical               :: ret
    character(:), allocatable     :: spec
    character(len=:), allocatable :: errmsg
    character(len=256)    :: arg

    spec = '{ "name"       : "custom-trace-spec", &
           &  "categories" : [ "output" ], &
           &  "services"   : [ "event", "trace", "recorder" ] &
           &}' 

    call cali_init()

    mgr = ConfigManager_new()

    call mgr%add_config_spec(spec)

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

    call mgr%start

    f_ann = ScopeAnnotation_begin('main')
    w_ann = ScopeAnnotation_begin('work')

    call cali_set_global_int_byname('global.int.1', 42)

    prop  = IOR(CALI_ATTR_GLOBAL, CALI_ATTR_ASVALUE)
    g_ann = Annotation_new_with_properties('global.int.2', prop)
    call g_ann%set_int(84)
    call Annotation_delete(g_ann)

    call ScopeAnnotation_end(w_ann)

    call cali_begin_region('foo')
    call cali_end_region('foo')

    call ScopeAnnotation_end(f_ann)

    call mgr%flush
    call ConfigManager_delete(mgr)
end program fortran_example
