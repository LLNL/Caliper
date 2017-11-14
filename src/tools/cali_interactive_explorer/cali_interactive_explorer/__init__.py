import pandas, argparse, sh, StringIO, glob, json, sys, math
import plotly.plotly as py
import plotly.graph_objs as go
import matplotlib.pyplot as plt
import inspect
import copy
import enum

plt.ion()
parser = argparse.ArgumentParser()
parser.add_argument("--inputs", nargs="*")
parser.add_argument("--initial-query-file")


class DocumentationType(enum.Enum):
    Off = 0
    CopyPaste = 1
    Google = 2


documentation_type = 0

documentation_map = {
    "Off": DocumentationType.Off,
    "CopyPaste": DocumentationType.CopyPaste,
    "Google": DocumentationType.Google
}


def map_column(column):
    if column == 'count()':
        return 'aggregate.count'
    else:
        return column


def unmap_column(column):
    if column == 'aggregate.count':
        return 'count()'
    else:
        return column


def set_documentation(level):
    global documentation_type
    if level not in documentation_map:
        print("Error: selected documentation type not valid, valid levels are" + str(
            documentation_map.keys()) + ", using 'Google'")
    else:
        documentation_type = documentation_map[level]


set_documentation("Off")

cali_query = sh.cali_query

files_to_analyze = []

args = parser.parse_known_args()

initial_query_file = None
try:
    initial_query_file = args.initial_query_file
except:
    pass

inputs = []
try:
    inputs = args.inputs
except:
    pass
initial_query_result = None


def local_name():
    stack = inspect.stack(0)
    for frame_num in range(len(stack)):
        caller_locals = stack[frame_num][0].f_locals
        name = next((k for k, v in caller_locals.items()
                     if v is sys.modules[__name__]), None)
        if name is not None:
            return name
    return None


def global_name():
    return __name__


def get_module_name():
    if documentation_type == DocumentationType.Google:
        return global_name()
    else:
        return local_name()


def to_pandas(poorly_formatted_string):
    formatted = poorly_formatted_string
    return pandas.read_json(formatted)


attribute_data = {
    "aggregate.count": {"type": "uint"}
}

calql_keywords = ["where ", "group by", "select"]


def process_attribute_dataframe(df):
    new_attribute_data = {
        "aggregate.count": {"type": "uint"}
    }
    for index, row in df.iterrows():
        if row['cali.attribute.name'] not in new_attribute_data:
            new_attribute_data[row['cali.attribute.name']] = {}
        new_attribute_data[row[0]]["type"] = row['cali.attribute.type']
        if 'class.aggregatable' in row and row['class.aggregatable'] == 'true':
            new_attribute_data[row[0]]["aggregatable"] = True
        else:
            new_attribute_data[row[0]]["aggregatable"] = False
    return new_attribute_data


def keyword_boundaries(full_string, keyword, other_keywords):
    keywords = [word for word in other_keywords]
    try:
        del (keywords[keyword])
    except:
        pass
    kw_loc = full_string.find(keyword) + len(keyword) + 1
    if kw_loc == len(keyword):
        return (None, None)
    locations = sorted([(kw, full_string.find(kw)) for kw in keywords], key=lambda (x, y): y)
    nearest_location = next((loc for kw, loc in locations if loc > kw_loc), len(full_string))
    return kw_loc, nearest_location


def get_between_keywords(full_string, keyword, other_keywords, separator=","):
    kw_loc, nearest_location = keyword_boundaries(full_string, keyword, other_keywords)
    if kw_loc == (None, None):
        return ""
    split = [x.strip() for x in full_string[kw_loc:nearest_location].split(separator)]
    return split


def get_columns(raw_query, frame, keyword, other_keywords=calql_keywords):
    query_to_parse = raw_query.lower()
    columns = set([])
    split = get_between_keywords(query_to_parse, keyword, other_keywords)
    for column in split:
        if column == "*":
            if frame is not None:
                for df_column in frame.columns:
                    columns.add(df_column)
            else:
                print("Wildcard requests not supported here, this is an implementation bug for now")
        else:
            columns.add(map_column(column))
    return [item for item in list(columns) if len(item)>0]


##TODO: remove goofy holdover default attribute_dict value
def get_column_types(cl_raw, search_type, attribute_dict=attribute_data):
    cl = [column for column in cl_raw if len(column)>0]
    integral_types = ["uint", "float", "int"]
    stringy_types = ["string"]
    choice_map = {"numeric": integral_types, "string": stringy_types}
    if search_type not in choice_map:
        print "Invalid search type"
        sys.exit(1)
    search_list = choice_map[search_type]
    return [column for column in cl if attribute_dict[column]["type"] in search_list]


