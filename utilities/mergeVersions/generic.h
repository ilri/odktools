/*
Merge Versions

Copyright (C) 2019 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com)

Merge Versions is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

Merge Versions is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with Merge Versions.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#ifndef GENERIC_H
#define GENERIC_H
#include <QString>

struct tableDiff
{
  QString table;
  QStringList diff;
  bool parsed = false;
};
typedef tableDiff TtableDiff;

struct compError
{
  QString code;
  QString desc;
  QString table;
  QString field;
  QString value;
  QString from;
  QString to;
};
typedef compError TcompError;

#endif // GENERIC_H
