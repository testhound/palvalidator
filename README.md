# PalValidator

## Prerequisites

Before building the project, you need to initialize and update the git submodules:

```bash
git submodule update --init --recursive
```

This will checkout the Alglib library located in `libraries/Alglib` which is required for compilation.

## For Maintainers: Adding/Updating Submodules

When adding a new submodule (like the Alglib library), you need to commit both the `.gitmodules` file and the submodule directory:

```bash
git add .gitmodules
git add libraries/Alglib
git commit -m "Add Alglib submodule"
git push
```

To update an existing submodule to a newer commit:
```bash
cd libraries/Alglib
git pull origin main  # or the appropriate branch
cd ../..
git add libraries/Alglib
git commit -m "Update Alglib submodule"
git push
```

## How to build executables

For production, use:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
```

For debug & profiling puposes, use the following instead:
```
mkdir build-debug
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
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
