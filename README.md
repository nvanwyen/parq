# parq
A generic parquet file reader

```
$ home/parq --help
Usage: parq [options] <parquet_file> [file2] [file3] ...
Options:
  -t, --tabular        Output in tabular format (default)
  -j, --json           Output in JSON format
  -c, --csv            Output in CSV format
  -x, --xml            Output in XML format
  -l, --limit <n>      Limit output to first n rows
  -m, --metadata       Show only column metadata information
  -C, --columns <list> Show only specified columns (comma-delimited)
      --case <upper|lower> Set output case (default: lower)
  -h, --help           Show this help message

Note: Output format options are mutually exclusive.
Multiple files can be processed in sequence.
```

## Examples
The following are some simple example to help get you started.

### Formatted output
The output can be in tabular (default), json, xml of csv format, and note that
more than one file can be passed in, each being processed separately. The output
formats cannot be used together, as the are  mutually exclusive. The other
options, such as --limit, --columns <list> and --case may be used with each
output type.

#### Tabular
```
$ home/parq test1.parquet test2.parquet 
File: test1.parquet
Rows: 10, Columns: 11

+----------+-----------+-------+---------+-----------+-----------+-------+----------+----------+----------+-------------------+
|    carat | cut       | color | clarity |     depth |     table | price |        x |        y |        z | __index_level_0__ |
+----------+-----------+-------+---------+-----------+-----------+-------+----------+----------+----------+-------------------+
| 0.230000 | Ideal     | E     | SI2     | 61.500000 | 55.000000 |   326 | 3.950000 | 3.980000 | 2.430000 |                 0 |
| 0.210000 | Premium   | E     | SI1     | 59.800000 | 61.000000 |   326 | 3.890000 | 3.840000 | 2.310000 |                 1 |
| 0.230000 | Good      | E     | VS1     | 56.900000 | 65.000000 |   327 | 4.050000 | 4.070000 | 2.310000 |                 2 |
| 0.290000 | Premium   | I     | VS2     | 62.400000 | 58.000000 |   334 | 4.200000 | 4.230000 | 2.630000 |                 3 |
| 0.310000 | Good      | J     | SI2     | 63.300000 | 58.000000 |   335 | 4.340000 | 4.350000 | 2.750000 |                 4 |
| 0.240000 | Very Good | J     | VVS2    | 62.800000 | 57.000000 |   336 | 3.940000 | 3.960000 | 2.480000 |                 5 |
| 0.240000 | Very Good | I     | VVS1    | 62.300000 | 57.000000 |   336 | 3.950000 | 3.980000 | 2.470000 |                 6 |
| 0.260000 | Very Good | H     | SI1     | 61.900000 | 55.000000 |   337 | 4.070000 | 4.110000 | 2.530000 |                 7 |
| 0.220000 | Fair      | E     | VS2     | 65.100000 | 61.000000 |   337 | 3.870000 | 3.780000 | 2.490000 |                 8 |
| 0.230000 | Very Good | H     | VS1     | 59.400000 | 61.000000 |   338 | 4.000000 | 4.050000 | 2.390000 |                 9 |
+----------+-----------+-------+---------+-----------+-----------+-------+----------+----------+----------+-------------------+

================================================================================

File: test2.parquet
Rows: 10, Columns: 10

+----------+-----------+-----------+-------+----------+----------+----------+-----------+-------+---------+
|    carat |     depth |     table | price |        x |        y |        z | cut       | color | clarity |
+----------+-----------+-----------+-------+----------+----------+----------+-----------+-------+---------+
| 0.220000 | 65.100000 | 61.000000 |   337 | 3.870000 | 3.780000 | 2.490000 | Fair      | E     | VS2     |
| 0.230000 | 56.900000 | 65.000000 |   327 | 4.050000 | 4.070000 | 2.310000 | Good      | E     | VS1     |
| 0.310000 | 63.300000 | 58.000000 |   335 | 4.340000 | 4.350000 | 2.750000 | Good      | J     | SI2     |
| 0.230000 | 61.500000 | 55.000000 |   326 | 3.950000 | 3.980000 | 2.430000 | Ideal     | E     | SI2     |
| 0.210000 | 59.800000 | 61.000000 |   326 | 3.890000 | 3.840000 | 2.310000 | Premium   | E     | SI1     |
| 0.290000 | 62.400000 | 58.000000 |   334 | 4.200000 | 4.230000 | 2.630000 | Premium   | I     | VS2     |
| 0.260000 | 61.900000 | 55.000000 |   337 | 4.070000 | 4.110000 | 2.530000 | Very Good | H     | SI1     |
| 0.230000 | 59.400000 | 61.000000 |   338 | 4.000000 | 4.050000 | 2.390000 | Very Good | H     | VS1     |
| 0.240000 | 62.300000 | 57.000000 |   336 | 3.950000 | 3.980000 | 2.470000 | Very Good | I     | VVS1    |
| 0.240000 | 62.800000 | 57.000000 |   336 | 3.940000 | 3.960000 | 2.480000 | Very Good | J     | VVS2    |
+----------+-----------+-----------+-------+----------+----------+----------+-----------+-------+---------+
```

