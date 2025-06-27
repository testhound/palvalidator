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

### Branch Switching with Submodules

When switching between branches that have different submodule configurations, you may see warnings like:
```
warning: unable to rmdir 'libraries/Alglib': Directory not empty
```

This happens when switching to a branch without the submodule while untracked files exist in the submodule directory. To handle this:

1. **After switching branches, clean up leftover submodule directories:**
   ```bash
   git checkout master  # or any branch without the submodule
   # If you see the warning, manually clean up:
   rm -rf libraries/Alglib  # Remove the leftover directory
   ```

2. **When switching back to a branch with submodules:**
   ```bash
   git checkout your-feature-branch
   git submodule update --init --recursive  # Reinitialize submodules
   ```

3. **To avoid this issue in the future, use git's submodule-aware checkout:**
   ```bash
   git checkout --recurse-submodules your-branch-name
   ```

### Working with Upstream Changes

**Switching to master branch (without submodules):**
If your local master branch doesn't have submodules yet, you can switch normally:
```bash
git checkout master
# If you see the "Directory not empty" warning, clean up:
rm -rf libraries/Alglib
```

**Updating master branch with upstream submodule changes:**
If the upstream master branch now has submodule changes merged in:
```bash
git checkout master
git fetch origin
git rebase origin/master  # or git merge origin/master
git submodule update --init --recursive  # Initialize new submodules
```

**Alternative using pull:**
```bash
git checkout master
git pull origin master
git submodule update --init --recursive
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
