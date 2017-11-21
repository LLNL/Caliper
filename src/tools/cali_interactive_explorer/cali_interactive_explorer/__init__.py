import pandas, argparse, sh, StringIO, glob, sys, math
import plotly.graph_objs as go
import matplotlib.pyplot as plt
import inspect
import enum
import networkx as nx

plt.ion()
parser = argparse.ArgumentParser()
parser.add_argument("--inputs", nargs="*")
parser.add_argument("--initial-query-file")


class DocumentationType(enum.Enum):
    Off = 0
    CopyPaste = 1
    Google = 2
    Default = 3


documentation_type = 0

documentation_map = {
    "Off": DocumentationType.Off,
    "CopyPaste": DocumentationType.CopyPaste,
    "Google": DocumentationType.Google,
    "Default": DocumentationType.Default
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


def resolve_documentation_level(doc_level):
    if doc_level == DocumentationType.Default:
        return documentation_type
    else:
        return doc_level


def get_module_name(raw_doc_level):
    doc_level = resolve_documentation_level(raw_doc_level)
    if doc_level == DocumentationType.Google:
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
    return [item for item in list(columns) if len(item) > 0]


##TODO: remove goofy holdover default attribute_dict value
def get_column_types(cl_raw, search_type, attribute_dict=attribute_data):
    cl = [column for column in cl_raw if len(column) > 0]
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


# Phase: Fluid
class CaliperFrame(object):
    def __init__(self, query_text=None, new_inputs=None, doc_level=DocumentationType.Default, **kwargs):
        self._ready = False
        self.valid_metadata = False
        self.documentation_level = doc_level
        self._query = query_text
        self._inputs = None
        self.input_specification = lambda x: str(x)
        self._skip_attributes = "skip_attributes" in kwargs and kwargs["skip_attributes"]
        self._attributes = None
        if type(new_inputs) == list:
            self.inputs = new_inputs
        elif new_inputs is not None:
            self.inputs = list(glob.glob(new_inputs))
        self._results = None
        self._documentation = None

    def recalculate_query(self):
        self.calculated_query = self.query

    def resolve_documentation(self):
        self.documentation = "query_result = " + get_module_name(
            self.documentation_level) + ".CaliperFrame(\"" + self.query + "\"," + self.input_specification(
            self.inputs) + ")"

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
        self.resolve_documentation()
        if resolve_documentation_level(self.documentation_level) != DocumentationType.Off:
            print self.documentation
        return panda_frame

    @property
    def calculated_query(self):
        return self._calculated_query

    @calculated_query.setter
    def calculated_query(self, value):
        self._calculated_query = value

    @property
    def time_metric(self):
        return self._time_metric

    @time_metric.setter
    def time_metric(self, value):
        self._time_metric = value

    @property
    def input_specification(self):
        return self._input_specification

    @input_specification.setter
    def input_specification(self, value):
        self._input_specification = value

    @property
    def documentation_level(self):
        return self._documentation_level

    @documentation_level.setter
    def documentation_level(self, value):
        self._documentation_level = value

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

    # Fluid logic:
    # "If I have aggregates, let Caliper do the aggregation and request aggregated values.
    #  else do it myself"
    def guess_time_metric(self, get_exclusive=False):
        search_key = "time.duration" if get_exclusive else "time.inclusive.duration"
        time_metrics = [attr for attr in self.attributes if search_key in attr]
        if not any(time_metrics):
            return None
        aggregates = [metric for metric in time_metrics if "aggregate.sum" in metric]
        if not any(aggregates):
            return "sum(" + time_metrics.pop() + ")"
        return aggregates.pop()

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
                                               self._inputs, DocumentationType.Off, skip_attributes=True)
            attribute_dataframe = attribute_califrame.run_query(additional_args=["--list-attributes"])
            self.attributes = process_attribute_dataframe(attribute_dataframe)
            self.time_metric = self.guess_time_metric()
            self.recalculate_query()

    @property
    def results(self):
        if self.query is None:
            raise Exception('Attempted to get results on CaliperFrame with invalid query')  # TODO: this better
        if not self.ready:
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
        self._query = query_text
        if query_text is not None:
            self.recalculate_query()


