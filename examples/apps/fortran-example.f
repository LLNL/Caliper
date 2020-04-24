program fortran_example
    use caliper_mod
  
    implicit none
  
    type(ScopeAnnotation) :: f_ann, i_ann
    type(ConfigManager)   :: mgr

    integer               :: i, count

    logical               :: ret

    call cali_init()

    mgr = ConfigManager_new()
    call mgr%add('runtime-report')
    call mgr%start

    f_ann = ScopeAnnotation_begin('main')
  
    ! Mark "initialization" phase
    i_ann = ScopeAnnotation_begin('initialization')
    count = 4
    call ScopeAnnotation_end(i_ann)

    call ScopeAnnotation_end(f_ann)

    call mgr%flush
end program fortran_example
