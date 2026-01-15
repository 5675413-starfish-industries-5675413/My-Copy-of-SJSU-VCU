#!/usr/bin/env python3
"""
Script to extract all struct members from C header and source files
in the dev/ and inc/ directories.
Uses pycparser for parsing C code.
"""

import os
import json
from pathlib import Path

try:
    from pycparser import parse_file, c_ast
    import pycparser
except ImportError:
    print("Error: pycparser is not installed.")
    print("Install it with: pip install pycparser")
    exit(1)

def strip_comments_and_directives(content):
    """Manually strip C comments and handle basic preprocessor directives."""
    result = []
    i = 0
    in_string = False
    string_char = None
    
    while i < len(content):
        # Handle string literals
        if content[i] in ('"', "'") and (i == 0 or content[i-1] != '\\'):
            if not in_string:
                in_string = True
                string_char = content[i]
                result.append(content[i])
            elif content[i] == string_char:
                in_string = False
                string_char = None
                result.append(content[i])
            else:
                result.append(content[i])
            i += 1
            continue
        
        if in_string:
            result.append(content[i])
            i += 1
            continue
        
        # Check for single-line comment
        if i < len(content) - 1 and content[i:i+2] == '//':
            # Skip to end of line
            while i < len(content) and content[i] != '\n':
                i += 1
            if i < len(content):
                result.append('\n')
                i += 1
        # Check for multi-line comment
        elif i < len(content) - 1 and content[i:i+2] == '/*':
            # Skip to end of comment
            i += 2
            while i < len(content) - 1:
                if content[i:i+2] == '*/':
                    i += 2
                    break
                i += 1
        # Check for preprocessor directive (skip all of them)
        elif content[i] == '#':
            # Skip to end of line
            while i < len(content) and content[i] != '\n':
                i += 1
            if i < len(content):
                result.append('\n')
                i += 1
        else:
            result.append(content[i])
            i += 1
    
    return ''.join(result)

def find_struct_definitions(filepath):
    """Find all struct definitions in a file using pycparser."""
    structs = []
    
    try:
        # Read file content
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        # Try with cpp preprocessor first
        fake_libc_path = os.path.join(os.path.dirname(pycparser.__file__), 'utils', 'fake_libc_include')
        cpp_paths = ['cpp', 'gcc', 'clang']
        cpp_found = False
        
        for cpp in cpp_paths:
            try:
                ast = parse_file(
                    filepath,
                    use_cpp=True,
                    cpp_path=cpp,
                    cpp_args=['-E', '-I' + fake_libc_path] if os.path.exists(fake_libc_path) else ['-E']
                )
                cpp_found = True
                break
            except:
                continue
        
        # If cpp not found, manually strip comments and try parsing
        if not cpp_found:
            # Strip comments and directives manually
            cleaned_content = strip_comments_and_directives(content)
            
            # Add type definitions that pycparser needs to understand
            type_defs = """
typedef unsigned char ubyte1;
typedef unsigned int ubyte2;
typedef unsigned long ubyte4;
typedef signed char sbyte1;
typedef signed int sbyte2;
typedef signed long sbyte4;
typedef float float4;
typedef unsigned char bool;
typedef void* SerialManager;
typedef void* Sensor;
typedef void* MotorController;
typedef void* BatteryManagementSystem;
typedef void* InstrumentCluster;
typedef void* WheelSpeeds;
typedef void* SafetyChecker;
typedef void* LaunchControl;
typedef void* PowerLimit;
typedef void* DRS;
typedef void* Regen;
typedef void* Efficiency;
typedef void* TorqueEncoder;
typedef void* BrakePressureSensor;
typedef void* ReadyToDriveSound;
typedef void* PID;
typedef void* CoolingSystem;
typedef void* CanManager;
typedef void* CanMessageNode;
typedef void* AVLNode;
typedef void* HashEntry;
typedef void* HashTable;
typedef void* WatchDog;
typedef int IO_ErrorType;
typedef int Status;
typedef int Direction;
typedef int Event;
typedef int LC_State;
typedef int LC_Mode;
typedef int LC_Phase;
typedef int RegenMode;
typedef int PLMode;
typedef int Wheel;
typedef int Light;
typedef int CanChannel;
typedef int DIAG_ERRORCODE;
struct _io_can_data_frame {
    ubyte1 data[8];
    ubyte1 length;
    ubyte1 id_format;
    ubyte4 id;
};
typedef struct _io_can_data_frame IO_CAN_DATA_FRAME;
"""
            cleaned_content = type_defs + cleaned_content
            
            # Parse the cleaned content
            from pycparser.c_parser import CParser
            parser = CParser()
            ast = parser.parse(cleaned_content, filename=filepath)
        
        # Visit the AST to find struct definitions
        visitor = StructVisitor(filepath)
        visitor.visit(ast)
        structs = visitor.structs
        
    except Exception as e:
        # If parsing fails, try to continue with other files
        print(f"Warning: Could not parse {filepath}: {e}")
    
    return structs