#### Limits
The limit option, restricts the number of rows output.
```
$ home/parq --limit 3 test1.parquet test2.parquet 
File: test1.parquet
Rows: 10, Columns: 11

+----------+---------+-------+---------+-----------+-----------+-------+----------+----------+----------+-------------------+
|    carat | cut     | color | clarity |     depth |     table | price |        x |        y |        z | __index_level_0__ |
+----------+---------+-------+---------+-----------+-----------+-------+----------+----------+----------+-------------------+
| 0.230000 | Ideal   | E     | SI2     | 61.500000 | 55.000000 |   326 | 3.950000 | 3.980000 | 2.430000 |                 0 |
| 0.210000 | Premium | E     | SI1     | 59.800000 | 61.000000 |   326 | 3.890000 | 3.840000 | 2.310000 |                 1 |
| 0.230000 | Good    | E     | VS1     | 56.900000 | 65.000000 |   327 | 4.050000 | 4.070000 | 2.310000 |                 2 |
+----------+---------+-------+---------+-----------+-----------+-------+----------+----------+----------+-------------------+

================================================================================

File: test2.parquet
Rows: 10, Columns: 10

+----------+-----------+-----------+-------+----------+----------+----------+------+-------+---------+
|    carat |     depth |     table | price |        x |        y |        z | cut  | color | clarity |
+----------+-----------+-----------+-------+----------+----------+----------+------+-------+---------+
| 0.220000 | 65.100000 | 61.000000 |   337 | 3.870000 | 3.780000 | 2.490000 | Fair | E     | VS2     |
| 0.230000 | 56.900000 | 65.000000 |   327 | 4.050000 | 4.070000 | 2.310000 | Good | E     | VS1     |
| 0.310000 | 63.300000 | 58.000000 |   335 | 4.340000 | 4.350000 | 2.750000 | Good | J     | SI2     |
+----------+-----------+-----------+-------+----------+----------+----------+------+-------+---------+
```

#### Columns
```
$ home/parq --limit 3 --columns "carat,depth,clarity,price,x,__index_level_0__" test1.parquet test2.parquet
File: test1.parquet
Rows: 10, Columns: 11

+----------+-----------+---------+-------+----------+-------------------+
|    carat |     depth | clarity | price |        x | __index_level_0__ |
+----------+-----------+---------+-------+----------+-------------------+
| 0.230000 | 61.500000 | SI2     |   326 | 3.950000 |                 0 |
| 0.210000 | 59.800000 | SI1     |   326 | 3.890000 |                 1 |
| 0.230000 | 56.900000 | VS1     |   327 | 4.050000 |                 2 |
+----------+-----------+---------+-------+----------+-------------------+

================================================================================

File: test2.parquet
Rows: 10, Columns: 10

Warning: Column '__index_level_0__' not found in parquet file
+----------+-----------+---------+-------+----------+
|    carat |     depth | clarity | price |        x |
+----------+-----------+---------+-------+----------+
| 0.220000 | 65.100000 | VS2     |   337 | 3.870000 |
| 0.230000 | 56.900000 | VS1     |   327 | 4.050000 |
| 0.310000 | 63.300000 | SI2     |   335 | 4.340000 |
+----------+-----------+---------+-------+----------+
```

