#!/usr/bin/env python3
"""
PiTrac Python Message Generator
Generates Python message classes from JSON schemas for Flask application
"""

import json
import os
import sys
from typing import Dict, Any, List
from jinja2 import Template

class PythonMessageGenerator:
    def __init__(self):
        # Type mappings for Python
        self.python_types = {
            'int8': 'int',
            'int16': 'int', 
            'int32': 'int',
            'int64': 'int',
            'uint8': 'int',
            'uint16': 'int',
            'uint32': 'int',
            'uint64': 'int',
            'float32': 'float',
            'float64': 'float',
            'string': 'str',
            'binary': 'bytes',
            'bool': 'bool'
        }

    def _get_python_type(self, field_def: Dict[str, Any]) -> str:
        """Convert field definition to Python type annotation"""
        field_type = field_def['type']
        
        if field_type == 'array':
            element_type = self._get_element_python_type(field_def.get('element_type', 'int'))
            return f"List[{element_type}]"
        elif field_type == 'map':
            key_type = self._get_element_python_type(field_def.get('key_type', 'str'))
            value_type = self._get_element_python_type(field_def.get('value_type', 'str'))
            return f"Dict[{key_type}, {value_type}]"
        elif field_type in self.python_types:
            return self.python_types[field_type]
        else:
            # Custom type
            return field_type

    def _get_element_python_type(self, type_name: str) -> str:
        """Get Python type for array/map elements"""
        return self.python_types.get(type_name, type_name)

    def _get_default_value(self, field_def: Dict[str, Any]) -> str:
        """Get default value for Python field"""
        field_type = field_def['type']
        
        if field_type == 'array':
            return "field(default_factory=list)"
        elif field_type == 'map':
            return "field(default_factory=dict)"
        elif field_type in ['int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64']:
            return "0"
        elif field_type in ['float32', 'float64']:
            return "0.0"
        elif field_type == 'string':
            return '""'
        elif field_type == 'binary':
            return "b''"
        elif field_type == 'bool':
            return "False"
        else:
            return "None"

    def generate_enum_class(self, enum_name: str, enum_def: Dict[str, Any]) -> str:
        """Generate Python enum class"""
        template = Template('''
class {{ enum_name }}:
    """{{ description }}"""
    {% for value_name, value in values.items() %}
    {{ value_name }} = {{ value }}
    {% endfor %}
    
    @classmethod
    def get_name(cls, value: int) -> str:
        """Get enum name from value"""
        mapping = {
            {% for value_name, value in values.items() %}
            {{ value }}: "{{ value_name }}",
            {% endfor %}
        }
        return mapping.get(value, f"Unknown({value})")
''')
        
        return template.render(
            enum_name=enum_name,
            description=enum_def.get('description', ''),
            values=enum_def['values']
        )

    def generate_message_class(self, schema: Dict[str, Any]) -> str:
        """Generate Python message class from schema"""
    def generate_message_class(self, schema: Dict[str, Any]) -> str:
        """Generate Python message class from schema"""
        template = Template('''from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
import msgpack
import json
from datetime import datetime
try:
    from ..message_interface import MessageBase
    from ..message_types import MessageType
except ImportError:
    from ..message_interface import MessageBase, MessageType
{% if has_enums %}

{% for enum_name, enum_def in enums.items() %}
{{ generate_enum_class(enum_name, enum_def) }}
{% endfor %}
{% endif %}

@dataclass
class {{ message_name }}(MessageBase):
    """{{ description }}"""
    
    # Fields
    {% for field_name, field_def in fields.items() %}
    {{ field_name }}: {{ get_python_type(field_def) }} = {{ get_default_value(field_def) }}  # {{ field_def.description }}
    {% endfor %}
    
    def __post_init__(self):
        """Initialize parent class after dataclass initialization"""
        super().__init__()
    
    def get_message_type(self) -> MessageType:
        """Get the message type for this message"""
        return MessageType.{{ message_type }}
    
    def _get_fields_data(self) -> List[Any]:
        """Get field values as list for serialization (matching C++ field order)"""
        return [
            {% for field_name, field_def in fields.items() %}
            self.{{ field_name }},
            {% endfor %}
        ]
    
    def _set_fields_data(self, fields_data: List[Any]) -> None:
        """Set field values from list during deserialization"""
        if len(fields_data) != {{ fields|length }}:
            raise ValueError(f"Expected {{ fields|length }} fields, got {len(fields_data)}")
        
        {% for field_name, field_def in fields.items() %}
        self.{{ field_name }} = fields_data[{{ loop.index0 }}]
        {% endfor %}
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> '{{ message_name }}':
        """Create instance from dictionary"""
        kwargs = {}
        {% for field_name, field_def in fields.items() %}
        if '{{ field_name }}' in data:
            {% if field_def.type == 'binary' %}
            # Handle binary data
            if isinstance(data['{{ field_name }}'], str):
                kwargs['{{ field_name }}'] = data['{{ field_name }}'].encode('utf-8')
            else:
                kwargs['{{ field_name }}'] = data['{{ field_name }}']
            {% else %}
            kwargs['{{ field_name }}'] = data['{{ field_name }}']
            {% endif %}
        {% endfor %}
        return cls(**kwargs)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        result = {}
        {% for field_name, field_def in fields.items() %}
        {% if field_def.type == 'binary' %}
        # Handle binary data
        if isinstance(self.{{ field_name }}, bytes):
            import base64
            result['{{ field_name }}'] = base64.b64encode(self.{{ field_name }}).decode('utf-8')
        else:
            result['{{ field_name }}'] = self.{{ field_name }}
        {% else %}
        result['{{ field_name }}'] = self.{{ field_name }}
        {% endif %}
        {% endfor %}
        return result
    
    # Field accessors (mirroring C++ style)
    {% for field_name, field_def in fields.items() %}
    def get_{{ field_name }}(self) -> {{ get_python_type(field_def) }}:
        """Get {{ field_name }}"""
        return self.{{ field_name }}
    
    def set_{{ field_name }}(self, value: {{ get_python_type(field_def) }}) -> None:
        """Set {{ field_name }}"""
        self.{{ field_name }} = value
    
    {% endfor %}
    
    @classmethod
    def from_msgpack(cls, data: bytes) -> '{{ message_name }}':
        """Deserialize from msgpack"""
        instance = cls()
        instance.deserialize(data)
        return instance
    
    def to_msgpack(self) -> bytes:
        """Serialize to msgpack"""
        return self.serialize()
    
    def __str__(self) -> str:
        return f"{{ message_name }}({', '.join(f'{k}={v}' for k, v in self.to_dict().items())})"
''')

        return template.render(
            message_name=schema['message_name'],
            description=schema.get('description', ''),
            message_type=schema.get('message_type', schema['message_name']).replace('Msg', ''),
            fields=schema['fields'],
            enums=schema.get('enums', {}),
            has_enums=bool(schema.get('enums')),
            get_python_type=self._get_python_type,
            get_default_value=self._get_default_value,
            generate_enum_class=self.generate_enum_class
        )

    def process_schema_file(self, schema_path: str, output_dir: str):
        """Process a single schema file"""
        print(f"Processing schema: {schema_path}")
        
        with open(schema_path, 'r') as f:
            schema = json.load(f)
        
        # Generate Python class
        python_code = self.generate_message_class(schema)
        
        # Write Python file
        message_name = schema['message_name']
        python_file = os.path.join(output_dir, f"{message_name}.py")
        
        with open(python_file, 'w') as f:
            f.write(python_code)
        
        print(f"Generated Python class: {python_file}")

    def generate_init_file(self, output_dir: str, message_files: List[str]):
        """Generate __init__.py file for the package"""
        init_content = '''"""
PiTrac Message Classes
Auto-generated Python message classes from JSON schemas
"""

'''
        
        # Add imports for all message classes
        for message_file in sorted(message_files):
            class_name = os.path.splitext(message_file)[0]
            init_content += f"from .{class_name} import {class_name}\n"
        
        init_content += "\n__all__ = [\n"
        for message_file in sorted(message_files):
            class_name = os.path.splitext(message_file)[0]
            init_content += f'    "{class_name}",\n'
        init_content += "]\n"
        
        init_file = os.path.join(output_dir, "__init__.py")
        with open(init_file, 'w') as f:
            f.write(init_content)
        
        print(f"Generated package init file: {init_file}")

    def process_schemas_directory(self, schemas_dir: str, output_base_dir: str):
        """Process all schema directories"""
        for root, dirs, files in os.walk(schemas_dir):
            # Skip if no JSON files
            json_files = [f for f in files if f.endswith('.json')]
            if not json_files:
                continue
            
            # Determine relative path from schemas_dir
            rel_path = os.path.relpath(root, schemas_dir)
            if rel_path == '.':
                # Root schemas directory
                output_dir = output_base_dir
            else:
                # Subdirectory (Common, External, etc.)
                output_dir = os.path.join(output_base_dir, rel_path.lower())
            
            # Create output directory
            os.makedirs(output_dir, exist_ok=True)
            
            # Process each schema file
            generated_files = []
            for json_file in json_files:
                schema_path = os.path.join(root, json_file)
                self.process_schema_file(schema_path, output_dir)
                
                # Track generated Python file
                message_name = json_file.replace('.json', '.py')
                generated_files.append(message_name)
            
            # Generate __init__.py for this directory
            if generated_files:
                self.generate_init_file(output_dir, generated_files)

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 GeneratePythonMessages.py <schemas_dir> <output_dir>")
        print("Example: python3 GeneratePythonMessages.py src/Infrastructure/Messaging/Messages/Schemas PiTrac-Flask/app/messages")
        sys.exit(1)
    
    schemas_dir = sys.argv[1]
    output_dir = sys.argv[2]
    
    if not os.path.exists(schemas_dir):
        print(f"Error: Schemas directory not found: {schemas_dir}")
        sys.exit(1)
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    generator = PythonMessageGenerator()
    generator.process_schemas_directory(schemas_dir, output_dir)
    
    print("Python message generation complete!")

if __name__ == "__main__":
    main()