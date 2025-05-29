# PalValidator

## How to build executables

```
mkdir build
cd build
cmake ..
```

## How to build & view the doc

```
cd build
make doc
open ./html/index.html
```

`open` will use your default web browser.

## How to build & run tests

```
cd build
make tests
```

To run a specific test, first check the test name:
```
./libs/{LIB}/{LIB}_unit_tests --list-tests
```
Then simply run
```
./libs/{LIB}/{LIB}_unit_tests {TEST_NAME}
```

## How to run PalValidator

```
cd build
./PalValidator config.cfg
```
