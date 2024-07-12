from pycaliper.instrumentation import (
    begin_region,
    end_region,
    begin_phase,
    end_phase,
    begin_comm_region,
    end_comm_region,
)

import functools


__annotation_decorator_begin_map = {
    "region": begin_region,
    "phase": begin_phase,
    "comm_region": begin_comm_region,
}


__annotation_decorator_end_map = {
    "region": end_region,
    "phase": end_phase,
    "comm_region": end_comm_region,
}


def annotate_function(name=None, annotation_type="region"):
    def inner_decorator(func):
        real_name = name
        if name is None or name == "":
            real_name = func.__name__
        if annotation_type not in list(__annotation_decorator_begin_map.keys()):
            raise ValueError("Invalid annotation type {}".format(annotation_type))

        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            __annotation_decorator_begin_map[annotation_type](real_name)
            result = func(*args, **kwargs)
            __annotation_decorator_end_map[annotation_type](real_name)
            return result

        return wrapper

    return inner_decorator
