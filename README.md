# ODK Tools
ODK Tools is a toolbox for processing [ODK](https://opendatakit.org/) survey data into MySQL databases. The toolbox can either be combined with [FormShare](https://github.com/qlands/FormShare) as it conveniently stores submissions in JSON format but also can process ODK raw submissions in XML format. The toolbox can also be combined with [META](https://github.com/ilri/meta) to support multiple languages. 

ODK Tools comprises of three command line tools performing different tasks and six utility programs. The toolbox is cross-platform and can be built in Windows, Linux, and Mac.

## The toolbox

### ODK to MySQL
ODK to MySQL converts an ODK Excel File (XLSX survey file) into a relational MySQL schema. Even though [ODK Aggregate](https://opendatakit.org/use/aggregate/) stores submissions in MySQL, the Aggregate schema lacks basic functionality like:
 - Avoid duplicated submissions if a unique ID is used in a survey.
 - Store and link select values.
 - Store multi-select values as independent rows.
 - Relational links between repeats and sub-repeats.
 - In some cases, data could end up with too many columns.
 - No dictionary.
 - No multi-language support.

 ODK to MySQL creates a complete relational schema with the following features:
 - A variable can be identified as a unique ID and becomes the primary key.
 - Select and multi-select values are stored in lookup tables. Lookup tables are then linked to main tables.
 - Multi-select values are stored in sub-tables where each value is recorded as an independent row.
 - Repeats are stored in independent tables. Sub-repeats are related to parent-repeats.
 - Repeats and variable names are extracted and accessible.
 - Multiple languages support.
 - Selects create lookup tables. Since two or more selects can have the same items, the tool minimizes the number of lookup tables created.
 - Yes/No selects are ignored
 - Tables with more than 100 fields (indicating the data covers various themes or topics) are required to be separated into sub-tables by theme or topic. 

 Output files produced:
 - create.sql: A [DDL](http://en.wikipedia.org/wiki/Data_definition_language) script containing all data structures, indexes, and relationships.
 - insert.sql: A [DML](http://en.wikipedia.org/wiki/Data_manipulation_language) script that inserts all the select and multi-select values in the lookup tables.
 - metadata.sql: A DML script that inserts the name of all tables and variables in META's dictionary tables.
 - iso639.sql: A DML script that inserts the name of all tables and variables and lookup values into META's language tables.
 - separationID.xml: This file is created when one or more tables need separation by theme or topic. This separation file can be edited to create groups by theme or topic then use it as an input file to perform the separation.
 - manifest.xml: This file maps each variable in the ODK survey with its corresponding field in a table in the MySQL database. **This file is used in subsequent tools.**
 - create.xml: This is an XML representation of the schema. Used by utilities compareCreateXML & createFromXML.
 - insert.xml: This is an XML representation of the lookup tables and values. Used by utilities compareInsertXML & insertFromXML.

#### *Parameters*
  - x - Input ODK XLSX file.
  - v - Main survey variable. This is the variable that is unique to each survey submission. For example National ID, Passport or Farmer Id, etc. **This IS NOT the ODK survey ID found in settings.** This variable will become the primary key in the main table. You can only select **one** variable.
  - t - Main table. Name of the master table for the target schema. ODK surveys do not have a master table however this is necessary to store ODK variables that are not inside a repeat. You can choose any name for example "maintable" **Important note: If the main survey variable is store inside a repeat then the main table must be such repeat.**
  - d - Default storing language. For example (en)English. **This is the default language for the database and might be different as the default language in the ODK survey.** If not indicated then English will be assumed.
  - l - Other languages. For example (fr)French, (es)Español. Required if the ODK file has multiple languages.
  - y - Yes and No strings in the default language in the format "String|String". This will allow the tool to identify Yes/No lookup tables and exclude them. **It is not case sensitive.** For example, if the default language is Spanish then this value should be indicated as "Si|No". If its empty then English "Yes|No" will be assumed.
  - p - Table prefix. A prefix that can be added to each table. This is useful if a common schema is used to store different surveys.
  - c - Output DDL file. "create.sql" by default.
  - i - Output DML file. "insert.sql" by default
  - m - Output metadata file. "metadata.sql" by default.
  - T - Output translation file. "iso639.sql" by default.
  - f - Output manifest file. "manifest.xml" by default
  - S - Output separation file. This file might be generated by this tool when tables have too many columns. If no output is indicated an autogenerated file is created.
  - s - Input separation file. This file is the one indicated with -S or autogenerated by the tool when tables have too many columns. 
  - I - Output lookup tables and values in XML format. "insert.xml" by default.
  - C - Output schema as in XML format. "create.xml" by default
  - o - Output type: (h)uman readable or (m)achine readable. Machine by default.
  - e - Temporary directory. If no directory is specified then ./tmp will be created.
  - *support files* separated with space. You can indicate multiple support files like CSVs or ZIPs. The tool will use CSVs to collect options from external sources.


#### *Example for a single language ODK*

    $ ./odktomysql -x my_input_xlsx_file.xlsx -v QID -t maintable

#### *Example for a multi-language ODK (English and Español) where English is the default storing language*

    $ ./odktomysql -x my_input_xlsx_file.xlsx -v main_questionarie_ID -t maintable -l "(es)Español"

#### *Example for a multi-language ODK (English and Español) where Spanish is the default storing language*
 

    $ ./odktomysql -x my_input_xlsx_file.xlsx -v main_questionarie_ID -t maintable -d "(es)Español" -l "(en)English" -y "Si|No"

  See examples for single and multiple languages [here](https://github.com/ilri/odktools/tree/master/ODKToMySQL/examples)

---
### FormShare to JSON
FormShare stores ODK submissions in a [Mongo](https://www.mongodb.org/) database. Although FormShare provides exporting functions to CSV and MS Excel it does not provide exporting to more interoperable formats like [JSON](http://en.wikipedia.org/wiki/JSON). FormShare to JSON is a small Python program that extracts survey data from MongoDB to JSON files. Each data submission is exported as a JSON file using FormShare submission UUID as the file name.
#### *Parameters*
  - m - URI for the Mongo Server. For example mongodb://localhost:2701
  - d - FormShare database. "formshare" by default.
  - c - FormShare collection storing the surveys. "instances" by default.
  - y - ODK Survey ID. This is found in the "settings" sheet of the ODK XLSX file.
  - o - Output directory. "./output" by default (created if it does not exist).
  - w - Overwrite JSON file if exists. False by default.

#### *Example*
 
    $ python formsharetojson.py -m "mongodb://my_mongoDB_server:27017/" -y my_survey_id -o ./my_output_directory


---
### XML to JSON
XML to JSON converts ODK XML data submissions and converts them into JSON format. The output is the same as **FormShare to JSON**.
#### *Parameters*
  - i- Input XML file
  - o- Output JSON file
  - m- Manifest XML file **(created by ODK To MySQL)**

#### *Example*

    $ xmltojson -i ./my_input_xml_file.xml -o ./my_output_json_file.json -m ./my_manifest_file.xml


### JSON to MySQL
JSON to MySQL imports JSON files (generated by **FormShare To JSON** or  **XML to JSON**) into a MySQL schema generated by **ODKToMySQL**. The tool imports one file at a time and requires a manifest file (see **ODK to MySQL**). The tool generates an error log file in XML or CSV format and a Map file.

**What is a Map file**

ODK submissions either in XML format or converted into JSON format contains data in a tree-like structure where different branches refer to different types of data. When this structure is imported into a MySQL Database by **JSON to MySQL** each branch of data goes into a different table. This process is called [normalization](https://en.wikipedia.org/wiki/Database_normalization). In some cases, particularly for data analysis, it is necessary to reconstruct the tree structure from the tables ([Denormalize the data](https://en.wikipedia.org/wiki/Denormalization)). This can only be done efficiently by preserving the tree structure but where each branch of data points to a row in a table. **JSON to MySQL** assigns a Universally Unique Identifier ([UUID](https://en.wikipedia.org/wiki/Universally_unique_identifier)) to each branch inserted into a table. The Map file has the same tree structure of the ODK submissions but only containing UUIDs. 


#### *Parameters*
  - H - MySQL host server. Default is localhost
  - P - MySQL port. Default 3306
  - s - Schema to be converted
  - u - User who has select access to the schema
  - p - Password of the user
  - m - Input manifest file.
  - M - Output directory to store the Map file
  - j - Input JSON file.
  - o - Output log file. "output.csv" by default.
  - O - Output type: (h)umand or (machine) readable. Machine by default.
  - J - Input JavaScript file with a "beforeInsert" function to customize the values entering the database. **See notes below**.

#### *Example*

    $ ./jsontomysql -H my_MySQL_server -u my_user -p my_pass -s my_schema -m ./my_manifest_file.xml -j ./my_JSON_file.json -o my_log_file.csv -M /path/to/map/file/directory

#### *Customizing the insert with an external JavaScript*
 The insert process of **JSON to MySQL** can be hooked up to an external JavaScript file to perform changes in the data before inserting them into the MySQL database. This is done by creating a .js file with the following code:

 

    function beforeInsert(table,data)
    {
       // My before insert code goes here
    }

The “beforeInsert” function is called for each row in each table entering the database.

*Parameters:*
  - table: **string**. Contains the table being processed.
  - data: **object**. Contains information about the columns to be inserted into the database and the values for one row of data.

The "data" object has the following structure:

*Properties:*
  - **int** count: contains the number if items (columns) in the row.

*Functions:*
  - **string** itemName(**int** index): Returns the column name for a given item index.
  - **string** itemXMLCode(**int**  index):  Returns the ODK column name for a given item index.
  - **string** itemValue(**int**  index):  Returns the column value for a given item index.
  - **bool** itemIsKey(**int**  index): Returns whether the column is key for a given item index.
  - **bool** itemToInsert(**int**  index): Returns whether the column is going to be inserted for a given item index.
  - **bool** ItemIsMultiSelect(**int**  index): Returns whether the column is a multiselect for a given item index.
  - **string** itemMultiSelectTable(**int**  index): Returns the multiselect table for a given item index.

*Procedures*
  - insertValue(**int** name, **string** xmlCode, **string** value, **bool** isKey, **bool**  isMultiSelect, **string** multiSelectTable): Inserts a new column into the object.
  - setItemName(**int** index, **string** name): Change the column name for a given item index.
  - setItemXMLCode(**int** index, **string** xmlCode): Change the ODK column name for a given item index.
  - setItemValue(**int** index, **string** value): Change the column value for a given item index.
  - setItemIsKey(**int** index, **bool** isKey): Sets a column to be key or not for a given item index.
  - setItemIsMultiSelect(**int** index, **bool** isMultiSelect): Sets a column to be multiselect or not for a given item index.
  - setItemMultiSelectTable(**int** index, **string** table): Change the multiselect table name of a column for a given item index.
  - setItemToInsert(**int** index, **bool** toInsert): Sets a column to be inserted or not for a given item index.

*Utility functions:*
  - **int** getIndexByColumnName(**string** name): Return the index of a column using its name. Notes: This is **NOT** case sensitive. Returns -1 if the column is not found.
  - **bool** valueIsNumber(**int** index): Returns whether the value of a column is a number or not for a given index.

---

### Compare Create XML (Utility)
Compare Create XML compares two create XML files generated by **ODK to MySQL** (**A** and **B**) for incremental changes. **A** is consider an incremental version of **B**. This tool is useful when dealing with multiple versions of an ODK survey that must be combined in one common database. The tool informs of tables and variables in **A** that ARE NOT in **B**. It can also create a combined file **C** with all of **B** plus all of **A** with the following condition:
  1. If a table in **A** is not found in **B** then it will be added to **C** only if its parent exists in **B**.

The tool WILL NOT fix the following:
  1. Inconsistencies in field definition between **A** and **B** like changes in the primary key,  downsize, type, parent table, and parent field.
  2. Tables that do not share the same parent.

If the merging on **A** into **B** is successful the tool will create **"C"** and a SQL file containing the instructions to upgrade the database created by **B** so it can store the incremental changes of **A**.

#### *Parameters*
  - a - Input **A** file.
  - b - Input **B** file.
  - c - Output combined file. "combined-create.xml" by default.
  - d - SQL file to store the changes required by the database.
  - o - Output type: It can be (h)uman readable or (m)achine readable. By default is machine readable and will produce an XML output

#### *Nomenclature:*
  - TNF: Table not found and will be added to **C**
  - TNS: The table does not have the same parent table. (The tool will not fix it in **C** thus the user will need to fix the ODK XLSX file before continue)
  - FNF: Field not found and will be added to **C**
  - FNS: The field is not the same. (The tool will not fix it in **C** thus the user will **need to fix the ODK XLSX file before continue**)

#### *Example*

    $ ./comparecreatexml -a ./my_A_file.xml -b ./my_B_file.xml -c ./my_output_C_file.xml -o h


---
### Compare Insert XML (Utility)
Compare Insert XML compares two insert XML files created by **ODK to MySQL** (**A** and **B**) for incremental changes. **A** is consider an incremental version of **B**. This tool is useful when dealing with multiple versions of an ODK survey that must be combined in one common database. The tool informs of lookup tables and their values in **A** that ARE NOT in **B**. 

If the merging on **A** into **B** is successful the tool will create **"C"** which has all the values of **B** plus all of **A** and a SQL file containing the insert instructions to upgrade the database created by **B** so it has the lookup values needed by **A**.

If the tool finds an inconsistency in a value between A and B  it will ask the user for a resolution.

#### *Parameters*
  - a - Input **A** file.
  - b - Input **B** file.
  - c - Output combined file. "combined-insert.xml" by default.
  - d - SQL file to store the changes required by the database.
  - o - Output type: It can be (h)uman readable or (m)achine readable. By default is machine readable and will produce an XML output

#### *Nomenclature:*
  - TNF: Table not found and will be added to **C**.  
  - VNF: Value not found and will be added to **C**.
  - VNS: The value is not the same. (The tool will not fix it in **C** thus the user will **need to fix the ODK XLSX file before continue**).


#### *Example*

    $ ./compareinsertxml -a ./my_A_file.xml -b ./my_B_file.xml -c ./my_output_C_file.xml -o h


---
### Create From XML (Utility)
Create From XML creates a SQL DDL script file from a XML schema file generated either by **ODK To MySQL** or **Compare Create XML**.

#### *Parameters*
  - i - Input create XML file.  
  - o - Output SQL DDL file. "create.sql" by default.

#### *Example*
 

    $ ./createfromxml -i ./my_create_file.xml -o ./my_output_sql_file.sql

---
### Insert From XML (Utility)
Insert From XML creates a SQL DML script file from a XML insert file generated either by **ODK To MySQL** or **Compare Insert XML**.

#### *Parameters*
  - i - Input insert XML file.  
  - o - Output SQL DML file. "insert.sql" by default.

#### *Example*

    $ ./insertfromxml -i ./my_insert_file.xml -o ./my_output_sql_file.sql

---
### MySQL Denormalize (Utility)
MySQL Denormalize converts data from a MySQL Database into the tree representation of the original submission but with the data coming from the database thus containing any changes made to the data after the original submission. It relies on the Map XML files created by **JSON to MySQL**. 

#### *Parameters*
  - H - MySQL host server. Default is localhost
  - P - MySQL port. Default 3306
  - s - Schema to be converted
  - u - User who has select access to the schema
  - p - Password of the user
  - t - Main table
  - T - Path to a temporary directory
  - m - Directory containing the Map files generated by  **JSON to MySQL**
  - o - Output directory to store the JSON files for each submission.

#### *Example*

    $ ./mysqldenormalize -H my_MySQL_server -u my_user -p my_pass -s my_schema -t main_table -m /path/to/the/map/files -o /path/to/output/directory 

---
### JSON to CSV (Utility)
JSON to CSV creates a CSV file based a JSON file by flattening the JSON structure as described [here](https://sunlightfoundation.com/2014/03/11/making-json-as-simple-as-a-spreadsheet/). 

#### *Parameters*
  - i - Input directory containing the JSON files
  - o - Output CSV file. ./output.csv by default.
  - t - Temporary directory. ./tmp by default. 

#### *Example*

    $ ./jsontocsv -i /path/to/directory/with/json/files -o ./my_output.csv 


## Building and testing
To build ODKTools on Ubuntu Server 16.04.3 LTS do:

    $ sudo apt-get update
    $ sudo apt-get install build-essential qt5-default qtbase5-private-dev qtdeclarative5-dev cmake mongodb jq libboost-all-dev unzip zlib1g-dev automake

You also need to build and install:

 - [Mongo C Driver](https://github.com/mongodb/mongo-c-driver/releases/download/1.6.1/mongo-c-driver-1.6.1.tar.gz)
 - [JSONCpp](https://github.com/open-source-parsers/jsoncpp/archive/1.8.4.tar.gz)
 - [QuaZip](http://quazip.sourceforge.net/) using -DCMAKE_C_FLAGS:STRING="-fPIC" -DCMAKE_CXX_FLAGS:STRING="-fPIC"
 - [LibCSV](https://github.com/rgamble/libcsv) . It uses automake 1.14 so create a symbolic link of aclocal and automake from your version to 1.14

To build ODK Tools do:

        $ git clone https://github.com/ilri/odktools.git
        $ cd odktools
        $ git submodule update --init --recursive
        $ cd dependencies/mongo-cxx-driver-r3.1.1/build
        $ cmake -DCMAKE_C_FLAGS:STRING="-O2 -fPIC" -DCMAKE_CXX_FLAGS:STRING="-O2 -fPIC" -DBSONCXX_POLY_USE_BOOST=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
        $ sudo make install
        $ cd ../../..
        $ cd dependencies/json2csv-cpp
        $ qmake
        $ make
        $ sudo cp json2csv /usr/bin
        $ cd ../..
        $ cd 3rdparty/qjson
        $ mkdir build
        $ cd build
        $ cmake ..
        $ make
        $ sudo make install
        $ cd ../../..
        $ qmake
        $ make


## Author
Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

## License
This repository contains the code of:

- [TClap](http://tclap.sourceforge.net/) which is licensed under the [MIT license](https://raw.githubusercontent.com/twbs/bootstrap/master/LICENSE).
- [Qt XLSX](https://github.com/dbzhang800/QtXlsxWriter) which is licensed under the [MIT license](https://raw.githubusercontent.com/twbs/bootstrap/master/LICENSE).
- [QJSON](https://github.com/flavio/qjson) which is licensed under the [GNU Lesser General Public License version 2.1](http://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html)
- [MongoDB C++ Driver](https://mongodb.github.io/mongo-cxx-driver/) which is licensed under the [Apache License, version 2.0](https://www.apache.org/licenses/LICENSE-2.0)
- [JSON2CSV-CPP](https://github.com/once-ler/json2csv-cpp) which does not have a license. We assume MIT or BSD.

Otherwise, ODKTools is licensed under [LGPL V3](http://www.gnu.org/licenses/lgpl-3.0.html).