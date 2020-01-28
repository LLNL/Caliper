module Caliper

  use, intrinsic :: iso_c_binding

  integer(kind=C_INT64_T), parameter :: CALI_INV_ID = -1
  
  ! cali_attr_type
  integer(kind=C_INT), parameter ::   &
       CALI_TYPE_INV           =  0,  &
       CALI_TYPE_USR           =  1,  &
       CALI_TYPE_INT           =  2,  &
       CALI_TYPE_UINT          =  3,  &
       CALI_TYPE_STRING        =  4,  &
       CALI_TYPE_ADDR          =  5,  &
       CALI_TYPE_DOUBLE        =  6,  &
       CALI_TYPE_BOOL          =  7,  &
       CALI_TYPE_TYPE          =  8

  ! cali_attr_properties
  integer(kind=C_INT), parameter ::   &
       CALI_ATTR_DEFAULT       =  0,  &
       CALI_ATTR_ASVALUE       =  1,  &
       CALI_ATTR_NOMERGE       =  2,  &
       CALI_ATTR_SCOPE_PROCESS = 12,  &
       CALI_ATTR_SCOPE_THREAD  = 20,  &
       CALI_ATTR_SCOPE_TASK    = 24,  &
       CALI_ATTR_SKIP_EVENTS   = 64,  &
       CALI_ATTR_HIDDEN        = 128, &
       CALI_ATTR_NESTED        = 256, &
       CALI_ATTR_GLOBAL        = 512
  
  ! cali_err
  integer(kind=C_INT), parameter ::  &
       CALI_SUCCESS            = 0,  &
       CALI_EBUSY              = 1,  &
       CALI_ELOCKED            = 2,  &
       CALI_EINV               = 3,  &
       CALI_ETYPE              = 4
  
