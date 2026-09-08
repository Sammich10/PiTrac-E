#!/usr/bin/env python3
"""
PiTrac Message Generator
Generates C++ message classes from JSON schema that implement MessageInterface
"""

import json
import os
import sys
from typing import Dict, Any, List
from jinja2 import Template

class MessageGenerator:
    def __init__(self):
        # Type mappings for C++
        self.cpp_types = {
            'int8': 'int8_t',
            'int16': 'int16_t', 
            'int32': 'int32_t',
            'int64': 'int64_t',
            'uint8': 'uint8_t',
            'uint16': 'uint16_t',
            'uint32': 'uint32_t',
            'uint64': 'uint64_t',
            'float32': 'float',
            'float64': 'double',
            'string': 'std::string',
            'binary': 'std::vector<uint8_t>',
            'bool': 'bool'
        }

    def _get_cpp_type(self, field_def: Dict[str, Any]) -> str:
        """Convert field definition to C++ type"""
        field_type = field_def['type']
        
        if field_type == 'array':
            element_type = self._get_element_cpp_type(field_def.get('element_type', 'uint8'))
            return f"std::vector<{element_type}>"
        elif field_type == 'map':
            key_type = self._get_element_cpp_type(field_def.get('key_type', 'string'))
            value_type = self._get_element_cpp_type(field_def.get('value_type', 'string'))
            return f"std::map<{key_type}, {value_type}>"
        elif field_type in self.cpp_types:
            return self.cpp_types[field_type]
        else:
            # Custom type
            return field_type

    def _get_element_cpp_type(self, type_name: str) -> str:
        """Get C++ type for array/map elements"""
        return self.cpp_types.get(type_name, type_name)

    def _get_default_value(self, field_def: Dict[str, Any]) -> str:
        """Get default value for field initialization"""
        field_type = field_def['type']
        
        defaults = {
            'int8': '0', 'int16': '0', 'int32': '0', 'int64': '0',
            'uint8': '0', 'uint16': '0', 'uint32': '0', 'uint64': '0',
            'float32': '0.0f', 'float64': '0.0',
            'string': '""', 'bool': 'false',
            'binary': '{}', 'array': '{}', 'map': '{}'
        }
        
        return defaults.get(field_type, '{}')

    def generate_header(self, schema: Dict[str, Any]) -> str:
        """Generate C++ header file"""
        template = Template('''
#pragma once

#include "Infrastructure/Messaging/MessageBase.h"
#include <msgpack.hpp>
#include <vector>
#include <map>
#include <string>

namespace PiTrac
{
/**
 * @brief {{ description }}
 */
class {{ message_name }} : public MessageBase
{
public:
{{ enum_code }}
private:
{%- for field_name, field_def in fields.items() %}
    {{ get_cpp_type(field_def) }} {{ field_name }}_;  ///< {{ field_def.description }}
{%- endfor %}

    void serialize(msgpack::sbuffer& buffer) const override;
    void deserialize(const char* data, size_t size) override;

public:
    // Constructors
    {{ message_name }}() = default;
    
    {{ message_name }}({% for field_name, field_def in fields.items() %}const {{ get_cpp_type(field_def) }}& {{ field_name }}{{ ", " if not loop.last else "" }}{% endfor %})
{%- if fields %}
        : {% for field_name, field_def in fields.items() %}{{ field_name }}_({{ field_name }}){{ ", " if not loop.last else "" }}{% endfor %}
{%- endif %}
    {
        is_valid_ = false;
    }

    virtual ~{{ message_name }}() = default;

    // MessageInterface implementation
    Message_Type getMessageType() const override {
        return Message_Type::{{ message_type }};
    }

    std::unique_ptr<MessageInterface> clone() const override;
    std::string toString() const override;

    // Field accessors
{%- for field_name, field_def in fields.items() %}
    const {{ get_cpp_type(field_def) }}& get{{ field_name|title }}() const { return {{ field_name }}_; }
    void set{{ field_name|title }}(const {{ get_cpp_type(field_def) }}& {{ field_name }}) { {{ field_name }}_ = {{ field_name }}; }
{%- endfor %}

{%- if custom_methods %}
    // Custom methods
{%- for method in custom_methods %}
    {{ method.return_type }} {{ method.name }}({% if method.get('parameters') %}{% for param in method.parameters %}{{ param.type }} {{ param.name }}{{ ", " if not loop.last else "" }}{% endfor %}{% endif %}){% if method.return_type != 'void' %} const{% endif %} {
        {{ method.body }}
    }
{%- endfor %}
{%- endif %}
};

} // namespace PiTrac
        '''.strip())

        # Generate enum code separately
        enum_code = ""
        enums = schema.get('enums', {})
        if enums:
            enum_code = "    // Message-specific enums\n"
            for enum_name, enum_def in enums.items():
                enum_code += f"    /**\n"
                enum_code += f"     * @brief {enum_def.get('description', '')}\n"
                enum_code += f"     */\n"
                enum_code += f"    enum class {enum_name} : int32_t\n"
                enum_code += f"    {{\n"
                for value_name, value in enum_def.get('values', {}).items():
                    enum_code += f"        {value_name} = {value},\n"
                enum_code += f"    }};\n\n"

        return template.render(
            message_name=schema['message_name'],
            description=schema['description'],
            message_type=schema['message_type'],
            fields=schema['fields'],
            enum_code=enum_code,
            custom_methods=schema.get('custom_methods', []),
            get_cpp_type=self._get_cpp_type
        )

    def generate_source(self, schema: Dict[str, Any]) -> str:
        """Generate C++ source file"""
        template = Template('''
#include "{{ message_name }}.h"
#include <sstream>

namespace PiTrac
{

void {{ message_name }}::serialize(msgpack::sbuffer& buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    
    // Pack as array: [type, timestamp, field1, field2, ...]
    packer.pack_array({{ field_count + 2 }});
    
    // Pack common fields (type, timestamp)
    packCommonFields(packer);
    
    // Pack message-specific fields
{%- for field_name, field_def in fields.items() %}
    packer.pack({{ field_name }}_);
{%- endfor %}
}

void {{ message_name }}::deserialize(const char* data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();
    
    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != {{ field_count + 2 }}) {
        throw std::runtime_error("Invalid {{ message_name }} format");
    }
    
    // Unpack common fields (type, timestamp) - validates message type
    int message_type;
    int64_t timestamp_ms;
    obj.via.array.ptr[0].convert(message_type);
    obj.via.array.ptr[1].convert(timestamp_ms);
    
    if (static_cast<Message_Type>(message_type) != getMessageType()) {
        throw std::runtime_error(incorrectMessageTypeString(static_cast<Message_Type>(message_type)));
    }
    
    // Restore timestamp
    timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
    
    // Unpack message-specific fields
{%- for field_name, field_def in fields.items() %}
    obj.via.array.ptr[{{ loop.index + 1 }}].convert({{ field_name }}_);
{%- endfor %}
    is_valid_ = true;
}

std::unique_ptr<MessageInterface> {{ message_name }}::clone() const
{
    auto cloned = std::make_unique<{{ message_name }}>();
    cloned->timestamp_ = timestamp_;
{%- for field_name, field_def in fields.items() %}
    cloned->{{ field_name }}_ = {{ field_name }}_;
{%- endfor %}
    return cloned;
}

std::string {{ message_name }}::toString() const
{
    std::ostringstream oss;
    oss << MessageBase::toString()
{%- for field_name, field_def in fields.items() %}
{%- if field_def.type == 'string' %}
        << ", {{ field_name }}: " << {{ field_name }}_
{%- elif field_def.type in ['int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64', 'float32', 'float64'] %}
        << ", {{ field_name }}: " << {{ field_name }}_
{%- elif field_def.type == 'bool' %}
        << ", {{ field_name }}: " << ({{ field_name }}_ ? "true" : "false")
{%- elif field_def.type == 'binary' %}
        << ", {{ field_name }}: [" << {{ field_name }}_.size() << " bytes]"
{%- elif field_def.type == 'array' %}
        << ", {{ field_name }}: [" << {{ field_name }}_.size() << " elements]"
{%- elif field_def.type == 'map' %}
        << ", {{ field_name }}: [" << {{ field_name }}_.size() << " entries]"
{%- endif %}
{%- endfor %};
    return oss.str();
}

} // namespace PiTrac
        '''.strip())

        return template.render(
            message_name=schema['message_name'],
            fields=schema['fields'],
            field_count=len(schema['fields'])
        )

    def generate_from_file(self, schema_file: str, output_dir: str):
        """Generate C++ files from JSON schema file"""
        with open(schema_file, 'r') as f:
            schema = json.load(f)
        
        message_name = schema['message_name']
        
        # Generate header
        header_content = self.generate_header(schema)
        header_path = os.path.join(output_dir, f"{message_name}.h")
        
        # Generate source
        source_content = self.generate_source(schema)
        source_path = os.path.join(output_dir, f"{message_name}.cpp")
        
        # Write files
        os.makedirs(output_dir, exist_ok=True)
        
        with open(header_path, 'w') as f:
            f.write(header_content)
        
        with open(source_path, 'w') as f:
            f.write(source_content)
        
        print(f"Generated {message_name}:")
        print(f"  Header: {header_path}")
        print(f"  Source: {source_path}")

    def generate_message_types(self, schema_files: List[str], output_dir: str):
        """Generate MessageTypes.h with enums for all messages organized by category"""
        common_messages = []
        internal_messages = []
        external_messages = []
        
        # Categorize messages based on their schema path
        for schema_file in schema_files:
            with open(schema_file, 'r') as f:
                schema = json.load(f)
                message_type = schema['message_type']
                
                # Categorize based on schema file path
                if 'Common' in schema_file:
                    common_messages.append(message_type)
                elif 'Internal' in schema_file:
                    internal_messages.append(message_type)
                elif 'External' in schema_file:
                    external_messages.append(message_type)
                else:
                    # Default to external if not clearly categorized
                    external_messages.append(message_type)
        
        # Sort each category
        common_messages.sort()
        internal_messages.sort()
        external_messages.sort()
        
        template = Template('''
#ifndef GS_MESSAGE_TYPES_H
#define GS_MESSAGE_TYPES_H

namespace PiTrac
{
enum class Message_Type
{
    // Common Messages (0-99) - Shared between internal and external systems
{%- for msg_type in common_messages %}
    {{ msg_type }} = {{ loop.index0 }},
{%- endfor %}
    
    // Internal Messages (100-199) - Process-to-process communication only
{%- for msg_type in internal_messages %}
    {{ msg_type }} = {{ loop.index0 + 100 }},
{%- endfor %}
    
    // External Messages (200-299) - Communication with host/Flask app
{%- for msg_type in external_messages %}
    {{ msg_type }} = {{ loop.index0 + 200 }},
{%- endfor %}
    
    // Add new message types in the appropriate range above...
};
} // namespace PiTrac

#endif // GS_MESSAGE_TYPES_H
        '''.strip())
        
        content = template.render(
            common_messages=common_messages,
            internal_messages=internal_messages,
            external_messages=external_messages
        )
        
        output_path = os.path.join(output_dir, "MessageTypes.h")
        os.makedirs(output_dir, exist_ok=True)
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        print(f"Generated unified MessageTypes.h: {output_path}")
        print(f"  Common messages ({len(common_messages)}): {', '.join(common_messages)}")
        print(f"  Internal messages ({len(internal_messages)}): {', '.join(internal_messages)}")
        print(f"  External messages ({len(external_messages)}): {', '.join(external_messages)}")

    def generate_message_factory(self, schema_files: List[str], output_dir: str):
        """Generate unified MessageFactory.h with registrations for all messages"""
        common_messages = []
        internal_messages = []
        external_messages = []
        
        # Categorize messages based on their schema path
        for schema_file in schema_files:
            with open(schema_file, 'r') as f:
                schema = json.load(f)
                message_info = {
                    'message_name': schema['message_name'],
                    'message_type': schema['message_type'],
                    'schema_file': schema_file
                }
                
                # Categorize based on schema file path
                if 'Common' in schema_file:
                    common_messages.append(message_info)
                elif 'Internal' in schema_file:
                    internal_messages.append(message_info)
                elif 'External' in schema_file:
                    external_messages.append(message_info)
                else:
                    external_messages.append(message_info)
        
        # Sort each category by message name
        common_messages.sort(key=lambda x: x['message_name'])
        internal_messages.sort(key=lambda x: x['message_name'])
        external_messages.sort(key=lambda x: x['message_name'])
        
        # Combine all for template
        all_messages = common_messages + internal_messages + external_messages
        
        template = Template('''
#ifndef MESSAGE_FACTORY_H
#define MESSAGE_FACTORY_H

#include "Infrastructure/Messaging/MessageInterface.h"
#include "Infrastructure/Messaging/Messages/MessageTypes.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <zmq.h>
#include <msgpack.hpp>

// Common message includes
{%- for msg in common_messages %}
#include "Infrastructure/Messaging/Messages/{{ msg.message_name }}.h"
{%- endfor %}

// Internal message includes  
{%- for msg in internal_messages %}
#include "Infrastructure/Messaging/Messages/{{ msg.message_name }}.h"
{%- endfor %}

// External message includes
{%- for msg in external_messages %}
#include "Infrastructure/Messaging/Messages/{{ msg.message_name }}.h"
{%- endfor %}

#include <memory>
#include <unordered_map>
#include <functional>

namespace PiTrac
{
class MessageFactory
{
  public:
    MessageFactory();

    template<typename MessageType>
    void registerMessage(const Message_Type &type)
    {
        creators_[type] = []() {
                              return std::make_unique<MessageType>();
                          };
    }

    std::unique_ptr<MessageInterface> createFromZmqMessage(zmq_msg_t &msg);

  private:
    std::unordered_map<Message_Type,
                       std::function<std::unique_ptr<MessageInterface>()> > creators_;
    std::shared_ptr<GSLogger> logger_ = GSLogger::getInstance();
};

// Constructor implementation
inline MessageFactory::MessageFactory()
{
    // Register common messages (0-99)
{%- for msg in common_messages %}
    registerMessage<{{ msg.message_name }}>(Message_Type::{{ msg.message_type }});
{%- endfor %}

    // Register internal messages (100-199)
{%- for msg in internal_messages %}
    registerMessage<{{ msg.message_name }}>(Message_Type::{{ msg.message_type }});
{%- endfor %}

    // Register external messages (200-299)
{%- for msg in external_messages %}
    registerMessage<{{ msg.message_name }}>(Message_Type::{{ msg.message_type }});
{%- endfor %}
}

// Factory method implementation
inline std::unique_ptr<MessageInterface> MessageFactory::createFromZmqMessage(zmq_msg_t &msg)
{
    // Extract message type from the beginning of the message
    const char *data = static_cast<const char *>(zmq_msg_data(&msg));
    size_t size = zmq_msg_size(&msg);

    if (size == 0)
    {
        logger_->error("Empty message received");
        return nullptr;
    }

    if (data == nullptr)
    {
        logger_->error("Message data is null");
        return nullptr;
    }

    // Unpack just the first element to get message type
    msgpack::object_handle oh;
    try {
        oh = msgpack::unpack(data, size);
    } catch (const std::exception &e) {
        logger_->error("Failed to unpack MessagePack data: " + std::string(e.what()));
        return nullptr;
    } catch (...) {
        logger_->error("Failed to unpack MessagePack data: unknown exception");
        return nullptr;
    }
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY || obj.via.array.size == 0)
    {
        logger_->error("Invalid message format");
        return nullptr;
    }

    int message_type;
    try {
        obj.via.array.ptr[0].convert(message_type);
    } catch (const std::exception &e) {
        logger_->error("Failed to extract message type: " + std::string(e.what()));
        return nullptr;
    }

    auto it = creators_.find(static_cast<Message_Type>(message_type));
    if (it == creators_.end())
    {
        logger_->error("Unknown message type: " + std::to_string(static_cast<int>(message_type)));
        return nullptr;
    }

    auto message = it->second();
    try {
        message->fromZmqMessage(msg);
    } catch (const std::exception &e) {
        logger_->error("Failed to deserialize message for type %d: %s", static_cast<int>(message_type), e.what());
        return nullptr;
    }
    
    return message;
}

} // namespace PiTrac

#endif // MESSAGE_FACTORY_H
        '''.strip())
        
        content = template.render(
            common_messages=common_messages,
            internal_messages=internal_messages, 
            external_messages=external_messages
        )
        
        output_path = os.path.join(output_dir, "MessageFactory.h")
        os.makedirs(output_dir, exist_ok=True)
        
        with open(output_path, 'w') as f:
            f.write(content)
        
        print(f"Generated unified MessageFactory.h: {output_path}")
        print(f"  Registered {len(all_messages)} message types:")
        print(f"    Common: {len(common_messages)}")
        print(f"    Internal: {len(internal_messages)}")
        print(f"    External: {len(external_messages)}")

def main():
    if len(sys.argv) < 3:
        print("Usage: python message_generator.py <schema_file> <output_directory>")
        print("   or: python message_generator.py <schema_directory> <output_directory>")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_dir = sys.argv[2]
    
    generator = MessageGenerator()
    schema_files = []
    
    if os.path.isfile(input_path):
        # Single file
        generator.generate_from_file(input_path, output_dir)
        schema_files = [input_path]
    elif os.path.isdir(input_path):
        # Directory of schema files - collect all JSON files recursively
        for root, dirs, files in os.walk(input_path):
            for filename in files:
                if filename.endswith('.json'):
                    schema_file = os.path.join(root, filename)
                    generator.generate_from_file(schema_file, output_dir)
                    schema_files.append(schema_file)
    else:
        print(f"Error: {input_path} is not a file or directory")
        sys.exit(1)
    
    # Generate MessageTypes.h and MessageFactory.h if we processed multiple files
    if len(schema_files) > 0:
        print(f"\nGenerating MessageTypes and MessageFactory from {len(schema_files)} schemas...")
        generator.generate_message_types(schema_files, output_dir)
        generator.generate_message_factory(schema_files, output_dir)

if __name__ == "__main__":
    main()