def graph_lines(frame, independent_set, dependent_set):
    series = get_column_types(dependent_set, "string")
    xAxes = get_column_types(dependent_set, "numeric")

    for independent in independent_set:
        for value in series:
            split_by = frame.groupby(value)
            for xAxis in xAxes:
                for label, df in split_by:
                    df = df.sort(xAxis)
                    finalXAxis = df[xAxis]
                    finalIndependent = df[independent]
                    plt.plot(finalXAxis, finalIndependent, label=label)
                    plt.xlabel(xAxis)
                    plt.ylabel(independent)
                    plt.legend()


#Phase: Fluid
class CaliperFrame(object):
    def __init__(self, query_text=None, new_inputs=None, **kwargs):
        self._ready = False
        self.valid_metadata = False
        self._query = query_text
        self._inputs = None
        self._skip_attributes = "skip_attributes" in kwargs and kwargs["skip_attributes"] == True
        self._attributes = None
        if type(new_inputs) == list:
            self.inputs = new_inputs
        elif new_inputs is not None:
            self.inputs = list(glob.glob(new_inputs))
        self._results = None
        self._documentation = None

    def debug_print(self):
        print "query: " + self._query
        print "inputs: " + str(self._inputs)

    def run_query(self, **kwargs):
        additional_args = []
        if 'additional_args' in kwargs:
            additional_args = kwargs['additional_args']
        full_query = ['--query=' + self.query + ' format ' + 'json()']
        full_query.extend([input for input in self.inputs])
        full_query.extend(additional_args)
        cmd_output = StringIO.StringIO()
        cali_query(full_query, _out=cmd_output)
        full_out = cmd_output.getvalue()
        panda_frame = to_pandas(full_out)
        if "condense" in kwargs:
            panda_frame = condense_dataframe(panda_frame)
        self.documentation = "query_result = " + get_module_name() + ".CaliperFrame(" + self.query + "," + str(
            self.inputs) + ")"
        if documentation_type != DocumentationType.Off:
            print self.documentation
        return panda_frame

    @property
    def ready(self):
        return self._ready

    @ready.setter
    def ready(self, value):
        self._ready = value

    @property
    def documentation(self):
        return self._documentation

    @documentation.setter
    def documentation(self, value):
        self._documentation = value

    @property
    def attributes(self):
        return self._attributes

    @attributes.setter
    def attributes(self, value):
        self._attributes = value

    @property
    def inputs(self):
        return self._inputs

    @inputs.setter
    def inputs(self, value):
        if type(value) == list:
            self._inputs = value
        else:
            self._inputs = list(glob.glob(value))
        self.ready = False
        if not self._skip_attributes:
            attribute_califrame = CaliperFrame("SELECT cali.attribute.name,cali.attribute.type,class.aggregatable",
                                               self._inputs, skip_attributes=True)
            attribute_dataframe = attribute_califrame.run_query(additional_args=["--list-attributes"])
            self.attributes = process_attribute_dataframe(attribute_dataframe)

    @property
    def results(self):
        if self.query is None:
            raise Exception('Attempted to get results on CaliperFrame with invalid query')  # TODO: this better
        if not self._ready:
            self._results = self.run_query()
            self.ready = True
        assert isinstance(self._results, pandas.DataFrame)
        return self._results

    @results.setter
    def results(self, value):
        self._results = value

    @property
    def query(self):
        return self._query

    @query.setter
    def query(self, query_text):
        if query_text is not None:
            self._query = query_text
            self.results = self.run_query()


#Phase: Prototype
class Analysis(object):
    def __init__(self,cali_frame,analysis_frame):
        self.cali_frame = cali_frame
        self.analysis_frame = analysis_frame

    @property
    def cali_frame(self):
        # type: () -> CaliperFrame
        return self._cali_frame
    
    @cali_frame.setter
    def cali_frame(self, value):
        self._cali_frame=value
    
    @property
    def analysis_frame(self):
        # type: () -> pandas.DataFrame
        return self._analysis_frame
    
    @analysis_frame.setter
    def analysis_frame(self, value):
        self._analysis_frame=value


class Graphable:
    def __init(self, frame, series, independent, dependent):
        self._query = frame.query


# TODO: program -> function in group_keys, this is for demo
def condense_dataframe(cali_frame, group_keys=["program", "run_size"], required_items=["time.inclusive.duration"]):
    # type: (CaliperFrame, list, list) -> pandas.DataFrame
    """

    :type cali_frame: cali_interactive_explorer.CaliperFrame
    """
    dataframe = cali_frame.results
    attributes = cali_frame.attributes
    grouped = dataframe.groupby(group_keys)
    final_frame = []
    for label, df in grouped:
        current_row = {}
        for key_name, key_val in zip(group_keys, label):
            current_row[key_name] = key_val
        final_row_dict = {}
        for index, row in df.iterrows():
            for key in required_items:
                val = row[key]
                if not math.isnan(val):
                    if key not in current_row:
                        current_row[key] = val
                    else:
                        if attributes[key]["aggregatable"]:
                            current_row[key] += val
                        elif current_row[key] != val:
                            print "Warning, multiple valid values for non-aggregatable key " + label
        for required_value in required_items:
            if required_value not in current_row:
                print "Warning, did not find value for " + required_value + " for key " + label
        final_frame.append(current_row)
    return pandas.DataFrame(final_frame)