#### JSON, XML, CSV formats
```
$ home/parq --json --limit 3 --columns "carat,depth,clarity,price,x,__index_level_0__" test1.parquet test2.parquet
File: test1.parquet
Rows: 10, Columns: 11

[
  {
    "carat": "0.230000",
    "depth": "61.500000",
    "clarity": "SI2",
    "price": "326",
    "x": "3.950000",
    "__index_level_0__": "0"
  },
  {
    "carat": "0.210000",
    "depth": "59.800000",
    "clarity": "SI1",
    "price": "326",
    "x": "3.890000",
    "__index_level_0__": "1"
  },
  {
    "carat": "0.230000",
    "depth": "56.900000",
    "clarity": "VS1",
    "price": "327",
    "x": "4.050000",
    "__index_level_0__": "2"
  }
]

================================================================================

File: test2.parquet
Rows: 10, Columns: 10

Warning: Column '__index_level_0__' not found in parquet file
[
  {
    "carat": "0.220000",
    "depth": "65.100000",
    "clarity": "VS2",
    "price": "337",
    "x": "3.870000"
  },
  {
    "carat": "0.230000",
    "depth": "56.900000",
    "clarity": "VS1",
    "price": "327",
    "x": "4.050000"
  },
  {
    "carat": "0.310000",
    "depth": "63.300000",
    "clarity": "SI2",
    "price": "335",
    "x": "4.340000"
  }
]
```
```
$ home/parq --xml --limit 3 --columns "carat,depth,clarity,price,x,__index_level_0__" test1.parquet test2.parquet
File: test1.parquet
Rows: 10, Columns: 11

<?xml version="1.0" encoding="UTF-8"?>
<parquet-data>
  <metadata>
    <file>test1.parquet</file>
    <rows>10</rows>
    <columns>11</columns>
  </metadata>
  <records>
    <record row="0">
      <carat>0.230000</carat>
      <depth>61.500000</depth>
      <clarity>SI2</clarity>
      <price>326</price>
      <x>3.950000</x>
      <__index_level_0__>0</__index_level_0__>
    </record>
    <record row="1">
      <carat>0.210000</carat>
      <depth>59.800000</depth>
      <clarity>SI1</clarity>
      <price>326</price>
      <x>3.890000</x>
      <__index_level_0__>1</__index_level_0__>
    </record>
    <record row="2">
      <carat>0.230000</carat>
      <depth>56.900000</depth>
      <clarity>VS1</clarity>
      <price>327</price>
      <x>4.050000</x>
      <__index_level_0__>2</__index_level_0__>
    </record>
  </records>
</parquet-data>

================================================================================

File: test2.parquet
Rows: 10, Columns: 10

Warning: Column '__index_level_0__' not found in parquet file
<?xml version="1.0" encoding="UTF-8"?>
<parquet-data>
  <metadata>
    <file>test2.parquet</file>
    <rows>10</rows>
    <columns>10</columns>
  </metadata>
  <records>
    <record row="0">
      <carat>0.220000</carat>
      <depth>65.100000</depth>
      <clarity>VS2</clarity>
      <price>337</price>
      <x>3.870000</x>
    </record>
    <record row="1">
      <carat>0.230000</carat>
      <depth>56.900000</depth>
      <clarity>VS1</clarity>
      <price>327</price>
      <x>4.050000</x>
    </record>
    <record row="2">
      <carat>0.310000</carat>
      <depth>63.300000</depth>
      <clarity>SI2</clarity>
      <price>335</price>
      <x>4.340000</x>
    </record>
  </records>
</parquet-data>
```
```
$ home/parq --csv --limit 3 --columns "carat,depth,clarity,price,x,__index_level_0__" test1.parquet test2.parquet
File: test1.parquet
Rows: 10, Columns: 11

"carat","depth","clarity","price","x","__index_level_0__"
0.230000,61.500000,SI2,326,3.950000,0
0.210000,59.800000,SI1,326,3.890000,1
0.230000,56.900000,VS1,327,4.050000,2

================================================================================

File: test2.parquet
Rows: 10, Columns: 10

Warning: Column '__index_level_0__' not found in parquet file
"carat","depth","clarity","price","x"
0.220000,65.100000,VS2,337,3.870000
0.230000,56.900000,VS1,327,4.050000
0.310000,63.300000,SI2,335,4.340000
```