class StructVisitor(c_ast.NodeVisitor):
    """Visitor to extract struct definitions from AST."""
    
    def __init__(self, filepath):
        self.filepath = filepath
        self.structs = []
    
    def visit_Struct(self, node):
        """Visit a struct definition."""
        if node.name:
            struct_tag = node.name
            struct_name = struct_tag.lstrip('_') if struct_tag.startswith('_') else struct_tag
            
            members = []
            if node.decls:
                for decl in node.decls:
                    member = self.extract_member(decl)
                    if member:
                        members.append(member)
            
            self.structs.append({
                'tag': struct_tag,
                'name': struct_name,
                'members': members,
                'file': self.filepath,
                'type': 'struct'
            })
        
        # Continue visiting children
        self.generic_visit(node)
    
    def visit_Typedef(self, node):
        """Visit a typedef definition."""
        if isinstance(node.type, c_ast.Struct):
            struct_node = node.type
            if struct_node.name:
                struct_tag = struct_node.name
                typedef_name = node.name
                
                members = []
                if struct_node.decls:
                    for decl in struct_node.decls:
                        member = self.extract_member(decl)
                        if member:
                            members.append(member)
                
                self.structs.append({
                    'tag': struct_tag,
                    'name': typedef_name,
                    'members': members,
                    'file': self.filepath,
                    'type': 'typedef'
                })
        
        # Continue visiting children
        self.generic_visit(node)
    
    def extract_member(self, decl):
        """Extract member information from a declaration node."""
        if not decl.name:
            return None
        
        member_name = decl.name
        
        # Get the type string
        type_str = self.get_type_string(decl.type)
        
        # Handle arrays
        if isinstance(decl.type, c_ast.ArrayDecl):
            array_size = self.get_array_size(decl.type)
            if array_size:
                member_name += '[' + array_size + ']'
        
        return {
            'type': type_str,
            'name': member_name
        }
    
    def get_type_string(self, type_node):
        """Convert a type node to a string representation."""
        if isinstance(type_node, c_ast.TypeDecl):
            return self.get_type_string(type_node.type)
        elif isinstance(type_node, c_ast.IdentifierType):
            return ' '.join(type_node.names)
        elif isinstance(type_node, c_ast.PtrDecl):
            base_type = self.get_type_string(type_node.type)
            return base_type + '*'
        elif isinstance(type_node, c_ast.ArrayDecl):
            base_type = self.get_type_string(type_node.type)
            return base_type
        elif isinstance(type_node, c_ast.Struct):
            return 'struct ' + (type_node.name or '')
        elif isinstance(type_node, c_ast.Enum):
            return 'enum ' + (type_node.name or '')
        else:
            return str(type(type_node).__name__)
    
    def get_array_size(self, array_node):
        """Get the array size as a string."""
        if isinstance(array_node, c_ast.ArrayDecl):
            if array_node.dim:
                if isinstance(array_node.dim, c_ast.Constant):
                    return array_node.dim.value
                elif isinstance(array_node.dim, c_ast.ID):
                    return array_node.dim.name
                elif isinstance(array_node.dim, c_ast.BinaryOp):
                    # Handle expressions like 0x7FF
                    return self.expression_to_string(array_node.dim)
            return None
        return None
    
    def expression_to_string(self, expr):
        """Convert an expression node to a string."""
        if isinstance(expr, c_ast.Constant):
            return expr.value
        elif isinstance(expr, c_ast.ID):
            return expr.name
        elif isinstance(expr, c_ast.BinaryOp):
            left = self.expression_to_string(expr.left)
            right = self.expression_to_string(expr.right)
            op = expr.op
            return f"{left} {op} {right}"
        else:
            return str(expr)

