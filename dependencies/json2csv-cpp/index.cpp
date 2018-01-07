#include <iostream>
#include <fstream>
#include <algorithm>
#include <set>
#include <sstream>
#include <memory>
#include <regex>
#include <json/json.h>

using namespace std;

typedef shared_ptr<std::vector<shared_ptr<std::vector<std::pair<string, string>>>>> objects_t;
typedef shared_ptr<std::vector<std::pair<string, string>>> object_t;
typedef vector<unique_ptr<string>> line_t;
typedef vector<shared_ptr<vector<unique_ptr<string>>>> lines_t;

bool jsonToCsv(shared_ptr<Json::Value> jsonInput, const char* input, const char* output){
  
  bool parsingSuccessful = true;
  Json::Reader reader;
  ifstream ifs(input);

  if (ifs.is_open()){
    istream& s = ifs;
    parsingSuccessful = reader.parse(s, *jsonInput);
    if (!parsingSuccessful){
      cout << "Error parsing file => " << input << "\n";
    }
  }
  else {
    cout << "File not found => " << input << "\n";
    parsingSuccessful = false;
  }
  ifs.close();
  return parsingSuccessful;
}

string joinVector(vector<string>& v, const char* delimiter){
  std::stringstream ss;
  for (size_t i = 0; i < v.size(); ++i)
  {
    if (i != 0)
      ss << delimiter;
    ss << v[i];
  }
  return ss.str();
}

void toKeyValuePairs(shared_ptr<std::vector<std::pair<string, string>>>& builder, Json::Value& source, vector<string>& ancestors, const char* delimiter){

  if (source.isObject()){
    for (Json::Value::iterator it = source.begin(); it != source.end(); it++){
      Json::Value key = it.key();
      Json::Value val = (*it);
      vector<string> objKeys;
      objKeys.insert(objKeys.end(), ancestors.begin(), ancestors.end());
      objKeys.push_back(key.asString());
      toKeyValuePairs(builder, val, objKeys, "/");
    }
  }
  else if (source.isArray()){
    int count = 0;
    std::for_each(source.begin(), source.end(), [&builder, &count, &ancestors](Json::Value& val){
      stringstream ss;
      ss << count;
      vector<string> arrKeys;
      arrKeys.insert(arrKeys.end(), ancestors.begin(), ancestors.end());
      arrKeys.push_back(ss.str());
      toKeyValuePairs(builder, val, arrKeys, "/");
      count++;
    });
  }
  else {
    string key = joinVector(ancestors, delimiter);
    auto tpl = std::make_pair(key, source.asString());
    builder->push_back(tpl);
  }

}

objects_t jsonToDicts(shared_ptr<Json::Value> jsonInput){

  //convert json into array if not already
  if (!jsonInput->isArray()){
    Json::Value jv;
    jv.append(std::move(*jsonInput));
    *jsonInput = jv;
  }

  auto objects = make_shared<std::vector<shared_ptr<std::vector<std::pair<string, string>>>>>();

  std::for_each(jsonInput->begin(), jsonInput->end(), [&objects](Json::Value& d){
    auto builder = make_shared<std::vector<std::pair<string, string>>>();
    objects->push_back(builder);
    auto o = objects->back();
    vector<string> keys;
    toKeyValuePairs(o, d, keys, "/");
  });

  return objects;
}

shared_ptr<lines_t> dictsToCsv(objects_t o) {

  std::set<string> k;
  auto lines = make_shared<lines_t>();
  std::regex newline("\\r|\\n");
  std::regex quote("\"");

  auto buildKeys = [&k](object_t& e){
    for (auto& g : *e){
      k.insert(g.first);
    }
  };

  auto buildHeaders = [&k, &lines](){
    auto l = make_shared<line_t>();
    for (auto& h : k){
      l.get()->push_back(make_unique<string>(h));
    }
    lines.get()->push_back(l);
  };

  auto buildRows = [&k, &lines, &newline, &quote](object_t& e){
    auto l = make_shared<line_t>();
    for (auto& h : k){
      auto it = std::find_if(e.get()->begin(), e.get()->end(), [&h](const std::pair<string, string>& p)->bool{
        return p.first == h;
      });

      if (it != e.get()->end()){
        std::string n = std::regex_replace((it)->second, newline, "\\\\n");
        n = std::regex_replace(n, quote, "\"\"");
        l.get()->push_back(make_unique<string>("\"" + n + "\""));
        e.get()->erase(it);
      }
      else {
        l.get()->push_back(make_unique<string>());
      }
    }
    lines.get()->push_back(l);

  };
  
  std::for_each(o->begin(), o->end(), buildKeys);
  buildHeaders();
  std::for_each(o->begin(), o->end(), buildRows);

  return lines;
}

int main(int argc, char *argv[]) { 

  if (argc < 2){
    cout << "At least 2 arguments is required!\n";
    exit(1);
  }

  auto jsonInput = make_shared<Json::Value>();

  //check if input file exists
  auto ok = jsonToCsv(jsonInput, argv[1], argv[2]);
  if (!ok){
    exit(1);
  }

  //flatten the objects
  auto objects = jsonToDicts(jsonInput);

  //merge and sort the keys
  auto csv = dictsToCsv(objects);

  //export file
  ofstream ofs;
  ofs.open(argv[2]);
  for (const auto&e : *csv){
    auto len = e.get()->size();
    int counter = 0;
    for (const auto&g : *e){
      ofs << *g << (counter < len - 1 ? "," : "");
      counter++;
    }
    ofs << "\n";
  }
  ofs.close();

  return 0;
}