# Phase: Prototype
class Analysis(object):
    def __init__(self, cali_frame, analysis_frame):
        self.cali_frame = cali_frame
        self.analysis_frame = analysis_frame

    @property
    def cali_frame(self):
        # type: () -> CaliperFrame
        return self._cali_frame

    @cali_frame.setter
    def cali_frame(self, value):
        self._cali_frame = value

    @property
    def analysis_frame(self):
        # type: () -> pandas.DataFrame
        return self._analysis_frame

    @analysis_frame.setter
    def analysis_frame(self, value):
        self._analysis_frame = value

    @property
    def documentation(self):
        return self._documentation

    @documentation.setter
    def documentation(self, value):
        self._documentation = value


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
    :type analysis: Analysis
    """
    frame = analysis.analysis_frame
    attributes = analysis.cali_frame.attributes
    independent_set = list(set(all_set) - set(dependent_set))
    series = get_column_types(dependent_set, "string", attributes)
    xAxes = get_column_types(dependent_set, "numeric", attributes)
    traces = []
    # TODO: this is broken for multidimensional. Make it less bad
    for independent in independent_set:
        current_trace = {"data": [],
                         "yAxis": independent}
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
    numeric_selects = get_column_types(select_columns, "numeric", query_frame.attributes)
    numeric_groups = get_column_types(group_columns, "numeric", query_frame.attributes)
    num_numeric_grouped = len(numeric_selects) - len(numeric_groups)
    num_character_grouped = get_column_types(group_columns, "string",
                                             query_frame.attributes)  # TODO: handle multiple character groups
    condensed = condense_dataframe(query_frame, group_columns, selected)
    analysis = Analysis(query_frame, condensed)
    scatters = []
    if num_numeric_grouped == 1:
        scatters = get_series_data(analysis, select_columns, group_columns)
    layout = go.Layout(showlegend=True,
                       xaxis={
                           "title": scatters[0]["xAxis"]
                       },
                       yaxis={
                           "title": scatters[0]["yAxis"]
                       },

                       )
    return go.Figure(data=scatters[0]["data"], layout=layout)


sankey_themes = {
    "light": {},
    "dark": {
        "plot_bgcolor": "black",
        "paper_bgcolor": 'black'

    }

}


def get_sankey(caliper_frame, tree_attribute="function", metrics=["time.inclusive.duration"], theme="light",
               globalism=False):
    """

    :type caliper_frame: CaliperFrame
    """

    def contextualize_label(label):
        return label

    metric_totals = {}

    def get_edge_color_value(name, metric, value):
        fraction = (1.0 * value) / (1.0 * metric_totals[metric])
        remaining_part = 1.0 - fraction
        red_value = str(int(256.0 * fraction))
        green_value = str(int(256.0 * remaining_part))
        blue_value = "0"
        alpha_value = "0.5"
        return "rgba(" + red_value + "," + green_value + "," + blue_value + "," + alpha_value + ")"

    def get_node_color_value(name, metric, value):
        fraction = (1.0 * value) / (1.0 * metric_totals[metric])
        remaining_part = 1.0 - fraction
        red_value = str(int(256.0 * fraction))
        green_value = str(int(256.0 * remaining_part))
        blue_value = "0"
        alpha_value = "0.5"
        return "rgba(" + red_value + "," + green_value + "," + blue_value + "," + alpha_value + ")"

    sankey = {
        "data": [
            {
                "type": "sankey",
                "domain":
                    {
                        "x": [0, 1],
                        "y": [0, 1]
                    },
                "orientation": "h",
                "valueformat": ".0f",
                "valuesuffix": "TWh",
                "node": {
                    "pad": 15,
                    "thickness": 15,
                    "line": {
                        "color": "black",
                        "width": 0.5
                    },
                    "label": [],
                    "color": []
                },
                "link": {
                    "source": [],
                    "target": [],
                    "color": [],
                    "value": [],
                    "label": []
                }
            }
        ],
        "layout": {
            "title": "How cool are dogs?",
            "width": 1118,
            "height": 772,
            "font": {
                "size": 10
            }
        }
    }
    metric_per_tree = {}
    node_to_idx = {}
    edge_to_idx = {}
    panda_frame = pandas.DataFrame
    if type(caliper_frame)==CaliperFrame:
        panda_frame = caliper_frame.results.copy()
    else:
        panda_frame = caliper_frame.copy()

    for metric in metrics:
        metric_per_tree[metric] = dict((key, val) for (key, val) in sankey.iteritems())

    common_prefix = panda_frame[tree_attribute][0].split("/")[0]

    offset = 0
    edge_num = 0

    def get_total_for_metric(frame, metric, share_prefix):
        if not share_prefix:
            return frame[metric].sum()
        else:
            return frame[metric].max()

    share_prefix = not any(name for name in panda_frame[tree_attribute] if name.split("/")[0] != common_prefix)
    for metric in metrics:
        metric_totals[metric] = get_total_for_metric(panda_frame, metric, share_prefix)
    if not share_prefix:
        panda_frame[tree_attribute] = panda_frame[tree_attribute].apply(lambda x: "problem/" + x)
        node_to_idx["problem"] = 0
        for metric in metrics:
            offset = 1
            data_dictionary = metric_per_tree[metric]["data"][0]
            data_dictionary["node"]["label"].append("problem")
            data_dictionary["node"]["color"].append(
                get_node_color_value("problem", metric, get_total_for_metric(panda_frame, metric, share_prefix)))
    max_index = 0
    # Child relations
    for raw_index, (raw_label, subframe) in enumerate(
            sorted(panda_frame.groupby(tree_attribute), key=lambda (label, frame): label.count("/"))):
        max_index = raw_index
        index = raw_index + offset
        label = contextualize_label(raw_label)
        node_to_idx[label] = index
        for metric in metrics:
            metric_for_frame = get_total_for_metric(subframe, metric, share_prefix)
            data_dictionary = metric_per_tree[metric]["data"][0]
            end_label = label.split("/")[-1]
            data_dictionary["node"]["label"].append(end_label)
            data_dictionary["node"]["color"].append(get_node_color_value(label, metric, metric_for_frame))
            #print "Establishing value of " + str(metric_for_frame) + " for node " + str(label)
            if "/" in label:
                path = label.split("/")
                uptick = "/".join(path[:-1])
                edge_dictionary = data_dictionary["link"]
                edge_dictionary["source"].append(node_to_idx[uptick])
                edge_dictionary["target"].append(node_to_idx[label])
                edge_to_idx[label] = edge_num
                edge_num += 1
                edge_dictionary["color"].append(get_edge_color_value(label, metric, metric_for_frame))
                edge_dictionary["value"].append(metric_for_frame)
                edge_dictionary["label"].append(label)
                substrings = [path[:y] for y in [x + 1 for x in range(1, len(path) - 2)]]
    # Establishing "others"
    # for raw_index, (raw_label, subframe) in enumerate(
    #        sorted(panda_frame.groupby(tree_attribute), key=lambda (label, frame): label.count("/"))):
    for skip_this_loop in []:
        index = raw_index + max_index + 1

        label = contextualize_label(raw_label)
        depth = label.count("/")
        label_for_exclusive = label + "/other"
        node_to_idx[label_for_exclusive] = index
        for metric in metrics:
            metric_for_frame = get_total_for_metric(subframe, metric, share_prefix)
            child_frames = [(child_label, child_frame) for (child_label, child_frame) in
                            panda_frame.groupby(tree_attribute) if
                            (child_label.count("/") == depth + 1) and (label in child_label)]
            child_contributions = 0
            for child_label, child_frame in child_frames:
                get_total_for_metric(child_frame, metric, share_prefix)
                child_contributions += get_total_for_metric(child_frame, metric, share_prefix)
            other_contributions = metric_for_frame - child_contributions
            data_dictionary = metric_per_tree[metric]["data"][0]
            end_label = label.split("/")[-1]
            data_dictionary["node"]["label"].append("other")
            data_dictionary["node"]["color"].append(
                get_node_color_value(label, metric, other_contributions))
            edge_dictionary = data_dictionary["link"]
            edge_dictionary["source"].append(node_to_idx[label])
            edge_dictionary["target"].append(node_to_idx[label_for_exclusive])
            edge_to_idx[label_for_exclusive] = edge_num
            edge_num += 1
            edge_dictionary["color"].append(get_edge_color_value(label, metric, other_contributions))
            edge_dictionary["value"].append(other_contributions)
            edge_dictionary["label"].append(label)

            # print "Establishing value of " + str(other_contributions) + " for node " + str(label_for_exclusive)

            # if globalism:
            #   for ancestor in substrings:
            #      ancestor_name = "/".join(ancestor)
            #     uptick = "/".join(ancestor[:-1])
        #
        #                       edge_dictionary["value"][edge_to_idx[ancestor_name]] -= metric_for_frame
        #                      edge_dictionary["source"].append(node_to_idx[ancestor_name])
        #                     edge_dictionary["target"].append(node_to_idx[uptick])
        #                    edge_dictionary["color"].append(get_edge_color_value(label, metric, metric_for_frame))
        #                   edge_dictionary["value"].append(metric_for_frame)
        #                  edge_dictionary["label"].append(label)
        #                 edge_num+=1

    sankeys = {}
    for metric in metric_per_tree:
        data = metric_per_tree[metric]
        sankeys[metric] = {}
        sankeys[metric]["data"] = [dict(
            type='sankey',
            domain=dict(
                x=[0, 1],
                y=[0, 1]
            ),
            orientation="h",
            valueformat=".0f",
            valuesuffix="",
            node=dict(
                pad=15,
                thickness=15,
                line=dict(
                    color="black",
                    width=0.5
                ),
                label=data['data'][0]['node']['label'],
                color=data['data'][0]['node']['color']
            ),
            link=dict(
                source=data['data'][0]['link']['source'],
                target=data['data'][0]['link']['target'],
                value=data['data'][0]['link']['value'],
                label=data['data'][0]['link']['label']
            )
        )]
        sankeys[metric]["layout"] = dict(
            title=metric,
            font=dict(
                size=10
            ),

        )
        for key in sankey_themes[theme]:
            sankeys[metric]["layout"][key] = sankey_themes[theme][key]
    return sankeys


def sankey_plot(files, metrics=["aggregate.sum#time.inclusive.duration"], against="function"):
    sankey_califrame = CaliperFrame("SELECT " + ",".join(metrics) + "," + against, files)
    sankeys = get_sankey(sankey_califrame, against, metrics)
    if len(sankeys) == 1:
        return sankeys[metrics[0]]
    return sankeys


if initial_query_file is not None:
    with open(initial_query_file, "r") as input_file:
        joined_query = " ".join(input_file.readlines())
        initial_query_result = query(joined_query)
