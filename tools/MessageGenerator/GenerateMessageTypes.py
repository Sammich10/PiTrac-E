#!/usr/bin/env python3
"""
PiTrac Message Types Generator
Generates message types enumeration from schema directories
Mirrors the C++ MessageTypes.h generation
"""

import json
import os
import sys
from typing import Dict, Any, List

def collect_message_types(schemas_dir: str) -> Dict[str, Dict[str, Any]]:
    """Collect all message types from schema files"""
    message_types = {
        'Common': {},
        'Internal': {}, 
        'External': {}
    }
    
    # Define the ranges for each category
    ranges = {
        'Common': (0, 99),
        'Internal': (100, 199),
        'External': (200, 299)
    }
    
    current_values = {
        'Common': 0,
        'Internal': 100,
        'External': 200
    }
    
    for category in ['Common', 'Internal', 'External']:
        category_dir = os.path.join(schemas_dir, category)
        if not os.path.exists(category_dir):
            continue
            
        for filename in sorted(os.listdir(category_dir)):
            if not filename.endswith('.json'):
                continue
                
            schema_path = os.path.join(category_dir, filename)
            try:
                with open(schema_path, 'r') as f:
                    schema = json.load(f)
                
                message_name = schema.get('message_name', '')
                message_type = schema.get('message_type', message_name.replace('Msg', ''))
                description = schema.get('description', '')
                
                if message_type:
                    message_types[category][message_type] = {
                        'value': current_values[category],
                        'description': description,
                        'file': filename
                    }
                    current_values[category] += 1
                    
            except Exception as e:
                print(f"Warning: Failed to process {schema_path}: {e}")
    
    return message_types

def generate_python_message_types(message_types: Dict[str, Dict[str, Any]]) -> str:
    """Generate Python message types enum"""
    content = '''"""
PiTrac Message Types
Auto-generated message type enumeration from schemas
Mirrors the C++ Message_Type enum
"""

from enum import IntEnum

class MessageType(IntEnum):
    """Message type enumeration matching C++ Message_Type"""
'''
    
    for category in ['Common', 'Internal', 'External']:
        if not message_types[category]:
            continue
            
        # Determine range comment
        if category == 'Common':
            range_comment = "# Common Messages (0-99) - Shared between internal and external systems"
        elif category == 'Internal':
            range_comment = "# Internal Messages (100-199) - Process-to-process communication only"
        else:  # External
            range_comment = "# External Messages (200-299) - Communication with host/Flask app"
        
        content += f"    {range_comment}\n"
        
        for msg_type, info in message_types[category].items():
            content += f"    {msg_type} = {info['value']}\n"
        
        content += "\n"
    
    content += '''    @classmethod
    def get_category(cls, msg_type: 'MessageType') -> str:
        """Get the category (Common/Internal/External) for a message type"""
        if 0 <= msg_type.value <= 99:
            return "Common"
        elif 100 <= msg_type.value <= 199:
            return "Internal"
        elif 200 <= msg_type.value <= 299:
            return "External"
        else:
            return "Unknown"
    
    @classmethod
    def is_external(cls, msg_type: 'MessageType') -> bool:
        """Check if message type is external (for Flask communication)"""
        return 200 <= msg_type.value <= 299
    
    @classmethod
    def is_internal(cls, msg_type: 'MessageType') -> bool:
        """Check if message type is internal (process-to-process only)"""
        return 100 <= msg_type.value <= 199
    
    @classmethod
    def is_common(cls, msg_type: 'MessageType') -> bool:
        """Check if message type is common (shared)"""
        return 0 <= msg_type.value <= 99


# Message type lookup dictionary for convenience
MESSAGE_TYPE_INFO = {
'''
    
    for category in ['Common', 'Internal', 'External']:
        for msg_type, info in message_types[category].items():
            content += f'''    MessageType.{msg_type}: {{
        'category': '{category}',
        'description': '{info["description"]}',
        'file': '{info["file"]}'
    }},
'''
    
    content += "}\n"
    
    return content

def generate_cpp_message_types(message_types: Dict[str, Dict[str, Any]]) -> str:
    """Generate C++ message types header"""
    content = '''#ifndef GS_MESSAGE_TYPES_H
#define GS_MESSAGE_TYPES_H

namespace PiTrac
{
enum class Message_Type
{
'''
    
    for category in ['Common', 'Internal', 'External']:
        if not message_types[category]:
            continue
            
        # Determine range comment
        if category == 'Common':
            range_comment = "    // Common Messages (0-99) - Shared between internal and external systems"
        elif category == 'Internal':
            range_comment = "    // Internal Messages (100-199) - Process-to-process communication only"
        else:  # External
            range_comment = "    // External Messages (200-299) - Communication with host/Flask app"
        
        content += f"{range_comment}\n"
        
        for msg_type, info in message_types[category].items():
            content += f"    {msg_type} = {info['value']},\n"
        
        content += "\n"
    
    content += '''    // Add new message types in the appropriate range above...
};
} // namespace PiTrac

#endif // GS_MESSAGE_TYPES_H
'''
    
    return content

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 GenerateMessageTypes.py <schemas_dir> <output_dir>")
        print("Example: python3 GenerateMessageTypes.py src/Infrastructure/Messaging/Messages/Schemas PiTrac-Flask/app/messages")
        sys.exit(1)
    
    schemas_dir = sys.argv[1]
    output_dir = sys.argv[2]
    
    if not os.path.exists(schemas_dir):
        print(f"Error: Schemas directory not found: {schemas_dir}")
        sys.exit(1)
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Collect message types
    message_types = collect_message_types(schemas_dir)
    
    # Generate Python message types
    python_content = generate_python_message_types(message_types)
    python_file = os.path.join(output_dir, "message_types.py")
    with open(python_file, 'w') as f:
        f.write(python_content)
    print(f"Generated Python message types: {python_file}")
    
    # Generate C++ message types (to keep them in sync)
    cpp_content = generate_cpp_message_types(message_types)
    cpp_file = os.path.join("src/Infrastructure/Messaging/Messages", "MessageTypes.h")
    if os.path.exists(cpp_file):
        with open(cpp_file, 'w') as f:
            f.write(cpp_content)
        print(f"Updated C++ message types: {cpp_file}")
    
    # Print summary
    total_types = sum(len(types) for types in message_types.values())
    print(f"\nGenerated {total_types} message types:")
    for category, types in message_types.items():
        if types:
            print(f"  {category}: {len(types)} types")

if __name__ == "__main__":
    main()