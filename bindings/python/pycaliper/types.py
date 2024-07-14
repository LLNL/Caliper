from pycaliper.__pycaliper_impl.types import *

from enum import Enum


_CALI_TYPE_ENUM_PREFIX = "CALI_TYPE_"
_CALI_PROPERTIES_ENUM_PREFIX = "CALI_ATTR_"


AttrType = Enum(
    "AttrType",
    {
        name[len(_CALI_TYPE_ENUM_PREFIX) :]: val
        for name, val in AttrTypeEnum.__members__.items()
        if name.startswith(_CALI_TYPE_ENUM_PREFIX)
    },
)


AttrProperties = Enum(
    "AttrProperties",
    {
        name[len(_CALI_PROPERTIES_ENUM_PREFIX) :]: val
        for name, val in AttrPropertiesEnum.__members__.items()
        if name.startswith(_CALI_PROPERTIES_ENUM_PREFIX)
    },
)
