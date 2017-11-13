import pandas,argparse,sh,StringIO,glob,json,sys
import matplotlib.pyplot as plt
plt.ion()
parser = argparse.ArgumentParser()
parser.add_argument("--inputs", nargs="*")
parser.add_argument("--initial-query-file")

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

def to_pandas(poorly_formatted_string):
  formatted = poorly_formatted_string
  return pandas.read_json(formatted)

attribute_data = { 
            "aggregate.count" : { "type" : "uint"}
        }

#TODO: program -> function in group_keys, this is for demo
def condense_dataframe(dataframe, group_keys = ["program","run_size"], required_items = ["time.inclusive.duration"]):
  grouped = dataframe.groupby(group_keys)  
  processed_frame = dataframe
  for label,df in grouped:
    pass
    #print label
    #print df
    #final_frame = 
    #for index,row in df.iterrows():
  return processed_frame
   
    

def query(query_string='', additional_args = [], **kwargs):
  full_query = ['--query=' +query_string +' format '+ 'json()']
  full_query.extend([input for input in inputs])
  full_query.extend(additional_args)
  cmd_output = StringIO.StringIO()
  a = cali_query(full_query,_out=cmd_output)
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
  attribute_frame = query("SELECT cali.attribute.name,cali.attribute.type,class.aggregatable",["--list-attributes"])
  for index,row in attribute_frame.iterrows():
      if row['cali.attribute.name'] not in attribute_data:
          attribute_data[row['cali.attribute.name']] = {}
      attribute_data[row[0]]["type"] = row['cali.attribute.type']
      if 'class.aggregatable' in row and row['class.aggregatable'] == 'true':
        attribute_data[row[0]]["aggregatable"] = True
      else:
        attribute_data[row[0]]["aggregatable"] = False

if inputs:
  load_data(inputs,True)

def get_column_types(cl, search_type):
    integral_types = ["uint", "float","int"]
    stringy_types = ["string"]
    choice_map = {"numeric" : integral_types , "string" : stringy_types}
    if search_type not in choice_map:
        print "Invalid search type"
        sys.exit(1)
    search_list = choice_map[search_type]    
    return [column for column in cl if attribute_data[column]["type"] in search_list]

def graph_lines(frame, independent_set, dependent_set):
    series = get_column_types(dependent_set,"string")
    xAxes = get_column_types(dependent_set,"numeric")

    values = {}
    for independent in independent_set:
      for value in series:
          split_by = frame.groupby(value) 
          for xAxis in xAxes:
            for label,df in split_by:
              df = df.sort(xAxis)
              finalXAxis = df[xAxis]
              finalIndependent = df[independent]
              plt.plot(finalXAxis,finalIndependent,label = label)
              plt.xlabel(xAxis)
              plt.ylabel(independent)
              plt.legend()

def graph(query_string=''):
  calql_keywords = ["where ","group by","select"]
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
  def keyword_boundaries(full_string, keyword, other_keywords):
      keywords = [word for word in other_keywords]
      try:
          del(keywords[keyword])
      except:
          pass
      kw_loc = full_string.find(keyword) + len(keyword) + 1
      if kw_loc == -1:
          return []
      locations = sorted([(kw,full_string.find(kw)) for kw in keywords],key = lambda (x,y): y)
      nearest_location = next((loc for kw,loc in locations if loc > kw_loc), len(full_string))
      return kw_loc,nearest_location
  def get_between_keywords(full_string, keyword, other_keywords, separator=","):
      kw_loc,nearest_location = keyword_boundaries(full_string,keyword,other_keywords)
      split = [x.strip() for x in full_string[kw_loc:nearest_location].split(separator)]
      return split
  def get_columns(raw_query,frame,keyword):
      query_to_parse = raw_query.lower()
      columns = set([])
      split = get_between_keywords(query_to_parse,keyword,calql_keywords)
      for column in split:
          if column == "*":
              if frame is not None:
                for df_column in frame.columns:
                    columns.add(df_column)
              else:
                  print("Wildcard requests not supported here, this is an implementation bug for now")
          else:
              columns.add(map_column(column))
      return list(columns)
  start,end = keyword_boundaries(query_string.lower(), "select" ,calql_keywords)
  new_select = ""
  select_columns = get_columns(query_string,None,"select")
  where_columns = get_columns(query_string,None,"where")
  group_columns = get_columns(query_string,None,"group by")
  new_select =" "+ ",".join(set([unmap_column(x) for x in select_columns]) | set([unmap_column(x) for x in group_columns]))+ " "
  new_query = query_string[:start] + new_select + query_string[end:]
  num_numeric_grouped = len(get_column_types(group_columns,"numeric"))
  num_character_grouped = get_column_types(group_columns,"string")
  frame = query(new_query)
  if num_numeric_grouped == 1:
      graph_lines(frame,select_columns,group_columns)
  return frame
if initial_query_file is not None:
    with open(initial_query_file,"r") as input_file:
        joined_query = " ".join(input_file.readlines())
        initial_query_result = query(joined_query) 

