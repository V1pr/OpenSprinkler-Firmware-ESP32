# Bundle the pinned web UI and compress all HTML assets before building firmware.
import subprocess
import os
import sys
import shutil

# Import the PlatformIO environment
Import("env")

print("--- Fetching and bundling pinned OpenSprinkler UI ---")

rebuild_app = False
definitions = env.get("CPPDEFINES", [])
if isinstance(definitions, str):
    definitions = [definitions]

for definition in definitions:
    if isinstance(definition, str):
        name, separator, value = definition.partition("=")
    elif isinstance(definition, (tuple, list)) and definition:
        name = str(definition[0])
        separator = "=" if len(definition) > 1 else ""
        value = str(definition[1]) if len(definition) > 1 else ""
    else:
        continue

    if name == "REBUILD_APP":
        rebuild_app = not separator or value.lower() not in ("0", "false", "no", "")
        break

if rebuild_app:
    print("REBUILD_APP is enabled; fetching the latest UI commit from GitHub.")

bundle_command = [sys.executable, os.path.join(".", "tools", "fetch_ui_bundle.py")]
if rebuild_app:
    bundle_command.append("--rebuild")

try:
    result = subprocess.run(
        bundle_command,
        check=True,
        capture_output=True,
        text=True,
    )

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print("Error output from UI bundler:")
        print(result.stderr)

    print("--- OpenSprinkler UI bundle generated successfully ---")

except FileNotFoundError:
    print("Error: PlatformIO's Python interpreter could not be started.")
    env.Exit(1)

except subprocess.CalledProcessError as e:
    print("Error executing the OpenSprinkler UI bundler.")
    print(f"Return code: {e.returncode}")
    print("Output (stdout):")
    print(e.stdout)
    print("Error output (stderr):")
    print(e.stderr)
    print("--- Build HALTED due to UI bundle failure ---")
    env.Exit(1)

print("--- Running pre-build HTML minifier script ---")

# Define the command to execute. We assume 'node' is in the system's PATH.
# The script is located in the 'tools' directory.
command = ["node", os.path.join(".", "compress_htmls.mjs")]

try:
    # We run the command from the project directory, which is the default
    # working directory for scripts in PlatformIO.
    result = subprocess.run(command, check=True, capture_output=True, text=True)

    # Print the output from the Node.js script
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print("Error output from script:")
        print(result.stderr)

    print("--- HTML minifier script finished successfully ---")

except FileNotFoundError:
    print("Error: 'node' command not found.")
    print("Please ensure Node.js is installed and in your system's PATH.")
    # Exit with an error code to stop the build
    env.Exit(1)

except subprocess.CalledProcessError as e:
    print("Error executing the HTML minifier script.")
    print(f"Return code: {e.returncode}")
    print("Output (stdout):")
    print(e.stdout)
    print("Error output (stderr):")
    print(e.stderr)
    print("--- Build HALTED due to script failure ---")
    env.Exit(1)

except Exception as e:
    print(f"An unexpected error occurred: {e}")
    env.Exit(1)


def create_esp32_n8_release_artifact(source, target, env):
    source_path = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    release_path = env.subst("$BUILD_DIR/${PROGNAME}.bin32n8")
    shutil.copy2(source_path, release_path)
    print(f"Created ESP32-C6-N8 release artifact: {release_path}")


# PlatformIO expects firmware.bin for its upload pipeline. Keep that internal
# artifact and also emit the architecture-specific file used for releases.
if env.get("PIOENV") == "os4_esp32c6_n8":
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", create_esp32_n8_release_artifact)
