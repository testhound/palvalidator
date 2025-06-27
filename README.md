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

### Troubleshooting Submodule Issues

If you see "untracked content" or "modified" status for a submodule after adding it, this usually means the submodule directory has untracked files or isn't properly initialized. To fix this:

1. **Check the submodule status:**
   ```bash
   git submodule status
   ```

2. **If the submodule shows as uninitialized (no commit hash), initialize it:**
   ```bash
   git submodule update --init libraries/Alglib
   ```

3. **If there are untracked files in the submodule, investigate and clean them:**
   ```bash
   cd libraries/Alglib
   git status  # Check what's untracked
   ls -la      # List all files including hidden ones
   ```
   
   Common causes of untracked content:
   - Build artifacts (`.o`, `.so`, `.dll`, `.exe` files)
   - IDE files (`.vscode/`, `.idea/`, etc.)
   - Temporary files
   
   To clean untracked files:
   ```bash
   git clean -fd  # Remove untracked files and directories (be careful!)
   # Or more safely, remove specific files:
   # rm -rf build/  # if there's a build directory
   # rm *.o         # if there are object files
   cd ../..
   ```
   
   **Specific case for Alglib:** If you see `libalglib/alglibversion.h` as untracked, this is a generated build artifact (created from `alglibversion.template`) that's needed for builds but shouldn't be tracked. The best approach is to ignore it:
   ```bash
   git config submodule.libraries/Alglib.ignore untracked
   ```
   
   This tells git to ignore untracked files in the Alglib submodule, allowing the generated `alglibversion.h` file to remain for builds while not causing git status issues.
   
   Note: The `alglibversion.h` file will be regenerated during the build process from the `alglibversion.template` file that's properly tracked in the repository.

4. **Alternative: Ignore untracked content (if the files should remain):**
   If the untracked files are meant to be there (like build outputs), you can configure git to ignore them:
   ```bash
   git config submodule.libraries/Alglib.ignore untracked
   ```

5. **Then add and commit the submodule reference:**
   ```bash
   git add libraries/Alglib
   git commit -m "Add Alglib submodule"
   ```

### Updating an existing submodule to a newer commit:
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