def query(query_string='', additional_args=[], **kwargs):
    full_query = ['--query=' + query_string + ' format ' + 'json()']
    full_query.extend([input for input in inputs])
    full_query.extend(additional_args)
    cmd_output = StringIO.StringIO()
    a = cali_query(full_query, _out=cmd_output)
    full_out = cmd_output.getvalue()
    panda_frame = to_pandas(full_out)
    if "condense" in kwargs:
        panda_frame = condense_dataframe(panda_frame)
    return panda_frame


def load_data(new_inputs, raw=False):
    global inputs
    if raw:
        inputs = new_inputs
    else:
        inputs = glob.glob(new_inputs)
    attribute_frame = query("SELECT cali.attribute.name,cali.attribute.type,class.aggregatable", ["--list-attributes"])
    for index, row in attribute_frame.iterrows():
        if row['cali.attribute.name'] not in attribute_data:
            attribute_data[row['cali.attribute.name']] = {}
        attribute_data[row[0]]["type"] = row['cali.attribute.type']
        if 'class.aggregatable' in row and row['class.aggregatable'] == 'true':
            attribute_data[row[0]]["aggregatable"] = True
        else:
            attribute_data[row[0]]["aggregatable"] = False


if inputs:
    load_data(inputs, True)


def get_series_data(analysis, all_set, dependent_set):
    """

    :type all_set: list
    :type dependent_set: list
    :type frame: pandas.DataFrame
    """
    frame = analysis.analysis_frame
    attribute_data = analysis.cali_frame.attributes
    independent_set = list(set(all_set) - set(dependent_set))
    series = get_column_types(dependent_set, "string",attribute_data)
    xAxes = get_column_types(dependent_set, "numeric",attribute_data)
    traces = []
    #TODO: this is broken for multidimensional. Make it less bad
    for independent in independent_set:
        current_trace = {"data" : [],
                         "yAxis" : independent}
        for value in series:
            split_by = frame.groupby(value)
            for xAxis in xAxes:
                current_trace["xAxis"] = xAxis
                for label, df in split_by:
                    df = df.sort_values(xAxis)
                    finalXAxis = df[xAxis]
                    finalIndependent = df[independent]
                    current_trace["data"].append(go.Scatter(
                        x=finalXAxis,
                        y=finalIndependent,
                        mode="lines+markers",
                        name=label
                    ))
        traces.append(current_trace)
    return traces


def to_graph(query_frame):
    """

    :type query_frame: CaliperFrame
    """
    query_string = query_frame.query

    select_columns = get_columns(query_string, None, "select")
    group_columns = get_columns(query_string, None, "group by")
    selected = list(set(select_columns) - set(group_columns))
    numeric_selects = get_column_types(select_columns, "numeric",query_frame.attributes)
    numeric_groups = get_column_types(group_columns, "numeric",query_frame.attributes)
    num_numeric_grouped = len(numeric_selects) - len(numeric_groups)
    num_character_grouped = get_column_types(group_columns, "string",query_frame.attributes) # TODO: handle multiple character groups
    condensed = condense_dataframe(query_frame, group_columns, selected)
    analysis = Analysis(query_frame,condensed)
    scatters = []
    if num_numeric_grouped == 1:
        scatters = get_series_data(analysis, select_columns, group_columns)
    layout = go.Layout(showlegend=True,
                       xaxis = {
                           "title" : scatters[0]["xAxis"]
                       },
                       yaxis={
                           "title": scatters[0]["yAxis"]
                       },

                       )
    return go.Figure(data=scatters[0]["data"], layout=layout)


# OLD: JUST SAVING THIS
# def graph(query_string=''):
#    def map_column(column):
#        if column == 'count()':
#            return 'aggregate.count'
#        else:
#            return column
#
#    def unmap_column(column):
#        if column == 'aggregate.count':
#            return 'count()'
#        else:
#            return column
#
#    start, end = keyword_boundaries(query_string.lower(), "select", calql_keywords)
#    new_select = ""
#    select_columns = get_columns(query_string, None, "select")
#    where_columns = get_columns(query_string, None, "where")
#    group_columns = get_columns(query_string, None, "group by")
#    new_select = " " + ",".join(
#        set([unmap_column(x) for x in select_columns]) | set([unmap_column(x) for x in group_columns])) + " "
#    new_query = query_string[:start] + new_select + query_string[end:]
#    num_numeric_grouped = len(get_column_types(group_columns, "numeric"))
#    num_character_grouped = get_column_types(group_columns, "string")
#    frame = query(new_query)
#    if num_numeric_grouped == 1:
#        graph_lines(frame, select_columns, group_columns)
#    return frame


if initial_query_file is not None:
    with open(initial_query_file, "r") as input_file:
        joined_query = " ".join(input_file.readlines())
        initial_query_result = query(joined_query)
