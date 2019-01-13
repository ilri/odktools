#!/bin/bash
../../../odktomysql -x ./Spanish-English.xlsx -t maintable -v QID -d '(es)Español' -l '(en)English' -y 'Si|No'
