import os
import re
import argparse

# A list of common Python standard library modules to exclude from requirements.txt
# This is not exhaustive, but covers the most frequently imported core modules.
STANDARD_LIBRARY_MODULES = {
    "os", "sys", "re", "time", "json", "math", "random", "logging", 
    "datetime", "collections", "functools", "itertools", "io", "subprocess",
    "threading", "multiprocessing", "argparse", "pathlib", "csv", "tempfile",
    "llama_cpp"
}

def extract_package_name(line):
    """
    Extracts the top-level package name from an import statement.
    e.g., 'from numpy.linalg import svd' -> 'numpy'
    e.g., 'import requests as r' -> 'requests'
    """
    # Regex for 'import package' or 'import package.submodule'
    match_import = re.match(r'^\s*import\s+([\w\d._]+)', line)
    if match_import:
        full_name = match_import.group(1)
        return full_name.split('.')[0]

    # Regex for 'from package import module' or 'from package.submodule import ...'
    match_from = re.match(r'^\s*from\s+([\w\d._]+)', line)
    if match_from:
        full_name = match_from.group(1)
        return full_name.split('.')[0]
        
    return None

def find_imports(start_directory):
    """
    Recursively scans all .py files in the start_directory and extracts unique external packages.
    """
    external_packages = set()
    print(f"Scanning directory: {os.path.abspath(start_directory)}")
    
    # os.walk generates the file names in a directory tree
    for root, _, files in os.walk(start_directory):
        for file in files:
            if file.endswith('.py'):
                filepath = os.path.join(root, file)
                
                # Skip the script itself if it is in the scan path
                if filepath == os.path.abspath(__file__):
                    continue

                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        for line in f:
                            # Skip comments and empty lines
                            if line.strip().startswith('#') or not line.strip():
                                continue

                            package = extract_package_name(line)
                            
                            if package and package not in STANDARD_LIBRARY_MODULES:
                                # We treat local imports (starting with '.') as internal to the project
                                if not line.strip().startswith('from .') and not line.strip().startswith('import .'):
                                    external_packages.add(package)

                except UnicodeDecodeError:
                    print(f"Skipping file due to encoding error: {filepath}")
                except Exception as e:
                    print(f"An error occurred while reading {filepath}: {e}")

    return sorted(list(external_packages))

def main():
    """
    Main function to parse arguments and generate the requirements file.
    """
    parser = argparse.ArgumentParser(
        description="Generates a requirements.txt file by scanning .py files for imports."
    )
    parser.add_argument(
        'directory', 
        nargs='?', 
        default='.', 
        help="The directory to scan (default: current directory)."
    )
    parser.add_argument(
        '--output',
        default='requirements.txt',
        help="The name of the output file (default: requirements.txt)."
    )
    args = parser.parse_args()

    packages = find_imports(args.directory)

    if not packages:
        print("No external packages found.")
        return

    try:
        with open(args.output, 'w') as f:
            for package in packages:
                f.write(f"{package}\n")
        
        print("-" * 50)
        print(f"Successfully generated {len(packages)} requirements in '{args.output}':")
        for package in packages:
            print(f"- {package}")
        print("-" * 50)

    except IOError:
        print(f"Error: Could not write to file {args.output}.")

if __name__ == "__main__":
    main()
