#!/usr/bin/env python
# -*- coding: utf-8 -*-


# This file is part of ODKTools.
#
# Copyright (C) 2015 International Livestock Research Institute.
# Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)
#
# ODKTools is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# ODKTools is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with ODKTools.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.


import getopt #Library for processing command line argument
import sys #System library
import pymongo #The Python MongoDB library. Get it from  http://api.mongodb.org/python/current/installation.html
import os #Library of OS operations
import json #The json library
import io
import pprint

from pymongo import MongoClient

#This function export all Formhub uuids to json files filtering by surveyID
def exportSurvey(mongoColl, surveyID, outputDir, overWrite):

    skipcount = 0
    surveys = mongoColl.find({"_xform_id_string" : surveyID}) #Find all Surveys with that surveyID
    for survey in surveys:
        #If the uuid of the survey does not exist or we want to rewrite existing files
        if not os.path.isfile(outputDir + "/"  + survey["_uuid"] + ".json") or overWrite == True:
            #Dump the survey into the json file
            with open(outputDir + "/"  + survey["_uuid"] + ".json", "w") as outfile:
                jsonString = json.dumps(survey,indent=4,ensure_ascii=False).encode("utf8")
                outfile.write(jsonString)

        else:
            print "Survey id " + survey["_uuid"] + " already exists. Skipping it"
            skipcount = skipcount + 1
    if skipcount > 0:
        print "Total skipped: " + str(skipcount)

#Print the help of the script
def usage():
    helptext = '\nFormHub to JSON\n\n'
    helptext = helptext + 'This script export Formhub data submissions from MongoDB to JSON files. \n'
    helptext = helptext + 'Each submission is exported with the UUID as file name.\n'
    helptext = helptext + 'Parameters:\n'


    helptext = helptext + '-y --surveyID : Survey ID to process \n'
    helptext = helptext + '-o --output : Output directory. Default ./output (Created if not exists)  \n'
    helptext = helptext + '-m --mongoURI : URI for the Mongo Server. For example mongodb://localhost:27017/ \n'
    helptext = helptext + '-d --mongoDB : Formhub database. Usually "formhub" \n'
    helptext = helptext + '-c --mongoCollection : Formhub collection storing the surveys. Usually "instances" \n'
    helptext = helptext + '-w : Overwrite JSON files if exist. Otherwise JSON data will be ignored. Default false \n'
    helptext = helptext + '-l --help : Print this help \n\n'
    helptext = helptext + "FormHubToJSON (c) ILRI, 2014\n"
    helptext = helptext + "Author: Carlos Quiros. c.f.quiros@cgiar.org / cquiros@qlands.com"
    print helptext

# Main function
def main():
    #Obtain the comamnd line arguments
    try:
        opts, args = getopt.getopt(sys.argv[1:], "ly:o:m:d:c:w", ["help"," surveyID=",  "output=", "mongoURI=", "mongoDB=",  "mongoCollection="])
    except getopt.GetoptError, err:
        print str(err) #If there is an error then print it
        usage() #Print the help
        sys.exit(1) #Exits with error

    #Input variables

    survey = 'mySurveyID' #The SQLite input database
    outputDir = './output' #The file to log the error
    overWrite = False #If the process overwrites the log file
    mongoURI = 'mongodb://localhost:27017/' #Connect to Mongo in ocalhost
    mongoDB = "formhub" #Database containing the surveys
    mongoCollection = 'instances' #Collection containing the surveys

    if len(opts) == 0:
        usage() #Print the help
        sys.exit(1) #Exits with error

    #This for statement gets each command line argument and fill avobe variables
    for o, a in opts:
        if o in ("-w"):
            overWrite = True
        elif o in ("-l", "--help"):
            usage()
            sys.exit()
        elif o in ("-y", "--survey"):
            survey = a
        elif o in ("-o", "--output"):
            outputDir = a
        elif o in ("-m", "--mongoURI"):
            mongoURI = a
        elif o in ("-d", "--mongoDB"):
            mongoDB = a
        elif o in ("-c", "--mongoCollection"):
            mongoCollection = a
        else:
            assert False, "unhandled option"

    #Print some of the variables

    print 'SurveyID          :', survey
    print 'MongoURI          :', mongoURI
    print 'MongoDB           :',  mongoDB
    print 'Collection        :', mongoCollection
    print 'Outoput Dir       :', outputDir
    print 'Overwrite output  :', overWrite

    #If the output directory does not exist then create it
    try:
        if not os.path.isdir(outputDir):
            os.makedirs(outputDir)
    except Exception,e:
        print str(e)
        sys.exit(1)

    #Try to connect to Mongo a
    try:
      mongoCon = MongoClient(mongoURI)
      mngdb = mongoCon[mongoDB]
      mngcoll = mngdb[mongoCollection]

      if mngcoll.count() == 0:
          print 'Collection "' + mongoCollection + '" does not have documents'  #Print the error
          sys.exit(1) #Exits with error

      if mngcoll.find_one({"_xform_id_string" : survey}) is None:
          print 'Collection "' + mongoCollection + '" does not have surveys with ID = ' + survey  #Print the error
          sys.exit(1) #Exits with error

      exportSurvey(mngcoll, survey, outputDir,overWrite) #Export the surveys

    except pymongo.errors.ConnectionFailure as e:
      print 'Cannot connect to Mongo database with URI ' + mongoURI #Print the error

    except pymongo.errors.ConfigurationError as ec:
      print 'Cannot connect to Mongo database. The URI might be malformed. Use something like "mongodb://localhost:27017/" '  #Print the error
      sys.exit(1) #Exits with error

#Load the main function at start
if __name__ == "__main__":
    main()