def format_output(structs_data, output_file):
    """Format and write the output as JSON."""
    # Deduplicate structs by name, keeping the one with the most members
    struct_dict = {}
    
    for struct in structs_data:
        struct_name = struct['name']
        members = struct['members']
        
        # If we haven't seen this struct name, or if this one has more members, keep it
        if struct_name not in struct_dict or len(members) > len(struct_dict[struct_name]['members']):
            struct_dict[struct_name] = struct
    
    # Convert to JSON format
    json_data = []
    
    # Sort structs by name for consistent output
    sorted_structs = sorted(struct_dict.values(), key=lambda x: x['name'])
    
    for struct in sorted_structs:
        struct_name = struct['name']
        members = struct['members']
        
        # Create parameters dictionary with member names as keys and null as values
        parameters = {}
        for member in members:
            parameters[member['name']] = None
        
        struct_entry = {
            "name": struct_name,
            "parameters": parameters
        }
        
        json_data.append(struct_entry)
    
    # Write JSON file
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, indent=2, ensure_ascii=False)

def scan_directory(directory):
    """Scan directory for C header and source files."""
    c_files = []
    for root, dirs, files in os.walk(directory):
        # Skip sil subdirectories to avoid scanning the script's own directory
        # This includes build_main_sil and other sil subdirectories
        if 'sil' in root and root != str(directory):
            continue
        
        for file in files:
            if file.endswith(('.h', '.c')):
                filepath = os.path.join(root, file)
                c_files.append(filepath)
    
    return c_files


def main():
    """Main function."""
    # Get the script directory (dev/sil/build_main_sil)
    script_dir = Path(__file__).parent  # dev/sil/build_main_sil
    build_dir = script_dir  # We're already in the build directory
    sil_dir = script_dir.parent  # dev/sil
    dev_dir = sil_dir.parent  # dev
    root_dir = dev_dir.parent  # VCU root
    inc_dir = root_dir / 'inc'
    json_files_dir = script_dir / 'json_files'
    output_file = json_files_dir / 'struct_members_output.json'
    
    all_structs = []
    
    # Scan dev directory
    if dev_dir.exists():
        print(f"Scanning {dev_dir}...")
        dev_files = scan_directory(dev_dir)
        for filepath in dev_files:
            try:
                structs = find_struct_definitions(filepath)
                all_structs.extend(structs)
            except Exception as e:
                print(f"Error processing {filepath}: {e}")
    
    # Scan inc directory
    if inc_dir.exists():
        print(f"Scanning {inc_dir}...")
        inc_files = scan_directory(inc_dir)
        for filepath in inc_files:
            try:
                structs = find_struct_definitions(filepath)
                all_structs.extend(structs)
            except Exception as e:
                print(f"Error processing {filepath}: {e}")
    
    # Write output
    print(f"Found {len(all_structs)} struct definitions")
    print(f"Writing output to {output_file}...")
    format_output(all_structs, output_file)
    print("Done!")

if __name__ == '__main__':
    main()
