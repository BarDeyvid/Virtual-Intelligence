import subprocess
import os
import glob
import re
import xml.etree.ElementTree as ET
from pathlib import Path

# Configuration
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DOCS_DIR = os.path.join(BASE_DIR, "docs")
XML_DIR = os.path.join(DOCS_DIR, "xml")
API_DIR = os.path.join(DOCS_DIR, "api")
DOXYFILE = "Doxyfile"

def ensure_directories():
    """Create necessary directories if they don't exist."""
    os.makedirs(XML_DIR, exist_ok=True)
    os.makedirs(API_DIR, exist_ok=True)

def build_and_split_docs():
    # Change working directory to /docs so Doxyfile relative paths work
    original_dir = os.getcwd()
    os.chdir(DOCS_DIR)

    try:
        # 1. Run Doxygen
        print("Step 1: Running Doxygen...")
        subprocess.run(["doxygen", DOXYFILE], check=True)

        # Check if XML files were generated
        xml_files = glob.glob(os.path.join(XML_DIR, "*.xml"))
        if not xml_files:
            print(f"Warning: No XML files found in {XML_DIR}")
            print("Check your Doxyfile INPUT settings and OUTPUT_DIRECTORY.")
            return

        print(f"Generated {len(xml_files)} XML files.")

        # 2. Try Moxygen first
        print("\nStep 2: Trying Moxygen to split documentation...")
        
        # Clear existing API files
        existing_md = glob.glob(os.path.join(API_DIR, "*.md"))
        for f in existing_md:
            os.remove(f)
        
        # Try different Moxygen patterns
        moxygen_patterns = [
            ("%s.md", "Default pattern"),
            ("%c.md", "Class pattern"),
            ("%n.md", "Namespace pattern"),
            ("api_%s.md", "API prefix pattern"),
        ]
        
        moxygen_success = False
        for pattern, description in moxygen_patterns:
            print(f"  Trying pattern: {pattern} ({description})")
            output_path = os.path.join(API_DIR, pattern)
            moxygen_cmd = [
                "moxygen",
                "--anchors",
                "--output", output_path,
                XML_DIR
            ]
            
            try:
                result = subprocess.run(moxygen_cmd, capture_output=True, text=True, timeout=30)
                if result.returncode == 0:
                    # Check if we got multiple files
                    md_files = glob.glob(os.path.join(API_DIR, "*.md"))
                    # Filter out any file that contains literal % character
                    actual_md_files = [f for f in md_files if '%' not in os.path.basename(f)]
                    
                    if len(actual_md_files) > 1:
                        print(f"    ✓ Success! Generated {len(actual_md_files)} files with pattern '{pattern}'")
                        moxygen_success = True
                        break
                    else:
                        print(f"    ✗ Only generated {len(actual_md_files)} valid file(s)")
                        # Clean up for next attempt
                        for f in md_files:
                            if os.path.exists(f):
                                os.remove(f)
                else:
                    print(f"    ✗ Command failed: {result.stderr[:100]}...")
            except subprocess.TimeoutExpired:
                print("    ✗ Timeout")
            except Exception as e:
                print(f"    ✗ Error: {e}")
        
        # 3. If Moxygen failed, use manual XML parsing
        if not moxygen_success:
            print("\nMoxygen failed to split properly. Using manual XML parsing...")
            manual_split_from_xml()
        else:
            # Verify the results
            md_files = glob.glob(os.path.join(API_DIR, "*.md"))
            valid_md_files = [f for f in md_files if '%' not in os.path.basename(f)]
            
            if valid_md_files:
                print(f"\nSuccessfully generated {len(valid_md_files)} markdown files:")
                for md_file in sorted(valid_md_files)[:10]:  # Show first 10
                    filename = os.path.basename(md_file)
                    with open(md_file, 'r', encoding='utf-8') as f:
                        lines = f.readlines()
                    print(f"  - {filename} ({len(lines)} lines)")
                if len(valid_md_files) > 10:
                    print(f"  ... and {len(valid_md_files) - 10} more files")
            else:
                print("\nNo valid markdown files generated. Falling back to manual parsing...")
                manual_split_from_xml()
        
        # 4. Create index file
        create_index_file()
        
        # 5. Commit the split results in chunks
        commit_split_results_in_chunks()

    except subprocess.CalledProcessError as e:
        print(f"Error during subprocess execution: {e}")
        print(f"Command output: {e.output}")
    except Exception as e:
        print(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        os.chdir(original_dir)

def manual_split_from_xml():
    """Manually parse XML files and create Markdown documentation."""
    print("\nManual XML parsing started...")
    
    # Get all XML files
    xml_files = glob.glob(os.path.join(XML_DIR, "*.xml"))
    print(f"Found {len(xml_files)} XML files to process")
    
    # Parse index.xml first to understand structure
    index_file = os.path.join(XML_DIR, "index.xml")
    if not os.path.exists(index_file):
        print("Error: index.xml not found!")
        return
    
    try:
        tree = ET.parse(index_file)
        root = tree.getroot()
        
        # Count compounds by type
        compounds_by_type = {}
        for compound in root.findall('.//compound'):
            kind = compound.get('kind', 'unknown')
            compounds_by_type[kind] = compounds_by_type.get(kind, 0) + 1
        
        print(f"Compounds by type: {compounds_by_type}")
        
        # Process namespace and class files
        processed_count = 0
        for xml_file in xml_files:
            filename = os.path.basename(xml_file)
            
            # Only process compound files (namespaces, classes, structs)
            if (filename.startswith('namespace') or 
                filename.startswith('class') or 
                filename.startswith('struct')):
                
                try:
                    process_compound_xml(xml_file)
                    processed_count += 1
                except Exception as e:
                    print(f"  Error processing {filename}: {e}")
        
        print(f"Manually processed {processed_count} compound files")
        
    except Exception as e:
        print(f"Error parsing XML: {e}")
        import traceback
        traceback.print_exc()

def process_compound_xml(xml_file):
    """Process a single compound XML file and generate Markdown."""
    try:
        tree = ET.parse(xml_file)
        root = tree.getroot()
        
        # Find the compounddef element
        compounddef = root.find('.//compounddef')
        if compounddef is None:
            return
        
        # Get compound information
        kind = compounddef.get('kind', '')
        name = get_text(compounddef.find('compoundname'))
        if not name:
            return
        
        # Create filename based on kind and name
        if kind == 'namespace':
            # Remove :: from namespace names
            safe_name = name.replace('::', '_').replace(' ', '_')
            filename = f"namespace_{safe_name}.md"
        elif kind in ['class', 'struct']:
            # Extract just the class name (remove namespace)
            class_name = name.split('::')[-1] if '::' in name else name
            safe_name = class_name.replace(' ', '_')
            filename = f"{kind}_{safe_name}.md"
        else:
            safe_name = name.replace('::', '_').replace(' ', '_')
            filename = f"{kind}_{safe_name}.md"
        
        output_path = os.path.join(API_DIR, filename)
        
        # Get documentation
        brief = get_text(compounddef.find('briefdescription'))
        detailed = get_text(compounddef.find('detaileddescription'))
        
        # Get member functions and variables
        members = []
        for sectiondef in compounddef.findall('.//sectiondef'):
            kind_attr = sectiondef.get('kind', '')
            for memberdef in sectiondef.findall('.//memberdef'):
                member_kind = memberdef.get('kind', '')
                member_name = get_text(memberdef.find('name'))
                member_type = get_text(memberdef.find('type'))
                member_brief = get_text(memberdef.find('briefdescription'))
                
                if member_name:
                    members.append({
                        'kind': member_kind,
                        'name': member_name,
                        'type': member_type,
                        'brief': member_brief
                    })
        
        # Write Markdown file
        with open(output_path, 'w', encoding='utf-8') as f:
            # Header
            if kind == 'namespace':
                f.write(f"# namespace `{name}`\n\n")
            else:
                f.write(f"# {kind} `{name}`\n\n")
            
            # Brief description
            if brief:
                f.write(f"{brief}\n\n")
            
            # Detailed description
            if detailed:
                f.write(f"## Detailed Description\n\n{detailed}\n\n")
            
            # Members table
            if members:
                f.write("## Summary\n\n")
                f.write("| Members | Descriptions |\n")
                f.write("|---------|--------------|\n")
                
                for member in members:
                    member_display = f"`{member['kind']} `[`{member['name']}`](#)"
                    if member['type']:
                        member_display = f"{member['type']} {member_display}"
                    
                    brief_desc = member['brief'] or ""
                    f.write(f"| {member_display} | {brief_desc} |\n")
                f.write("\n")
            
            # Member details
            if members:
                f.write("## Members\n\n")
                for i, member in enumerate(members, 1):
                    f.write(f"### `{member['name']}`\n\n")
                    if member['type']:
                        f.write(f"**Type**: {member['type']}\n\n")
                    if member['brief']:
                        f.write(f"{member['brief']}\n\n")
                    f.write("---\n\n")
        
        return filename
        
    except Exception as e:
        print(f"Error processing {xml_file}: {e}")
        return None

def get_text(element):
    """Extract text from XML element, handling None case."""
    if element is None:
        return ""
    
    # Get all text content
    text_parts = []
    if element.text:
        text_parts.append(element.text.strip())
    
    # Also check for para elements within
    for para in element.findall('.//para'):
        if para.text:
            text_parts.append(para.text.strip())
    
    return " ".join(text_parts) if text_parts else ""

def create_index_file():
    """Create an index file listing all generated documentation files."""
    index_path = os.path.join(API_DIR, "index.md")
    md_files = glob.glob(os.path.join(API_DIR, "*.md"))
    
    # Filter out index.md itself
    md_files = [f for f in md_files if not f.endswith('index.md')]
    
    if not md_files:
        print("No markdown files to index.")
        return
    
    with open(index_path, 'w', encoding='utf-8') as f:
        f.write("# API Documentation Index\n\n")
        f.write(f"Generated on: {subprocess.check_output(['date']).decode().strip()}\n\n")
        f.write(f"Total files: {len(md_files)}\n\n")
        
        # Group files by type
        namespace_files = []
        class_files = []
        struct_files = []
        other_files = []
        
        for md_file in sorted(md_files):
            filename = os.path.basename(md_file)
            
            if filename.startswith('namespace_'):
                namespace_files.append(filename)
            elif filename.startswith('class_'):
                class_files.append(filename)
            elif filename.startswith('struct_'):
                struct_files.append(filename)
            else:
                other_files.append(filename)
        
        # Write sections
        if namespace_files:
            f.write("## Namespaces\n\n")
            for nf in sorted(namespace_files):
                display_name = nf.replace("namespace_", "").replace(".md", "").replace("_", "::")
                f.write(f"- [{display_name}]({nf})\n")
            f.write("\n")
        
        if class_files:
            f.write("## Classes\n\n")
            for cf in sorted(class_files):
                display_name = cf.replace("class_", "").replace(".md", "")
                f.write(f"- [{display_name}]({cf})\n")
            f.write("\n")
        
        if struct_files:
            f.write("## Structures\n\n")
            for sf in sorted(struct_files):
                display_name = sf.replace("struct_", "").replace(".md", "")
                f.write(f"- [{display_name}]({sf})\n")
            f.write("\n")
        
        if other_files:
            f.write("## Other Documentation\n\n")
            for of in sorted(other_files):
                f.write(f"- [{of}]({of})\n")
        
        # Add quick stats
        f.write("\n---\n")
        f.write(f"**Statistics**: {len(namespace_files)} namespaces, {len(class_files)} classes, {len(struct_files)} structures\n")

def commit_split_results_in_chunks(chunk_size=10):
    """Adiciona os arquivos ao index do git. O commit e push serão feitos pela Action."""
    print("\nStep 5: Prepping files for Git...")
    
    md_files = glob.glob(os.path.join(API_DIR, "*.md"))
    if not md_files:
        print("No markdown files found to add.")
        return
    
    try:
        for file_path in md_files:
            subprocess.run(["git", "add", file_path], check=True)
        print(f"✓ {len(md_files)} files added to git index.")
    except Exception as e:
        print(f"✗ Error adding files to git: {e}")

if __name__ == "__main__":
    ensure_directories()
    build_and_split_docs()
    print("\nDone! Files are ready for commit.")