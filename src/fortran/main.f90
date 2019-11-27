program testf03
  use Caliper

  implicit none

  integer                    :: cali_ret
  integer(kind(CALI_INV_ID)) :: iter_attr
  integer                    :: i, count

  ! Mark "initialization" phase
  call cali_begin_byname('initialization')
  count = 4
  call cali_end_byname('initialization')

  if (count .gt. 0) then
     ! Mark "loop" phase
     call cali_begin_byname('loop')

     ! create attribute for iteration counter with CALI_ATTR_ASVALUE property
     call cali_create_attribute('iteration', CALI_TYPE_INT, &
          CALI_ATTR_ASVALUE, iter_attr)

     do i = 1,count
        ! Update iteration counter attribute
        call cali_set_int(iter_attr, i)

        ! A Caliper snapshot taken at this point will contain
        ! { "loop", "iteration"=<i> } 
        
        ! perform calculation
     end do

     ! Clear the iteration counter attribute (otherwise, snapshots taken
     ! after the loop will still contain the last iteration value)
     call cali_end(iter_attr, cali_ret)
     
     ! End "loop" phase
     call cali_end_byname('loop')
  end if
end program testf03