contains

  ! cali_create_attribute
  subroutine cali_create_attribute(name, type, properties, id)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR, C_INT64_T
    implicit none

    character(len=*),        intent(in)  :: name
    integer,                 intent(in)  :: type
    integer,                 intent(in)  :: properties

    integer(kind=C_INT64_T), intent(out) :: id

    ! cali_id_t cali_create_attribute(const char* name, cali_attr_type type, int properties)
    interface
       integer(kind=C_INT64_T) function cali_create_attribute_c (name, type, properties) &
            bind(C, name='cali_create_attribute')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT, C_INT64_T
         character(kind=C_CHAR), intent(in)        :: name
         integer(kind=C_INT),    intent(in), value :: type
         integer(kind=C_INT),    intent(in), value :: properties
       end function cali_create_attribute_c
    end interface

    id = cali_create_attribute_c( trim(name)//C_NULL_CHAR, type, properties )
  end subroutine cali_create_attribute

  
  ! cali_begin
  subroutine cali_begin(id, err)
    use, intrinsic :: iso_c_binding, only : C_INT64_T
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_begin(cali_id_t id, int val);
    interface
       integer(kind=C_INT) function cali_begin_c (id) &
            bind(C, name='cali_begin')
         use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
       end function cali_begin_c
    end interface

    err_ = cali_begin_c( id )

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_begin

  ! cali_begin_string
  subroutine cali_begin_string(id, val, err)
    use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT64_T, C_NULL_CHAR
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    character(len=*),            intent(in) :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_begin_string(cali_id_t id, int val);
    interface
       integer(kind=C_INT) function cali_begin_string_c (id, val) &
            bind(C, name='cali_begin_string')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
         character(kind=C_CHAR),  intent(in)        :: val
       end function cali_begin_string_c
    end interface

    err_ = cali_begin_string_c( id, trim(val)//C_NULL_CHAR )

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_begin_string
  
  ! cali_begin_int
  subroutine cali_begin_int(id, val, err)
    use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    integer,                     intent(in) :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_begin_int(cali_id_t id, int val);
    interface
       integer(kind=C_INT) function cali_begin_int_c (id, val) &
            bind(C, name='cali_begin_int')
         use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
         integer(kind=C_INT),     intent(in), value :: val
       end function cali_begin_int_c
    end interface

    err_ = cali_begin_int_c(id, val)

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_begin_int

  ! cali_begin_double
  subroutine cali_begin_double(id, val, err)
    use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    real*8,                      intent(in) :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_begin_int(cali_id_t id, int val);
    interface
       integer(kind=C_INT) function cali_begin_double_c (id, val) &
            bind(C, name='cali_begin_double')
         use, intrinsic :: iso_c_binding, only : C_INT, C_DOUBLE, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
         real(kind=C_DOUBLE),     intent(in), value :: val
       end function cali_begin_double_c
    end interface

    err_ = cali_begin_double_c( id, val )

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_begin_double

  ! cali_set_string
  subroutine cali_set_string(id, val, err)
    use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT64_T, C_NULL_CHAR
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    character(len=*),            intent(in) :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_set_string(cali_id_t id, int val);
    interface
       integer(kind=C_INT) function cali_set_string_c (id, val) &
            bind(C, name='cali_set_string')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
         character(kind=C_CHAR),  intent(in)        :: val
       end function cali_set_string_c
    end interface

    err_ = cali_set_string_c( id, trim(val)//C_NULL_CHAR )

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_set_string
  
  ! cali_set_int
  subroutine cali_set_int(id, val, err)
    use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    integer,                     intent(in) :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_set_int(cali_id_t id, int val);
    interface
       integer(kind=C_INT) function cali_set_int_c (id, val) &
            bind(C, name='cali_set_int')
         use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
         integer(kind=C_INT),     intent(in), value :: val
       end function cali_set_int_c
    end interface

    err_ = cali_set_int_c(id, val)

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_set_int

  ! cali_set_double
  subroutine cali_set_double(id, val, err)
    use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    real*8,                      intent(in) :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_set_int(cali_id_t id, int val);
    interface
       integer(kind=C_INT) function cali_set_double_c (id, val) &
            bind(C, name='cali_set_double')
         use, intrinsic :: iso_c_binding, only : C_INT, C_DOUBLE, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
         real(kind=C_DOUBLE),     intent(in), value :: val
       end function cali_set_double_c
    end interface

    err_ = cali_set_double_c( id, val )

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_set_double


  ! cali_end
  subroutine cali_end(id, err)
    use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
    implicit none

    integer(kind=C_INT64_T),     intent(in) :: id
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))             :: err_

    ! cali_err cali_end(cali_id_t id);
    interface
       integer(kind=C_INT) function cali_end_c (id) &
            bind(C, name='cali_end')
         use, intrinsic :: iso_c_binding, only : C_INT, C_INT64_T
         integer(kind=C_INT64_T), intent(in), value :: id
       end function cali_end_c
    end interface

    err_ = cali_end_c(id)

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_end

  
  !
  ! --- _byname "overloads"
  !
  
  ! cali_begin_byname
  subroutine cali_begin_byname(attr_name, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),  intent(in)  :: attr_name
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))    :: err_

    ! cali_err cali_begin_byname(const char* attr_name, const char* val);
    interface
       integer(kind=C_INT) function cali_begin_byname_c (attr_name) &
            bind(C, name='cali_begin_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT
         character(kind=C_CHAR), intent(in) :: attr_name(*)
       end function cali_begin_byname_c
    end interface

    err_ = cali_begin_byname_c( trim(attr_name)//C_NULL_CHAR )
    
    if (present(err)) then
       err = err_
       return
    end if
  end subroutine cali_begin_byname
  
  ! cali_begin_string_byname
  subroutine cali_begin_string_byname(attr_name, val, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),  intent(in)  :: attr_name
    character(len=*),  intent(in)  :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))    :: err_

    ! cali_err cali_begin_string_byname(const char* attr_name, const char* val);
    interface
       integer(kind=C_INT) function cali_begin_string_byname_c (attr_name, val) &
            bind(C, name='cali_begin_string_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT
         character(kind=C_CHAR), intent(in) :: attr_name(*)
         character(kind=C_CHAR), intent(in) :: val
       end function cali_begin_string_byname_c
    end interface

    err_ = cali_begin_string_byname_c( trim(attr_name)//C_NULL_CHAR, &
         trim(val)//C_NULL_CHAR )
    
    if (present(err)) then
       err = err_
       return
    end if
  end subroutine cali_begin_string_byname

  ! cali_begin_double_byname
  subroutine cali_begin_double_byname(attr_name, val, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),  intent(in)  :: attr_name
    real*8,            intent(in)  :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))    :: err_

    ! cali_err cali_begin_double_byname(const char* attr_name, double val);
    interface
       integer(kind=C_INT) function cali_begin_double_byname_c (attr_name, val) &
            bind(C, name='cali_begin_double_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_DOUBLE, C_INT
         character(kind=C_CHAR), intent(in)        :: attr_name(*)
         real(kind=C_DOUBLE),    intent(in), value :: val
       end function cali_begin_double_byname_c
    end interface

    err_ = cali_begin_double_byname_c( trim(attr_name)//C_NULL_CHAR, val )
    
    if (present(err)) then
       err = err_
       return
    end if
  end subroutine cali_begin_double_byname

  ! cali_begin_int_byname
  subroutine cali_begin_int_byname(attr_name, val, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),  intent(in)  :: attr_name
    integer,           intent(in)  :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))    :: err_

    ! cali_err cali_begin_int_byname(const char* attr_name, int val);
    interface
       integer(kind=C_INT) function cali_begin_int_byname_c (attr_name, val) &
            bind(C, name='cali_begin_int_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT
         character(kind=C_CHAR), intent(in)        :: attr_name(*)
         integer(kind=C_INT),    intent(in), value :: val
       end function cali_begin_int_byname_c
    end interface

    err_ = cali_begin_int_byname_c( trim(attr_name)//C_NULL_CHAR, val )
    
    if (present(err)) then
       err = err_
       return
    end if
  end subroutine cali_begin_int_byname

    ! cali_set_string_byname
  subroutine cali_set_string_byname(attr_name, val, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),  intent(in)  :: attr_name
    character(len=*),  intent(in)  :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))    :: err_

    ! cali_err cali_set_string_byname(const char* attr_name, const char* val);
    interface
       integer(kind=C_INT) function cali_set_string_byname_c (attr_name, val) &
            bind(C, name='cali_set_string_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT
         character(kind=C_CHAR), intent(in) :: attr_name(*)
         character(kind=C_CHAR), intent(in) :: val
       end function cali_set_string_byname_c
    end interface

    err_ = cali_set_string_byname_c( trim(attr_name)//C_NULL_CHAR, &
         trim(val)//C_NULL_CHAR )
    
    if (present(err)) then
       err = err_
       return
    end if
  end subroutine cali_set_string_byname

  ! cali_set_double_byname
  subroutine cali_set_double_byname(attr_name, val, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),  intent(in)  :: attr_name
    real*8,            intent(in)  :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))    :: err_

    ! cali_err cali_set_double_byname(const char* attr_name, double val);
    interface
       integer(kind=C_INT) function cali_set_double_byname_c (attr_name, val) &
            bind(C, name='cali_set_double_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_DOUBLE, C_INT
         character(kind=C_CHAR), intent(in)        :: attr_name(*)
         real(kind=C_DOUBLE),    intent(in), value :: val
       end function cali_set_double_byname_c
    end interface

    err_ = cali_set_double_byname_c( trim(attr_name)//C_NULL_CHAR, val )
    
    if (present(err)) then
       err = err_
       return
    end if
  end subroutine cali_set_double_byname

  ! cali_set_int_byname
  subroutine cali_set_int_byname(attr_name, val, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),  intent(in)  :: attr_name
    integer,           intent(in)  :: val
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))    :: err_

    ! cali_err cali_set_int_byname(const char* attr_name, int val);
    interface
       integer(kind=C_INT) function cali_set_int_byname_c (attr_name, val) &
            bind(C, name='cali_set_int_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT
         character(kind=C_CHAR), intent(in)        :: attr_name(*)
         integer(kind=C_INT),    intent(in), value :: val
       end function cali_set_int_byname_c
    end interface

    err_ = cali_set_int_byname_c( trim(attr_name)//C_NULL_CHAR, val )
    
    if (present(err)) then
       err = err_
       return
    end if
  end subroutine cali_set_int_byname

  
  ! cali_end_byname
  subroutine cali_end_byname(attr_name, err)
    use, intrinsic :: iso_c_binding, only : C_NULL_CHAR
    implicit none

    character(len=*),            intent(in)   :: attr_name
    integer(kind(CALI_SUCCESS)), intent(out), optional :: err

    integer(kind(CALI_SUCCESS))               :: err_

    ! kind(CALI_SUCCESS) cali_end_byname(const char* attr_name)
    interface
       integer(kind=C_INT) function cali_end_byname_c (attr_name) &
            bind(C, name='cali_end_byname')
         use, intrinsic :: iso_c_binding, only : C_CHAR, C_INT
         character(kind=C_CHAR), intent(in) :: attr_name(*)
       end function cali_end_byname_c
    end interface

    err_ = cali_end_byname_c( trim(attr_name)//C_NULL_CHAR )

    if (present(err)) then
       err = err_
    end if
  end subroutine cali_end_byname

  ! cali_mpi_init
  subroutine cali_mpi_init()
   use, intrinsic :: iso_c_binding, only : C_NULL_CHAR, C_INT64_T
   implicit none

   ! void cali_mpi_init()
   interface
      subroutine cali_mpi_init_c () &
           bind(C, name='cali_mpi_init')
        use, intrinsic :: iso_c_binding
      end subroutine cali_mpi_init_c
   end interface

   call cali_mpi_init_c()
 end subroutine cali_mpi_init

end module Caliper
