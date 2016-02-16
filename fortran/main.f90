program testf03
  use Caliper

  implicit none

  integer                    :: cali_ret
  integer(kind(CALI_INV_ID)) :: iter_attr
  integer                    :: i
  
  call cali_begin_string_byname('testf03.phase', 'main')

  call cali_create_attribute('testf03.iteration', CALI_TYPE_INT, &
       CALI_ATTR_ASVALUE, iter_attr)

  do i = 1,4
     call cali_set_int(iter_attr, i)
  end do

  call cali_end(iter_attr, cali_ret)

  if (cali_ret .ne. CALI_SUCCESS) then
     print *, "cali_end returned with", cali_ret
  end if
  
  call cali_end_byname('testf03.phase')
end program testf03
