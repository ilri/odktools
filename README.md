# ODK Tools
ODK Tools is a toolbox for processing [ODK](https://opendatakit.org/) survey data into MySQL databases. The toolbox relies on [OnaData](https://github.com/onaio/onadata) or [Formhub](https://github.com/SEL-Columbia/formhub) as a temporary storage of ODK submissions because they conveniently store them in JSON format; and on [META](https://github.com/ilri/meta) for storing the dictionary information and to support multiple languages. ODK Tools comprises of four command line tools performing different tasks and four utility programs. The toolbox is cross-platform and can be build in Windows, Linux and Mac.

## The toolbox

### ODKToMySQL
ODKToMySQL converts a ODK Excel File (XLSX survey file) into a relational MySQL schema. Even though [ODK Aggregate](https://opendatakit.org/use/aggregate/) stores submissions in MySQL, the Aggregate schema lacks basic functionality like:
 - Avoid duplicated submissions if an unique ID is used in a survey.
 - Store and link select values.
 - Store multi-select values as independent rows.
 - Relational links between repeats and sub-repeats.
 - In some cases data could end up not normalized.
 - No dictionary.
 - No multi-language support.

 The tools creates a complete relational schemas with the following features:
 - A variable can be identified as a unique ID and becomes a primary key.
 - Select and multi-select values are stored in lookup tables. Lookup tables are then related to main tables.
 - Multi-select values are stored in sub tables where each value is recorded as an independent row.
 - Repeats are stored in independent tables sub-repeats are related to parent-repeats.
 - Repeats and variable names are stored in METAS's dictionary tables.
 - Multiple languages support.
 - Duplicated selects are avoided.
 - Yes/No selects are ignored
 - Tables with more than 100 fields (an indicator for not normalized data) are separated into sub-tables. This separation is done by the user using a separation file (created after running this tool).

 Output files produced:
 - create.sql: A [DDL](http://en.wikipedia.org/wiki/Data_definition_language) script containing all data structures, indexes and relationships.
 - insert.sql: A [DML](http://en.wikipedia.org/wiki/Data_manipulation_language) script that inserts all the select and multi-select values in the lookup tables.
 - uuid-triggers.sql: A script containing code for storing each row in each table with an [Universally unique identifier](http://en.wikipedia.org/wiki/Universally_unique_identifier) (UUID).
 - metadata.sql: A DML script that inserts the name of all tables and variables in META's dictionary tables.
 - iso639.sql: A DML script that inserts the name of all tables and variables and lookup values into META's language tables.
 - separation-TimeStamp.xml: This file is created when one or more tables might not be normalized (contains more than 100 fields). This separation file can be edited to normalize the tables and then used as an input file for this tool.
 - manifest.xml: This file maps each variable in the ODK survey with its corresponding field in a table in the MySQL database. **This file is used in subsequent tools.**
 - create.xml: This is a XML representation of the schema. Used by utilities compareCreateXML & createFromXML.
 - insert.xml: This is a XML representation of the lookup tables and values. Used by utilities compareInsertXML & insertFromXML.

#### *Parameters*
  - x - Input ODK XLSX file.
  - v - Main survey variable. This is the unique ID of each survey. **This IS NOT the ODK survey ID found in settings.**
  - t - Main table. Name of the master table for the target schema. ODK surveys do not have a master table however this is necessary to store ODK variables that are not inside a repeat. **If the main survey variable is store inside a repeat then the main table is such repeat.**
  - d - Default storing language. For example: (en)English. **This is the default language for the database and might be different as the default language in the ODK survey.** If not indicated then English will be assumed.
  - l - Other languages. For example: (fr)French,(es)Español. Required if the ODK file has multiple languages.
  - y - Yes and No strings in the default language in the format "String|String". This will allow the tool to identify Yes/No lookup tables and exclude them. **It is not case sensitive.** For example, if the default language is Spanish then this value should be indicated as "Si|No". If its empty then English "Yes|No" will be assumed.
  - p - Table prefix. A prefix that can be added to each table. This is useful if a common schema is used to store different surveys.
  - c - Output DDL file. "create.sql" by default.
  - i - Output DML file. "insert.sql" by default.
  - u - Output UUID trigger file. "uuid-triggers.sql" by default.  
  - m - Output metadata file. "metadata.sql" by default.
  - T - Output translation file. "iso639.sql" by default.
  - f - Output manifest file. "manifest.xml" by default
  - s - Input separation file. This file might be generated by this tool.
  - I - Output lookup tables and values in XML format. "insert.xml" by default.
  - C - Output schema as in XML format. "create.xml" by default


#### *Example for a single language ODK*
  ```sh
$ ./odktomysql -x my_input_xlsx_file.xlsx -v QID -t maintable
```
#### *Example for a multi-language ODK (English and Español) where English is the default storing language*
  ```sh
$ ./odktomysql -x my_input_xlsx_file.xlsx -v main_questionarie_ID -t maintable -l "(es)Español"
```
#### *Example for a multi-language ODK (English and Español) where Spanish is the default storing language*
  ```sh
$ ./odktomysql -x my_input_xlsx_file.xlsx -v main_questionarie_ID -t maintable -d "(es)Español" -l "(en)English" -y "Si|No"
```
  See examples for single and multiple languages [here](https://github.com/ilri/odktools/tree/master/ODKToMySQL/examples)

---
### FormhubToJSON
OnaData/Formhub stores ODK submissions in a [Mongo](https://www.mongodb.org/) database. Although OnaData/Formhub provides exporting functions to CSV and MS Excel it do not provide exporting to more interoperable formats like [JSON](http://en.wikipedia.org/wiki/JSON). FormhubToJSON is a small Python program that extracts survey data from MongoDB to JSON files. Each data submission is exported as a JSON file using OnaData/Formhub submission UUID as the file name.
#### *Parameters*
  - m - URI for the Mongo Server. For example mongodb://localhost:2701
  - d - OnaData/Formhub database. "formhub" by default.
  - c - OnaData/Formhub collection storing the surveys. "instances" by default.
  - y - ODK Survey ID. This is found in the "settings" sheet of the ODK XLSX file.
  - o - Output directory. "./output" by default (created if it does not exists).
  - w - Overwrite JSON file if exists. False by default.

#### *Example*
  ```sh
   $ python formhubtojson.py -m "mongodb://my_mongoDB_server:27017/" -y my_survey_id -o ./my_output_directory
```

---
### JSONToMySQL
JSONToMySQL imports JSON files (generated by **FormhubToJSON**) into a MySQL schema generated by **ODKToMySQL**. The tool imports one file at a time and requires a manifest file (see **ODKToMySQL**). The tool generates an error log file in CSV format.
#### *Parameters*
  - H - MySQL host server. Default is localhost
  - P - MySQL port. Default 3306
  - s - Schema to be converted
  - u - User who has select access to the schema
  - p - Password of the user
  - m - Input manifest file.
  - j - Input JSON file.
  - o - Output log file. "output.csv" by default.
  - J - Input JavaScript file with a "beforeInsert" function to customize the values entering the database. **See notes below**.

#### *Example*
  ```sh
$ ./jsontomysql -H my_MySQL_server -u my_user -p my_pass -s my_schema -m ./my_manifest_file.xml -j ./my_JSON_file.json -o my_log_file.csv
```

#### *Customizing the insert with an external JavaScript*
 The insert process of JSONToMySQL can be hooked up to an external JavaScript file to perform changes in the data before insert them into the MySQL database. This is done by creating a .js file with the following code:

 ```javascript
 function beforeInsert(table,data)
{
   // My before insert code goes here
}
 ```
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
### ODKDataToMySQL
ODKDataToMySQL imports ODK XML data files (device data) into a MySQL schema generated by **ODKToMySQL**. The tool imports one file at a time and requires a manifest file (see **ODKToMySQL**). The tool generates an error log file in CSV format.
#### *Parameters*
  - H - MySQL host server. "localhost" by default
  - P - MySQL port. "3306" by default.
  - s - Schema to be converted.
  - u - User who has insert access to the schema.
  - p - Password of the user.
  - m - Input manifest file.
  - d - Input ODK XML file.
  - o - Output log file. "output.csv" by default.

#### *Example*
  ```sh
$ ./odkdatatomysql -H my_MySQL_server -u my_user -p my_pass -s my_schema -m ./my_manifest_file.xml -d ./my_ODK_XML_Data_file.xml -o my_log_file.csv
```

---
### compareCreateXML (Utility)
compareCreateXML compares two create XML files (**A** and **B**) for incremental changes. This tool is useful when dealing with multiple versions of an ODK survey that must be combined in one common database. The tool informs of tables and variables in **A** that ARE NOT in **B**. It can also create a combined file **C** with all of **B** plus all of **A** with the following condition:
  1. If a table in **A** is not found in **B** then it will be added to **C** only if its parent exists in **B**.

The tool WILL NOT fix the following:
  1. Inconsistencies in field definition between **A** and **B** like size, type, parent table and parent field.
  2. Tables that do not share the same parent.

#### *Nomenclature:*
  - TNF: Table not found.
  - TNS: The table does not have the same parent table. (The tool will not fix it in **C**)
  - FNF: Field not found.
  - FNS: The field is not the same. (The tool will not fix it in **C**)

#### *Parameters*
  - a - Input **A** file.
  - b - Input **B** file.
  - c - Output combined file. "combined-create.xml" by default.

#### *Example*
  ```sh
$ ./comparecreatexml -a ./my_A_file.xml -b ./my_B_file.xml -c ./my_output_C_file.xml
```

---
### compareInsertXML (Utility)
compareInsertXML compares two insert XML files (**A** and **B**) for incremental changes. This tool is useful when dealing with multiple versions of an ODK survey that must be combined in one common database. The tool informs of lookup tables and their values in **A** that ARE NOT in **B**. It can also create a combined file **C** with all of **B** plus all of **A**.

The tool WILL NOT fix the following:
  1. Inconsistencies in value code and description.

#### *Nomenclature:*
  - TNF: Table not found.  
  - VNF: Value not found.
  - VNS: The value is not the same. (The tool will not fix it in **C**)

#### *Parameters*
  - a - Input **A** file.
  - b - Input **B** file.
  - c - Output combined file. "combined-insert.xml" by default.

#### *Example*
  ```sh
$ ./compareinsertxml -a ./my_A_file.xml -b ./my_B_file.xml -c ./my_output_C_file.xml
```

---
### createFromXML (Utility)
createFromXML creates a SQL DDL script file from a XML schema file generated either by ODKToMySQL or compareCreateXML.

#### *Parameters*
  - i - Input create XML file.  
  - o - Output SQL DDL file. "create.sql" by default.

#### *Example*
  ```sh
$ ./createfromxml -i ./my_create_file.xml -o ./my_output_sql_file.sql
```

---
### insertFromXML (Utility)
insertFromXML creates a SQL DML script file from a XML insert file generated either by ODKToMySQL or compareInsertXML.

#### *Parameters*
  - i - Input insert XML file.  
  - o - Output SQL DML file. "insert.sql" by default.

#### *Example*
  ```sh
$ ./insertfromxml -i ./my_insert_file.xml -o ./my_output_sql_file.sql
```

## Technology
ODK Tools was built using:

- [C++](https://isocpp.org/), a general-purpose programming language.
- [Qt 5](https://www.qt.io/), a cross-platform application framework.
- [Python 2.7.x](https://www.python.org/), a widely used general-purpose programming language.
- [TClap](http://tclap.sourceforge.net/), a small, flexible library that provides a simple interface for defining and accessing command line arguments. *(Included in source code)*
- [Qt XLSX](https://github.com/dbzhang800/QtXlsxWriter), a XLSX file reader and writer for Qt5. **Requires QTInternals (e.g., apt-get qtbase5-private-dev)**. *(Included in source code)*
- [QJSON](https://github.com/qlands/qjson), a qt-based library that maps JSON data to QVariant objects. *(Included in source code)*
- [CMake] (http://www.cmake.org/), a cross-platform free and open-source software for managing the build process of software using a compiler-independent method.


## Building and testing
To build ODKTools on Linux do:

    $ git clone https://github.com/ilri/odktools.git
    $ cd odktools
    $ git submodule update --init --recursive
    $ cd 3rdparty/qjson
    $ mkdir build
    $ cd build
    $ cmake ..
    $ sudo make install
    $ cd ../..
    $ qmake
    $ make

## Author
Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

Harrison Njmaba (h.njamba_at_cgiar.org)

## License
This repository contains the code of:

- [TClap](http://tclap.sourceforge.net/) which is licensed under the [MIT license](https://raw.githubusercontent.com/twbs/bootstrap/master/LICENSE).
- [Qt XLSX](https://github.com/dbzhang800/QtXlsxWriter) which is licensed under the [MIT license](https://raw.githubusercontent.com/twbs/bootstrap/master/LICENSE).
- [QJSON](https://github.com/qlands/qjson) which is licensed under the [GNU Lesser General Public License version 2.1](http://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html)

Otherwise, ODKToMySQL is licensed under [LGPL V3](http://www.gnu.org/licenses/lgpl-3.0.html).
