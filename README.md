# parq
A generic parquet, avro and ORC file reader

```
$ home/parq --help
Usage: /projects/parq/home/bin/parq [options] <file> [file2] [file3] ...
Options:
  -p, --parquet            Input files are Parquet (default)
  -a, --avro               Input files are Avro
  -o, --orc                Input files are ORC
  -t, --tabular            Output in tabular format (default)
  -j, --json               Output in JSON format
  -c, --csv                Output in CSV format
  -x, --xml                Output in XML format
  -l, --limit <n>          Limit output to first n rows
  -m, --metadata           Show only column metadata information
  -C, --columns <list>     Show only specified columns (comma-delimited)
      --case <upper|lower> Set output case (default: lower)
  -h, --help               Show this help message

Note: Output format options are mutually exclusive.
      Input format options are mutually exclusive.
Multiple files can be processed in sequence.
```

## Building

```
./configure            # release build into home/bin
./configure debug      # debug build
./configure clean      # remove build/ and the generated home/bin, home/lib
```

Parquet is always available. Avro and ORC are optional: `./configure` probes for
each and, when it cannot find one, still builds a working binary whose `-a` or
`-o` reports that this build has no support for that format. The probe result is
printed during configure, so it is never a silent downgrade.

### Avro

Needs `avro-cpp` (`brew install avro-cpp`, `dnf install avro-cpp-devel`, or the
source build the ide images do). Turn it off explicitly with `-DWITH_AVRO=OFF`.

### ORC

ORC is unusual: there is no separate ORC package to install. The adapter lives
*inside* libarrow, and only when arrow itself was built with `ARROW_ORC=ON`. An
arrow either carries it or it does not, and nothing a consumer does afterwards
can add it -- so the only fix for a missing adapter is a different arrow.

`./configure` reads `ARROW_ORC` out of the `ArrowOptions.cmake` that arrow
installs alongside its cmake config, which is arrow's own record of how it was
configured.

| Platform                     | ORC available | How |
| ---------------------------- | ------------- | --- |
| ide7 / ide8 / ide9 images     | yes           | arrow is built from source with `-DARROW_ORC=ON`, and the image build fails if the adapter header is missing |
| macOS, homebrew apache-arrow  | no            | the homebrew formula sets `-DARROW_ORC=OFF` |
| macOS, local tap ( below )    | yes           | the same formula with ORC turned on |

#### ORC on macOS

Homebrew's `apache-arrow` sets `ARROW_ORC=OFF`, so `-o/--orc` cannot work against
it. Building arrow by hand instead is a trap on this platform: AppleClang searches
`/usr/local/include` *before* the SDK, so a source build picks up whatever headers
homebrew has installed ahead of arrow's own vendored copies -- flatbuffers and
abseil in particular -- and fails in places that have nothing to do with ORC.

The route that works is to let homebrew keep owning the dependency graph, and
change only the one flag. Copy the real formula into a local tap:

```
brew tap-new mti/local
cp "$( find ~/Library/Caches/Homebrew/api-source -path '*Formula/a/apache-arrow.rb' | head -1 )" \
   "$( brew --repository mti/local )/Formula/apache-arrow-orc.rb"
```

Then edit that copy:

- rename the class to `ApacheArrowOrc` so it installs beside the real keg
- delete the `bottle do ... end` block ( no bottles exist under this name )
- `-DARROW_ORC=OFF` becomes `-DARROW_ORC=ON`
- add `-DORC_SOURCE=BUNDLED` -- the formula builds with
  `ARROW_DEPENDENCY_SOURCE=SYSTEM`, and there is no `apache-orc` formula to
  satisfy it, so ORC itself has to be fetched
- add `-DHOMEBREW_ALLOW_FETCHCONTENT=ON` -- homebrew traps `FetchContent` in
  sandboxed builds, and the line above needs it

```
brew install --build-from-source mti/local/apache-arrow-orc
```

