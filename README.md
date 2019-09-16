# ODK Tools
ODK Tools is a toolbox for processing [ODK](https://opendatakit.org/) survey data into MySQL databases. The toolbox can be combined with [FormShare](https://github.com/qlands/FormShare) as it conveniently stores submissions in JSON format but also can process ODK raw submissions in XML format. 

ODK Tools comprises of four command line tools performing different tasks and eight utility programs. The toolbox is cross-platform and can be built in Windows, Linux, and Mac.

## The toolbox

### JSON PyXForm to MySQL (JXFormToMySQL)
JXFormToMySQL converts an the JSON output of [PyXForm](https://github.com/XLSForm/pyxform) into a relational MySQL schema. Even though [ODK Aggregate](https://opendatakit.org/use/aggregate/) stores submissions in MySQL, the Aggregate schema lacks basic functionality like:
 - Avoid duplicated submissions if a unique ID is used in a survey.
 - Store and link select values.
 - Store multi-select values as independent rows.
 - Relational links between repeats and sub-repeats.
 - In some cases, data could end up with too many columns.
 - No dictionary.
 - No multi-language support.
 - No proper support for complex types like Open Street Map (OSM).

 JXFormToMySQL creates a complete relational schema with the following features:
 - A variable can be identified as a unique ID and becomes the primary key.
 - Select and multi-select values are stored in lookup tables. Lookup tables are then linked to main tables.
 - Multi-select values are stored in sub-tables where each value is recorded as an independent row.
 - Repeats are stored in independent tables. Sub-repeats are related to parent-repeats/groups.
 - Repeats and variable names are extracted and accessible.
 - Multiple languages support.
 - Selects create lookup tables.
 - Complex types like OSM, Loops, GeoShapes and GoTraces are stored in separated tables.

 Output files produced:
 - create.sql: A [DDL](http://en.wikipedia.org/wiki/Data_definition_language) script containing all data structures, indexes, and relationships.
 - insert.sql: A [DML](http://en.wikipedia.org/wiki/Data_manipulation_language) script that inserts all the select and multi-select values in the lookup tables.
 - metadata.sql: A DML script that inserts the name of all tables and variables in META's dictionary tables.
 - iso639.sql: A DML script that inserts the name of all tables and variables and lookup values into META's language tables.
 - manifest.xml: This file maps each variable in the ODK survey with its corresponding field in a table in the MySQL database. **This file is used in subsequent tools.**
 - create.xml: This is an XML representation of the schema.
 - insert.xml: This is an XML representation of the lookup tables and values.
 - drop.sql: This script drops all the tables in order.

#### *Parameters*
  - j - Input PyXForm JSON file. This is generated with [xls2json](https://github.com/XLSForm/pyxform/blob/master/pyxform/xls2json.py)
  - v - Main survey variable. This is the variable that is unique to each survey submission. For example National ID, Passport or Farmer Id, etc. **This IS NOT the ODK survey ID found in settings.** This variable will become the primary key in the main table. You can select only **one** variable.
  - t - Main table. Name of the master table for the target schema. ODK surveys do not have a master table however this is necessary to store ODK variables that are not inside a repeat. You can choose any name for example "maintable" **Important note: The main survey variable cannot be stored inside a repeat.**
  - d - Default storing language. For example (en)English. **This is the default language for the database and might be different as the default language in the ODK survey.** If not indicated then (en)English will be assumed.
  - l - Other languages. For example (fr)French, (es)Español. Required if the ODK file has multiple languages.
  - p - Table prefix. A prefix that can be added to each table. This is useful if a common schema is used to store different surveys.
  - c - Output DDL file. "create.sql" by default.
  - i - Output DML file. "insert.sql" by default.
  - m - Output metadata file. "metadata.sql" by default.
  - D - Output drop table sql file. "drop.sql" by default.
  - T - Output translation file. "iso639.sql" by default.
  - f - Output manifest file. "manifest.xml" by default.
  - I - Output lookup tables and values in XML format. "insert.xml" by default.
  - C - Output schema as in XML format. "create.xml" by default.
  - o - Output type: (h)uman readable or (m)achine readable. Machine by default.
  - e - Temporary directory. If no directory is specified then ./tmp will be created.
  - K - Just check. This will only check the ODK form for inconsistencies in the default language.
  - *support files* separated with space. You can indicate multiple support files like XMLs, CSVs or ZIPs. The tool will use XML's or CSVs to collect options from external sources.


#### *Example for a single language ODK*

    $ ./jxformtomysql -j my_input_json_file.json -v QID -t maintable

#### *Example for a multi-language ODK (English and Español) where English is the default storing language*

    $ ./jxformtomysql -j my_input_json_file.json -v main_questionarie_ID -t maintable -l "(es)Español"

#### *Example for a multi-language ODK (English and Español) where Spanish is the default storing language*


    $ ./jxformtomysql -j my_input_json_file.json -v main_questionarie_ID -t maintable -d "(es)Español" -l "(en)English"



---
### FormShare to JSON (FormShareToJSON)
FormShare 1.0 stores ODK submissions in a [Mongo](https://www.mongodb.org/) database. Although FormShare provides exporting functions to CSV and MS Excel it does not provide exporting to more interoperable formats like [JSON](http://en.wikipedia.org/wiki/JSON). FormShare to JSON is a small Python program that extracts survey data from MongoDB to JSON files. Each data submission is exported as a JSON file using FormShare submission UUID as the file name.
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
### XML to JSON (XMLtoJSON)
XMLtoJSON converts ODK XML data submissions and converts them into JSON format. The output is the same as FormShareToJSON.
#### *Parameters*
  - i- Input XML file.
  - o- Output JSON file.
  - x- ODK XForm file **(created with PyXform)**.

#### *Example*

    $ xmltojson -i ./my_input_xml_file.xml -o ./my_output_json_file.json -x ./my_xform_file.xml


### JSON to MySQL (JSONToMySQL)
JSONToMySQL imports JSON files (generated by **FormShareToJSON** or  **XMLtoJSON**) into a MySQL schema generated by **JXFormToMySQL**. The tool imports one file at a time and requires a manifest file. The tool generates an error log file in XML or CSV format and a Map file.

**What is a Map file**

ODK submissions either in XML format or converted into JSON format contains data in a tree-like structure where different branches refer to different types of data. When this structure is imported into a MySQL Database by **JSONToMySQL** each branch of data goes into a different table as a row. This process is called [normalization](https://en.wikipedia.org/wiki/Database_normalization). In some cases, particularly for data analysis, it is necessary to reconstruct the tree structure from the tables ([Denormalize the data](https://en.wikipedia.org/wiki/Denormalization)). This can only be done efficiently by preserving the tree structure but where each branch of data points to a row in a table. **JSONToMySQL** assigns a Universally Unique Identifier ([UUID](https://en.wikipedia.org/wiki/Universally_unique_identifier)) to each branch inserted into a table. The Map file has the same tree structure of the ODK submissions but only containing UUIDs. The tool **MySQLDenormalize** used the map files to generate JSON files out of a MySQL database.


#### *Parameters*
  - H - MySQL host server. Default is localhost.
  - P - MySQL port. Default 3306.
  - s - Schema to be converted.
  - u - User who has select access to the schema.
  - p - Password of the user.
  - m - Input manifest file.
  - i - Imported sqlite file. Store the files names properly imported. Also used to skip repeated files.
  - M - Output directory to store the Map file.
  - j - Input JSON file.
  - o - Output log file. "output.csv" by default.
  - O - Output type: (h)umand or (machine) readable. Machine by default.
  - U - Output UUIDs file. This contains the unique ids pushed to each table.
  - w - Overwrite the log file.
  - S - Write all SQL script lines to a file. 
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

### Merge Versions (mergeVersions) (Utility)
mergeVersions compares two create and insert XML files generated by **JXFormToMySQL** (**A** and **B**) for incremental changes. **A** is consider an incremental version of **B**. This tool is useful when dealing with multiple versions of an ODK survey that must be combined in one common database. The tool informs of tables, variables and options in **A** that ARE NOT in **B**. It also create a combined file **C** with all of **B** plus all of **A**. If a merge is possible the tool will create an alter SQL script to make all the necessary changes in the database to accommodate the data of **A**. 
#### *Parameters*
  - a - Input **A** create file.
  - b - Input **B** create file.
  - A - Input **A** insert file.
  - B - Input **B** insert file.
  - c - Output combined create file. "combined-create.xml" by default.
  - C - Output combined insert file. "combined-insert.xml" by default.
  - d - SQL file to store the structural changes required by the database to accommodate **A**.
  - D - SQL file to store the lookup changes required by the database to accommodate **A**.
  - t - Output type: It can be (h)uman readable or (m)achine readable. By default is machine readable and will produce an XML output
  - s - Save XML output to file.
  - o - Output XML file: Used in combination with s.
  - i - Ignore changes in value descriptions. Indicated like Table1:value1,value2,...;Table2:value1,value2,..;.. . This is required to tell that tool that certain changes in options are allowed. For example to fix a typo.

#### *Nomenclature:*

The tool generates an output based on the possibility of merging the versions reporting each change with a code.

Changes allowed from B to A:

  - TNF: Table in A not found in B. It will be added to C.
  - FNF: Field in A not found in B. It will be added to C.
  - VNF: Lookup value in A not found in B. It will be added to C.
  - FIC: The field size in A is bigger than in B. It will be increased in C to match the size of in A.
  - FDC: The field in A is smaller than in B. This is a decremental change thus will be ignored.
  - CHR: The referenced lookup table+field in A is different than in B, however the new lookup table in A has the same values or more than the previous one in B. The referenced lookup table+field will be changed in C.
  - FTC: Field type changed from int in B to varchar A. This is allowed because a varchar can hold numeric characters. The field will be changed to varchar in C.

Changes **NOT** allowed from B to A:

- TNS: The table does not have the same parent table.
- TWP: The parent table is not found in B.
- FNS: The field is not the same and such change will generate inconsistencies in the data. An example is to change a variable from categorical to continuous.
- VNS: The description of a lookup value changed from B to A. For example 1-Male in B changed to 1-Female in A. This can be ignored with the i parameter at your own risk.
- RNS: The related lookup table changed from B to A.

#### *Example*

    $ ./mergeversions -a ./my_A_create_file.xml -b ./my_B_create_file.xml -A ./my_A_insert_file.xml -B ./my_B_insert_file.xml -o h

---
### Create From XML (createFromXML) (Utility)
Create From XML creates a SQL DDL script file from a XML schema file generated either by **JXFormToMySQL** or **mergeVersions**.

#### *Parameters*
  - i - Input create XML file.  
  - o - Output SQL DDL file. "create.sql" by default.

#### *Example*


    $ ./createfromxml -i ./my_create_file.xml -o ./my_output_sql_file.sql

---
### Insert From XML (insertFromXML) (Utility)
Insert From XML creates a SQL DML script file from a XML insert file generated either by **JXFormToMySQL** or **mergeVersions**.

#### *Parameters*
  - i - Input insert XML file.  
  - o - Output SQL DML file. "insert.sql" by default.

#### *Example*

    $ ./insertfromxml -i ./my_insert_file.xml -o ./my_output_sql_file.sql

---
### MySQL Denormalize (MySQLDenormalize) (Utility)
MySQL Denormalize converts data from a MySQL Database into the tree representation of the original submission but with the data coming from the database thus containing any changes made to the data after the original submission. It relies on the Map XML files created by **JXFormToMySQL**.

#### *Parameters*
  - H - MySQL host server. Default is localhost.
  - P - MySQL port. Default 3306.
  - s - Schema to be converted.
  - u - User who has select access to the schema.
  - p - Password of the user.
  - t - Main table.
  - T - Path to a temporary directory.
  - m - Directory containing the Map files generated by  **JXFormToMySQL**.
  - o - Output directory to store the JSON files for each submission.
  - c - Input create XML file from **JXFormToMySQL**.
  - S - Separate multi-select variables in different keys.

#### *Example*

    $ ./mysqldenormalize -H my_MySQL_server -u my_user -p my_pass -s my_schema -t main_table -m /path/to/the/map/files -o /path/to/output/directory -c /path/to/my/create.xml

---
### Create Audit Triggers (createAuditTriggers) (Utility)
createAuditTriggers creates the audit triggers based on a MySQL schemata. It creates the triggers for both MySQL and Sqlite to be used with **MySQLToSQLite**

#### *Parameters*
- H - MySQL host server. Default is localhost.
- P - MySQL port. Default 3306.
- s - Schema to be converted.
- u - User who has select access to the schema.
- p - Password of the user.
- o - Output directory to store the trigger files.
- t - Coma separated list of tables to generate audit. Empty (default) means all.

#### *Example*

    $ ./createaudittriggers -H my_MySQL_server -u my_user -p my_pass -s my_schema -o /path/to/my/directory

------

### Create Dummy JSON (createDummyJSON) (Utility)

createDummyJSON creates a dummy data file in JSON format based on the create and insert XML files. This tool is useful when flatting JSONs into a CSV format while conserving a proper order of the variables.

#### *Parameters*

- c - Input create XML file from **JXFormToMySQL**.
- i - Input insert XML file from **JXFormToMySQL**.
- o - Output JSON file.
- a - List of array sizes as defined as name:size,name:size.
- r - Generate the keys using repository names instead of ODK xml variable names.
- s - Separate multi-selects in different columns.

#### *Example*

```
$ ./createdummyjson -c ./my_create_xml_file.xml -i ./my_insert_xml_file.xml -o ./my_output_file.json
```

------

### MySQL to SQLite (MySQLToSQLite) (Utility)

MySQLToSQLite creates a SQLite database from a ODK Tools MySQL database. The resulting SQLite database will have the audit created by **createAuditTriggers**.

#### *Parameters*

- H - MySQL host server. Default is localhost.
- P - MySQL port. Default 3306.
- s - Schema to be converted.
- u - User who has select access to the schema.
- p - Password of the user.
- a - SQLite audit file.
- c - Create XML file from **JXFormToMySQL.**
- o - Output SQLite database file.

#### *Example*

```
$ ./mysqltosqlite -H my_MySQL_server -u my_user -p my_pass -s my_schema -o /my_file.sqlite -a /path/to/my/sqlite_create_audit_file.sql -c /path/to/my/create.xml
```

------

### MySQL to XLSX (MySQLToXLSX) (Utility)

MySQLToXLSX extracts data from a ODK Tools MySQL Database into XLSX files. Each table will create a new sheet in the excel file. The tool requires the create.xml file created by JXFormToMySQL to determine the type of data and whether a field or a table should be exported due to the sensitivity of its information.

#### *Parameters*

- H - MySQL host server. Default is localhost.
- P - MySQL port. Default 3306.
- s - Schema to be converted.
- u - User who has select access to the schema.
- p - Password of the user.
- T - Temporary directory to use. ./tmp by default.
- I - Insert XML file from **JXFormToMySQL**.
- x - Create XML file from **JXFormToMySQL**.
- o - Output XLSX file.
- f - Name for the first sheet.
- i - Include fields marked as sensitive in the create XML file.
- l - Include lookup tables as sheets. False by default.
- m - Include multi-select tables as sheets. False by default.
- S - Separate multi-select variables in multiple columns.

#### *Example*

```
$ ./mysqltosqlite -H my_MySQL_server -u my_user -p my_pass -s my_schema -o /my_file.sqlite -a /path/to/my/sqlite_create_audit_file.sql -c /path/to/my/create.xml
```

------



## Building and testing

Installation instructions for Ubuntu Server 16.04 or 18.04 are available [here](https://github.com/qlands/odktools/blob/master/AWS_build_script.txt)


## Author
Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

## License
This repository contains the code of:

- [TClap](http://tclap.sourceforge.net/) which is licensed under the [MIT license](https://raw.githubusercontent.com/twbs/bootstrap/master/LICENSE).
- [MongoDB C++ Driver](https://mongodb.github.io/mongo-cxx-driver/) which is licensed under the [Apache License, version 2.0](https://www.apache.org/licenses/LICENSE-2.0)

Otherwise, ODKTools is licensed under [LGPL V3](http://www.gnu.org/licenses/lgpl-3.0.html).
