import pandas,argparse,sh,StringIO,glob,json
import matplotlib.pyplot as plt

plt.ioff()
parser = argparse.ArgumentParser()
parser.add_argument("--inputs", nargs="*")
parser.add_argument("--initial-query-file")

cali_query = sh.cali_query


args = parser.parse_args()

initial_query_file = None
try:
  initial_query_file = args.initial_query_file  
except:
  pass

inputs = args.inputs  
initial_query_result = None

def to_pandas(poorly_formatted_string):
  raw_json = json.loads(poorly_formatted_string) 
  polished_json = []
  for row in raw_json["rows"]:
    row_dict = {}
    for index,item in enumerate(row):
       row_dict[raw_json["attributes"][index]] = item
    polished_json.append(row_dict)
  return pandas.read_json(json.dumps(polished_json))

def query(query_string='', additional_args = []):
  #full_query = ["-q ",  query_string ,'format', 'json()']  
  full_query = ["-q", query_string +' format '+ 'json()']
  full_query.extend([input for input in inputs])
  full_query.extend(additional_args)
  cmd_output = StringIO.StringIO()
  a=cali_query(full_query,_out=cmd_output)
  full_out = cmd_output.getvalue()
  full_out=full_out.strip().replace(",\x08","").replace("\n","").replace("\\","\\\\")
  return to_pandas(full_out)

attribute_frame = query("SELECT cali.attribute.name,cali.attribute.type",["--list-attributes"])
attribute_data = { 
            "aggregate.count" : { "type" : "uint"}
        }
for index,row in attribute_frame.iterrows():
    if row['cali.attribute.name'] not in attribute_data:
        attribute_data[row['cali.attribute.name']] = {}
    attribute_data[row[0]]["type"] = row['cali.attribute.type']


def graph(query_string=''):
  def map_column(column):
      if column == 'count()':
          return 'aggregate.count'
      else:
          return column
  def get_between_keywords(full_string, keyword, other_keywords, separator=","):
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
      split = [x.strip() for x in full_string[kw_loc:nearest_location].split(separator)]
      return split
  def get_columns(raw_query,frame,keyword):
      query_to_parse = raw_query.lower()
      keywords = ["where ","group by","select"]
      columns = set([])
      split = get_between_keywords(query_to_parse,keyword,keywords)
      for column in split:
          if column == "*":
              for df_column in frame.columns:
                  columns.add(df_column)
          else:
              columns.add(map_column(column))
      return list(columns)
  frame = query(query_string)
  select_columns = get_columns(query_string,frame,"select") 
  where_columns = get_columns(query_string,frame,"where") 
  group_columns = get_columns(query_string,frame,"group by") 
  #num_numeric_grouped = [column for column in group_columns if attribute_data[column]["type"] in ["uint", "float"]]
  num_numeric_grouped = [(column,attribute_data[column]["type"]) for column in group_columns if True]
  print num_numeric_grouped
  return frame
if initial_query_file is not None:
    with open(initial_query_file,"r") as input_file:
        joined_query = " ".join(input_file.readlines())
        initial_query_result = query(joined_query) 

frame = graph("SELECT count(),program,run_size,time.inclusive.duration GROUP BY program,run_size WHERE program,run_size")
print frame