That takes about six minutes, because every other dependency comes from kegs
homebrew already built and keeps consistent with each other.

Finally, make it the arrow the machine actually uses. Nothing else needs to
depend on `apache-arrow` -- check with `brew uses --installed apache-arrow`:

```
brew uninstall apache-arrow
brew link --force apache-arrow-orc
```

Installing is not the same as linking. Homebrew puts every formula in its own
keg under `Cellar/` and then symlinks it into `/usr/local`; until that second
step happens nothing can find it, and `./configure` will keep resolving the old
arrow and report ORC as unavailable. The `library :` line in the configure
summary is the one to check -- it prints which libarrow was actually resolved.

Alternatively keep both and leave the new one keg-only, selecting it per build:

```
export CMAKE_PREFIX_PATH=/usr/local/opt/apache-arrow-orc
```

Either way, `-DWITH_ORC=OFF` builds quietly without ORC. Parquet and Avro are
unaffected.

Reading ORC timestamps also needs the IANA timezone database (`/usr/share/zoneinfo`)
at run time -- ORC files record the writer's timezone and the reader resolves it.
It is present in the ide images and on macOS; a stripped-down container may need
`tzdata` installed.

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
### Metadata
The file information and metadata may be found by using the --metadata option.
```
$ home/parq --metadata test1.parquet test2.parquet
File: test1.parquet
Rows: 10, Columns: 11

File Information:

  Property        | Value                                                              |
  ----------------+--------------------------------------------------------------------+
  Row Groups      | 1                                                                  |
  Created By      | parquet-cpp version 1.3.2-SNAPSHOT                                 |
  File Size       | 2.16 KB                                                            |
  File Checksum   | be6773848ce905b99192adc68f0c3b2aabab7d214db50b92a52203790566ab2b   |

Column Information:

  Index | Column Name            | Data Type         | Compression  |
  ------+------------------------+-------------------+--------------+
      0 | carat                  | DOUBLE            | snappy       |
      1 | cut                    | STRING            | none         |
      2 | color                  | STRING            | none         |
      3 | clarity                | STRING            | none         |
      4 | depth                  | DOUBLE            | none         |
      5 | table                  | DOUBLE            | none         |
      6 | price                  | INT64             | none         |
      7 | x                      | DOUBLE            | none         |
      8 | y                      | DOUBLE            | none         |
      9 | z                      | DOUBLE            | none         |
     10 | __index_level_0__      | INT64             | none         |

================================================================================

File: test2.parquet
Rows: 10, Columns: 10

File Information:

  Property        | Value                                                              |
  ----------------+--------------------------------------------------------------------+
  Row Groups      | 1                                                                  |
  Created By      | parquet-cpp version 1.3.2-SNAPSHOT                                 |
  File Size       | 1.95 KB                                                            |
  File Checksum   | 60f1945edc3e4ec38f6e234389e647a1b369de8afb9c7840c491a39880c0caa1   |

Column Information:

  Index | Column Name            | Data Type         | Compression  |
  ------+------------------------+-------------------+--------------+
      0 | carat                  | DOUBLE            | snappy       |
      1 | depth                  | DOUBLE            | none         |
      2 | table                  | DOUBLE            | none         |
      3 | price                  | INT64             | none         |
      4 | x                      | DOUBLE            | none         |
      5 | y                      | DOUBLE            | none         |
      6 | z                      | DOUBLE            | none         |
      7 | cut                    | STRING            | none         |
      8 | color                  | STRING            | none         |
      9 | clarity                | STRING            | none         |
```

## Copyright

Copyright (c) 2004-2025 Metasystems Technologies Inc. (MTI)
All rights reserved

Distributed under the MTI Software License, Version 0.1.

as defined by accompanying file MTI-LICENSE-0.1.info or
at http://www.mtihq.com/license/MTI-LICENSE-0.1.info

You are welcome to use this application and source free of charge. The Licensing is
Open-Source, like the MIT Software License in it's scope.
